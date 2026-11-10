"""
Tests for mysql_bridge.py (standard library unittest).

    cd other/sql_bridge && python3 -m unittest discover -s tests -v

Unit tests cover the classifier, packet codec, response scanner, auth and
config handling.  Integration tests run the bridge between a test client and
two fake MariaDB servers (tests/fake_mysql.py) and check routing, framing,
failover, TLS and the security properties end to end.  Interop tests with
PyMySQL / MariaDB Connector/Python / mysql-connector-python / mysql-mimic run
when those packages are installed (they are optional).
"""

import asyncio
import configparser
import os
import random
import shutil
import ssl
import struct
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

import mysql_bridge as mb  # noqa: E402
import logging; logging.basicConfig(level=os.environ.get("TEST_LOG", "CRITICAL"))
from fake_mysql import (FakeMySQLServer, MiniClient, ClientError, pkt, lenenc_int,  # noqa: E402
                        lenenc_str, coldef)


# --- helpers ---------------------------------------------------------------------
def make_conf(primary_port, replica_port, primary_opts=None, replica_opts=None, **bridge):
    cfg = configparser.ConfigParser(interpolation=None)
    b = {"listen_host": "127.0.0.1", "listen_port": "0", "max_clients": "64",
         "replica_pool_size": "4", "route_selects_to_replica": "true",
         "read_your_writes_grace_sec": "0", "stats_interval_sec": "0",
         "replica_acquire_timeout_sec": "0.5", "replica_query_timeout_sec": "5"}
    b.update({k: str(v) for k, v in bridge.items()})
    cfg["bridge"] = b
    p = {"host": "127.0.0.1", "port": str(primary_port)}
    p.update(primary_opts or {})
    cfg["primary"] = p
    r = {"host": "127.0.0.1", "port": str(replica_port), "user": "bridge",
         "password": "bridgepw", "database": "mmotee"}
    r.update(replica_opts or {})
    cfg["replica"] = r
    return mb.build_config(cfg)


class BridgeTestCase(unittest.IsolatedAsyncioTestCase):
    primary_kwargs = {}
    replica_kwargs = {}

    async def asyncSetUp(self):
        mb.STATS.clear()
        self.primary = await FakeMySQLServer("primary", **self.primary_kwargs).start()
        self.replica = await FakeMySQLServer("replica", read_only=True, **self.replica_kwargs).start()
        self.bridges = []
        self.clients = []

    async def asyncTearDown(self):
        for c in self.clients:
            try:
                await asyncio.wait_for(c.close(), 1)
            except Exception:
                pass
        for b in self.bridges:
            await b.stop()
        await self.primary.stop()
        await self.replica.stop()

    async def bridge(self, primary_opts=None, replica_opts=None, **opts):
        conf = make_conf(self.primary.port, self.replica.port, primary_opts, replica_opts, **opts)
        b = mb.Bridge(conf)
        await b.start()
        self.bridges.append(b)
        return b

    async def client(self, bridge, **kw):
        c = MiniClient()
        await asyncio.wait_for(c.connect(bridge.ports[0], **kw), 5)
        self.clients.append(c)
        return c

    async def served_by(self, client, sql):
        rows = await asyncio.wait_for(client.rows(sql), 5)
        return rows[0][0]


# --- unit tests --------------------------------------------------------------------
class TestClassifier(unittest.TestCase):
    def kind(self, sql):
        return mb.classify_query(sql.encode())[0]

    def test_reads(self):
        for q in ["SELECT * FROM tw_accounts WHERE ID = '5';",
                  "  select ID from tw_accounts_data where Nick = 'x;y'",
                  "(SELECT 1) UNION (SELECT 2)",
                  "/* comment */ SELECT 1",
                  "-- c\nSELECT 1",
                  "# c\nSELECT 1",
                  "SELECT 'a''b', \"q\\\"\" FROM `tw_accounts`",
                  "SELECT * FROM t WHERE name = 'for update'",
                  "SELECT * FROM t WHERE x = '@'",
                  "SHOW TABLES", "SHOW FULL COLUMNS FROM t", "SHOW CREATE TABLE t",
                  "SHOW TABLE STATUS", "SHOW INDEX FROM t", "DESCRIBE t", "DESC t",
                  "EXPLAIN SELECT * FROM t",
                  "WITH x AS (SELECT 1) SELECT * FROM x",
                  "SELECT ID FROM tw_accounts ORDER BY ID DESC LIMIT 1;"]:
            self.assertEqual(self.kind(q), mb.K_READ, q)

    def test_reads_that_must_use_primary(self):
        for q in ["SELECT * FROM t FOR UPDATE",
                  "SELECT * FROM t WHERE x IN (" + "1," * 300 + "1) FOR UPDATE",
                  "SELECT * FROM t LOCK IN SHARE MODE",
                  "SELECT * FROM t FOR SHARE",
                  "SELECT LAST_INSERT_ID()", "SELECT last_insert_id ()",
                  "SELECT FOUND_ROWS()", "SELECT SQL_CALC_FOUND_ROWS * FROM t",
                  "SELECT ROW_COUNT()", "SELECT GET_LOCK('a', 1)", "SELECT RELEASE_LOCK('a')",
                  "SELECT IS_USED_LOCK('a')", "SELECT CONNECTION_ID()",
                  "SELECT NEXT VALUE FOR seq", "SELECT NEXTVAL(seq)", "SELECT LASTVAL(seq)",
                  "SELECT @x", "SELECT @@session.sql_mode", "SELECT @a := 1",
                  "SELECT 1 INTO @x", "SELECT * INTO OUTFILE '/tmp/x' FROM t",
                  "SELECT LOAD_FILE('/etc/passwd')", "SELECT DATABASE()", "SELECT USER()",
                  "SELECT CURRENT_USER", "SELECT * FROM information_schema.PROCESSLIST",
                  "SELECT * FROM performance_schema.threads",
                  "SHOW WARNINGS", "SHOW ERRORS", "SHOW COUNT(*) WARNINGS", "SHOW VARIABLES",
                  "SHOW SESSION STATUS", "SHOW GRANTS", "SHOW PROCESSLIST",
                  "SHOW FULL PROCESSLIST", "SHOW SLAVE STATUS", "SHOW MASTER STATUS",
                  "SHOW BINLOG EVENTS", "SHOW DATABASES", "SHOW CREATE USER x",
                  "SHOW PROFILES", "SHOW ENGINE INNODB STATUS", "SHOW OPEN TABLES",
                  "EXPLAIN UPDATE t SET a = 1", "EXPLAIN EXTENDED SELECT 1",
                  "EXPLAIN FOR CONNECTION 5", "VALUES (1)", "TABLE t"]:
            self.assertEqual(self.kind(q), mb.K_PRIMARY_READ, q)

    def test_writes_and_unknown(self):
        for q in ["INSERT INTO t VALUES (1)", "UPDATE t SET a = 1", "DELETE FROM t",
                  "REPLACE INTO t VALUES (1)", "CREATE TABLE x (a int)", "CALL p()",
                  "DO GET_LOCK('a', 1)", "TRUNCATE t", "WITH x AS (SELECT 1) UPDATE t SET a = 1",
                  "LOAD DATA LOCAL INFILE 'f' INTO TABLE t", "FLUSH TABLES"]:
            self.assertEqual(self.kind(q), mb.K_WRITE, q)
        # multi-statements, executable comments and unterminated literals are
        # never sent to the replica
        for q in ["SELECT 1; DELETE FROM tw_accounts", "SELECT 1;SELECT 2",
                  "SELECT 1 /*! ; DELETE FROM t */", "/*!40101 SELECT 1 */",
                  "/*M!100100 SELECT 1 */", "SELECT 'unterminated", "SELECT `x",
                  "SELECT 1 /* unterminated", ""]:
            self.assertEqual(self.kind(q), mb.K_UNKNOWN, q)

    def test_session_and_tx(self):
        self.assertEqual(self.kind("SET NAMES utf8mb4"), mb.K_SET)
        self.assertEqual(self.kind("set autocommit=0"), mb.K_SET)
        self.assertEqual(self.kind("USE mmotee"), mb.K_USE)
        for q in ["BEGIN", "START TRANSACTION", "COMMIT", "ROLLBACK", "SAVEPOINT a",
                  "LOCK TABLES t READ", "UNLOCK TABLES", "XA START 'x'"]:
            self.assertEqual(self.kind(q), mb.K_TX, q)

    def test_trailing_semicolons_and_whitespace(self):
        self.assertEqual(self.kind("SELECT 1 ;  \n ; "), mb.K_READ)
        self.assertEqual(self.kind("SELECT ';' FROM t;"), mb.K_READ)

    def test_code_is_literal_free(self):
        _, code, _ = mb.classify_query(
            b"SELECT * FROM tw_accounts WHERE Password = 'secret-hash' AND PasswordSalt = \"salt\"")
        self.assertNotIn(b"secret", code)
        self.assertNotIn(b"salt\"", code)
        self.assertNotIn("secret", mb.describe_sql(code))

    def test_log_sql_masks_passwords(self):
        self.assertNotIn("hunter2", mb.log_sql("CREATE USER x IDENTIFIED BY 'hunter2'"))
        self.assertNotIn("hunter2", mb.log_sql("SET PASSWORD = PASSWORD('hunter2')"))

    def test_write_tables(self):
        wt = lambda q: mb.write_tables(mb.classify_query(q.encode())[2])
        self.assertEqual(wt("INSERT INTO tw_accounts (a) VALUES (1)"), (b"tw_accounts",))
        self.assertEqual(wt("INSERT IGNORE INTO `mmotee`.`tw_x` VALUES (1)"), (b"tw_x",))
        self.assertEqual(wt("REPLACE tw_y VALUES (1)"), (b"tw_y",))
        self.assertEqual(wt("UPDATE tw_accounts_data SET Level = 2 WHERE ID = 1"),
                         (b"tw_accounts_data",))
        self.assertEqual(wt("DELETE FROM tw_items WHERE ID = 1"), (b"tw_items",))
        self.assertIsNone(wt("UPDATE a JOIN b ON a.x = b.x SET a.y = 1"))
        self.assertIsNone(wt("DELETE a FROM a JOIN b"))
        self.assertIsNone(wt("DELETE FROM a USING a JOIN b"))
        self.assertIsNone(wt("CREATE TABLE z (a int)"))


class TestWriteTracker(unittest.TestCase):
    def test_scopes(self):
        low = lambda q: mb.classify_query(q.encode())[2]
        t = mb.WriteTracker(2.0, "table")
        t.note((b"tw_accounts",), 100.0)
        self.assertTrue(t.blocks(low("SELECT ID FROM tw_accounts WHERE x=1"), 101.0))
        self.assertTrue(t.blocks(low("SELECT * FROM `mmotee`.`tw_accounts`"), 101.0))
        self.assertFalse(t.blocks(low("SELECT * FROM tw_accounts_data"), 101.0))
        self.assertFalse(t.blocks(low("SELECT ID FROM tw_accounts"), 102.5))
        t.note(None, 200.0)
        self.assertTrue(t.blocks(low("SELECT 1"), 201.0))
        g = mb.WriteTracker(2.0, "global")
        g.note((b"a",), 10.0)
        self.assertTrue(g.blocks(low("SELECT * FROM b"), 11.0))
        c = mb.WriteTracker(2.0, "connection")
        c.note(None, 10.0)
        self.assertFalse(c.blocks(low("SELECT * FROM b"), 10.5))
        z = mb.WriteTracker(0.0, "table")
        z.note(None, 10.0)
        self.assertFalse(z.blocks(low("SELECT 1"), 10.0))


class TestPackets(unittest.IsolatedAsyncioTestCase):
    async def test_roundtrip_small_and_huge(self):
        for size in (0, 1, 1000, 0xFFFFFE, 0xFFFFFF, 0xFFFFFF + 5, 2 * 0xFFFFFF):
            payload = os.urandom(16) * (size // 16) + os.urandom(size % 16)
            r = asyncio.StreamReader(limit=1 << 26)
            r.feed_data(mb.encode_packet(7, payload))
            r.feed_eof()
            seq, got = await mb.read_packet(r, limit=1 << 27)
            self.assertEqual(seq, 7)
            self.assertEqual(got, payload)

    async def test_packet_limit(self):
        r = asyncio.StreamReader(limit=1 << 26)
        r.feed_data(mb.encode_packet(0, b"x" * (0xFFFFFF + 10)))
        r.feed_eof()
        with self.assertRaises(mb.ProtocolError):
            await mb.read_packet(r, limit=0xFFFFFF)

    def test_err_packet(self):
        data = mb.err_packet(3, 1040, "Too many", "08004")
        self.assertEqual(data[3], 3)
        self.assertEqual(mb.parse_err(data[4:]), (1040, "Too many"))
        self.assertEqual(mb.parse_err(mb.err_packet(0, 1130, "nope", None)[4:]), (1130, "nope"))


class TestAuth(unittest.TestCase):
    def test_tokens(self):
        salt = b"abcdefghijklmnopqrst"
        self.assertEqual(mb.native_password_token("", salt), b"")
        self.assertEqual(mb.caching_sha2_token("", salt), b"")
        self.assertEqual(len(mb.native_password_token("pw", salt)), 20)
        self.assertEqual(len(mb.caching_sha2_token("pw", salt)), 32)
        try:
            from pymysql import _auth
        except ImportError:
            return
        self.assertEqual(mb.native_password_token("pw", salt), _auth.scramble_native_password(b"pw", salt))
        self.assertEqual(mb.caching_sha2_token("pw", salt), _auth.scramble_caching_sha2(b"pw", salt))

    def test_handshake_response_db(self):
        caps = (mb.CLIENT_PROTOCOL_41 | mb.CLIENT_SECURE_CONNECTION | mb.CLIENT_CONNECT_WITH_DB
                | mb.CLIENT_PLUGIN_AUTH)
        p = struct.pack("<IIB", caps, 1 << 24, 45) + b"\0" * 23 + b"game\0" + bytes([20]) + b"t" * 20 \
            + b"mmotee\0mysql_native_password\0"
        self.assertEqual(mb.parse_handshake_db(p), "mmotee")
        p2 = struct.pack("<IIB", caps | mb.CLIENT_PLUGIN_AUTH_LENENC_DATA, 1 << 24, 45) + b"\0" * 23 \
            + b"game\0" + bytes([20]) + b"t" * 20 + b"db2\0"
        self.assertEqual(mb.parse_handshake_db(p2), "db2")
        p3 = struct.pack("<IIB", caps & ~mb.CLIENT_CONNECT_WITH_DB, 1 << 24, 45) + b"\0" * 23 + b"g\0\0"
        self.assertIsNone(mb.parse_handshake_db(p3))
        self.assertIsNone(mb.parse_handshake_db(b"garbage"))


# --- response scanner ------------------------------------------------------------
def _ok(header=0x00, status=2, info=b"", track=False):
    p = bytes([header]) + b"\x00\x00" + struct.pack("<HH", status, 0)
    if info:
        p += lenenc_str(info) if track else info
    return p


def _eof(status=2):
    return b"\xfe" + struct.pack("<HH", 0, status)


def _stream(payloads):
    out, seq = bytearray(), 1
    for p in payloads:
        data, seq = pkt(seq, p)
        out += data
    return bytes(out)


def _resultset(ncols, rows, deof, status=2, info=b""):
    out = [lenenc_int(ncols)] + [coldef("c%d" % i) for i in range(ncols)]
    if not deof:
        out.append(_eof())
    for r in rows:
        out.append(b"".join(lenenc_str(v) for v in r))
    out.append(_ok(0xFE, status, info, True) if deof else _eof(status))
    return out


class TestScanner(unittest.TestCase):
    def run_stream(self, stream, kinds, deof, chunking, **kw):
        statuses = []
        done = []
        sc = mb.ResponseScanner(deprecate_eof=deof, on_status=statuses.append, **kw)
        for i, k in enumerate(kinds):
            sc.expect(k, lambda p, i=i: done.append(i))
        pos = 0
        rnd = random.Random(1)
        while pos < len(stream):
            n = chunking if isinstance(chunking, int) else rnd.randint(1, 50)
            sc.feed(stream[pos:pos + n])
            pos += n
        self.assertIsNone(sc.desynced)
        self.assertFalse(sc.busy)
        self.assertEqual(sc.unsolicited, 0)
        return done, statuses, sc

    def test_resultsets_any_chunking(self):
        for deof in (False, True):
            rows = [("a", None, "c" * 300), ("", "x", None), ("\xfe" * 3, "y", "z")]
            payloads = (_resultset(3, rows, deof, info=b"session-track-info" if deof else b"")
                        + [_ok()] + _resultset(1, [], deof))
            stream = _stream(payloads[:len(payloads) - len(_resultset(1, [], deof)) - 1]) \
                + _stream([_ok()]) + _stream(_resultset(1, [], deof))
            for chunking in (1, 2, 3, 7, 4096, "random"):
                done, _, _ = self.run_stream(stream, [mb.R_RESULT] * 3, deof, chunking)
                self.assertEqual(done, [0, 1, 2], (deof, chunking))

    def test_long_deprecate_eof_terminator(self):
        # old bridge missed 0xFE terminators >= 9 bytes (session tracking)
        stream = _stream(_resultset(1, [("v",)], True, info=b"x" * 40))
        done, _, _ = self.run_stream(stream, [mb.R_RESULT], True, 5)
        self.assertEqual(done, [0])

    def test_multi_results_and_status(self):
        for deof in (False, True):
            p = _resultset(1, [("1",)], deof, status=2 | 8) + [_ok(status=3 | 8)] + _resultset(1, [("2",)], deof, status=1)
            done, statuses, _ = self.run_stream(_stream(p), [mb.R_RESULT], deof, 3)
            self.assertEqual(done, [0])
            self.assertEqual(statuses[-1], 1)

    def test_error_mid_rows_and_progress(self):
        progress = b"\xff\xff\xff" + b"\x01\x01" + b"\x00\x00\x00" + b"stage"
        p = [progress, lenenc_int(1), coldef("a"), b"\x011",
             b"\xff" + struct.pack("<H", 1317) + b"#70100interrupted"]
        done, _, sc = self.run_stream(_stream(p), [mb.R_RESULT], True, 4)
        self.assertEqual(done, [])                       # ERR terminates, but is not an OK
        self.assertEqual(sc.first_byte, 0x01)

    def test_err_and_single(self):
        p = [b"\xff\x15\x04#28000denied"]
        sc = mb.ResponseScanner()
        sc.expect(mb.R_RESULT)
        sc.feed(_stream(p))
        self.assertFalse(sc.busy)
        self.assertEqual(sc.first_byte, 0xFF)

    def test_huge_row(self):
        for deof in (False, True):
            big = "x" * (0xFFFFFF + 100)
            stream = _stream(_resultset(1, [(big,), ("small",)], deof))
            done, _, _ = self.run_stream(stream, [mb.R_RESULT], deof, 1 << 20)
            self.assertEqual(done, [0])

    def test_prepare_and_fieldlist(self):
        for deof in (False, True):
            prep = [b"\x00" + struct.pack("<IHHBH", 1, 2, 1, 0, 0), coldef("?")]
            if not deof:
                prep.append(_eof())
            prep += [coldef("a"), coldef("b")]
            if not deof:
                prep.append(_eof())
            fl = [coldef("a"), coldef("b"), _eof() if not deof else _ok(0xFE)]
            stream = _stream(prep) + _stream(fl) + _stream([b"\x00" + struct.pack("<IHHBH", 2, 0, 0, 0, 0)])
            done, _, _ = self.run_stream(stream, [mb.R_PREPARE, mb.R_FIELDLIST, mb.R_PREPARE], deof, 3)
            self.assertEqual(done, [0, 1, 2])

    def test_infile(self):
        hits = []
        sc = mb.ResponseScanner(deprecate_eof=True, on_infile=lambda: hits.append(1))
        sc.expect(mb.R_RESULT)
        sc.feed(_stream([b"\xfbfile.csv"]))
        self.assertEqual(hits, [1])
        self.assertTrue(sc.busy)
        data, _ = pkt(4, _ok())
        sc.feed(data)
        self.assertFalse(sc.busy)

    def test_unsolicited_and_desync(self):
        sc = mb.ResponseScanner()
        sc.feed(_stream([_ok()]))
        self.assertEqual(sc.unsolicited, 1)
        sc.expect(mb.R_RESULT)
        sc.feed(_stream([b"\xfc"]))                       # malformed column count
        self.assertIsNotNone(sc.desynced)

    def test_cache_metadata_skip(self):
        p = [lenenc_int(1) + b"\x00", b"\x011", _ok(0xFE)]
        done, _, _ = self.run_stream(_stream(p), [mb.R_RESULT], True, 2, cache_metadata=True)
        self.assertEqual(done, [0])


class TestGreeting(unittest.IsolatedAsyncioTestCase):
    async def test_parse_and_patch(self):
        srv = await FakeMySQLServer("p", tls_ctx=None).start()
        try:
            r, w = await asyncio.open_connection("127.0.0.1", srv.port)
            _, g = await mb.read_packet(r)
            w.close()
            gr = mb.parse_greeting(g)
            self.assertTrue(gr.caps & mb.CLIENT_DEPRECATE_EOF)
            self.assertTrue(gr.caps & mb.CLIENT_COMPRESS)
            self.assertFalse(gr.caps & mb.CLIENT_LONG_PASSWORD)
            self.assertEqual(gr.ext_caps, 0x04)
            self.assertEqual(len(gr.salt), 20)
            self.assertEqual(gr.plugin, "mysql_native_password")
            patched = mb.patch_greeting(g, gr, gr.caps & ~mb.STRIPPED_SERVER_CAPS)
            g2 = mb.parse_greeting(patched)
            self.assertFalse(g2.caps & (mb.CLIENT_COMPRESS | mb.CLIENT_SSL))
            self.assertEqual(g2.salt, gr.salt)
            self.assertEqual(len(patched), len(g))
        finally:
            await srv.stop()


class TestConfig(unittest.TestCase):
    def setUp(self):
        self.saved = {k: v for k, v in os.environ.items() if k.startswith("BRIDGE_")}
        for k in self.saved:
            del os.environ[k]

    def tearDown(self):
        for k in [k for k in os.environ if k.startswith("BRIDGE_")]:
            del os.environ[k]
        os.environ.update(self.saved)

    def load(self, text):
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.read_string(text)
        return mb.build_config(cfg)

    def test_example_config_is_valid_and_compatible(self):
        cfg = configparser.ConfigParser(interpolation=None)
        cfg.read(os.path.join(os.path.dirname(HERE), "bridge.cfg.example"))
        c = mb.build_config(cfg)
        self.assertEqual(c.listen_host, "127.0.0.1")
        self.assertEqual(c.listen_port, 33060)
        self.assertEqual(c.replica.user, "mrpg_bridge")
        self.assertEqual(c.replica.database, "mrpg_shared")
        self.assertFalse(c.primary.use_tls)

    def test_minimal_old_style_config(self):
        c = self.load("[bridge]\n[primary]\nhost=db\nport=3306\n[replica]\nhost=127.0.0.1\nport=3306\n"
                      "user=u\npassword=p\ndatabase=mmotee\n")
        self.assertEqual(c.max_clients, 256)
        self.assertEqual(c.pool_size, 16)
        self.assertEqual(c.acquire_timeout, 0.25)
        self.assertEqual(c.grace, 0.0)
        self.assertEqual(c.ryw_scope, "table")

    def test_env_overrides_and_password_file(self):
        with tempfile.NamedTemporaryFile("w", delete=False) as fh:
            fh.write("filepw\n")
        try:
            os.environ["BRIDGE_PRIMARY_HOST"] = "envhost"
            os.environ["BRIDGE_LISTEN_PORT"] = "4000"
            c = self.load("[bridge]\n[primary]\nhost=db\n[replica]\nhost=h\nuser=u\npassword=p\n"
                          "password_file=%s\n" % fh.name)
            self.assertEqual(c.primary.host, "envhost")
            self.assertEqual(c.listen_port, 4000)
            self.assertEqual(c.replica.password, "filepw")  # password_file wins over inline
            c = self.load("[bridge]\n[primary]\nhost=db\n[replica]\nhost=h\nuser=u\n"
                          "password_file=%s\n" % fh.name)
            self.assertEqual(c.replica.password, "filepw")
            os.environ["BRIDGE_REPLICA_PASSWORD"] = "envpw"
            c = self.load("[bridge]\n[primary]\nhost=db\n[replica]\nhost=h\nuser=u\n"
                          "password_file=%s\n" % fh.name)
            self.assertEqual(c.replica.password, "envpw")
        finally:
            os.unlink(fh.name)

    def test_invalid(self):
        base = "[bridge]\n%s\n[primary]\nhost=db\n[replica]\nhost=h\nuser=u\n"
        for bad in ["listen_port=70000", "max_clients=0", "read_your_writes_scope=bogus",
                    "replica_pool_size=abc", "allowed_clients=10.0.0.0/33", "log_level=LOUD",
                    "read_your_writes_grace_sec=-1"]:
            with self.assertRaises(mb.ConfigError, msg=bad):
                self.load(base % bad)
        with self.assertRaises(mb.ConfigError):
            self.load("[bridge]\n[primary]\nhost=db\n")
        with self.assertRaises(mb.ConfigError):
            self.load("[bridge]\n[primary]\nhost=db\n[replica]\nhost=h\nuser=u\ntls_mode=x\n")

    def test_check_config_cli(self):
        ex = os.path.join(os.path.dirname(HERE), "bridge.cfg.example")
        out = subprocess.run([sys.executable, os.path.join(os.path.dirname(HERE), "mysql_bridge.py"),
                              ex, "--check-config"], capture_output=True, text=True, timeout=30)
        self.assertEqual(out.returncode, 0, out.stderr)
        out = subprocess.run([sys.executable, os.path.join(os.path.dirname(HERE), "mysql_bridge.py"),
                              "/nonexistent.cfg", "--check-config"], capture_output=True, text=True,
                             timeout=30)
        self.assertEqual(out.returncode, 2)


# --- integration: routing ----------------------------------------------------------
class TestRouting(BridgeTestCase):
    async def test_reads_to_replica_writes_to_primary(self):
        b = await self.bridge()
        for deof in (True, False):
            c = await self.client(b, deprecate_eof=deof)
            self.assertEqual(await self.served_by(c, "SELECT * FROM tw_accounts WHERE ID = 1"), "replica")
            res = await c.query("INSERT INTO tw_accounts (a) VALUES (1)")
            self.assertEqual(res[0][0], "ok")
            self.assertEqual(res[0][1], 1)
            self.assertEqual(await self.served_by(c, "SHOW TABLES"), "replica")
            self.assertEqual(await self.served_by(c, "SELECT LAST_INSERT_ID()"), "primary")
        self.assertTrue(self.primary.queries_matching("INSERT INTO tw_accounts"))
        self.assertFalse(self.replica.queries_matching("INSERT"))
        self.assertEqual(mb.STATS["replica"], 4)

    async def test_large_resultsets_identical(self):
        b = await self.bridge(replica_buffer_kb=64)
        for deof in (True, False):
            c = await self.client(b, deprecate_eof=deof)
            rows_r = await c.rows("SELECT ROWS 20000 50")
            self.assertEqual(len(rows_r), 20000)
            self.assertEqual(rows_r[7], ["7", "v" * 50, None])
            rows_p = await c.rows("SELECT ROWS 20000 50 FOR UPDATE")   # forced to primary
            self.assertEqual(rows_r, rows_p)

    async def test_long_terminator_no_hang(self):
        self.replica.long_terminator = True
        self.primary.long_terminator = True
        b = await self.bridge()
        c = await self.client(b, deprecate_eof=True, session_track=True)
        t0 = time.monotonic()
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        self.assertLess(time.monotonic() - t0, 1.0)

    async def test_multi_statement_never_reaches_replica(self):
        b = await self.bridge()
        c = await self.client(b, multi_statements=True)
        self.assertEqual(await self.served_by(c, "SELECT 0"), "replica")
        res = await c.query("SELECT 1; DELETE FROM tw_accounts")
        self.assertEqual([r[0] for r in res], ["rows", "ok"])
        self.assertEqual(res[0][2][0][0], "primary")
        self.assertFalse(self.replica.queries_matching("DELETE"))
        # replica connections never negotiate multi-statements
        self.assertTrue(self.replica.sessions)
        for s in self.replica.sessions:
            self.assertFalse(s.caps & 0x10000)

    async def test_transactions_and_autocommit(self):
        b = await self.bridge()
        c = await self.client(b)
        await c.query("BEGIN")
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        await c.query("COMMIT")
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        await c.query("SET autocommit=0")
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        await c.query("SET autocommit=1")
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_session_state_pins_to_primary(self):
        b = await self.bridge()
        c = await self.client(b)
        await c.query("CREATE TEMPORARY TABLE tmp (a int)")
        self.assertEqual(await self.served_by(c, "SELECT * FROM tmp"), "primary")
        c2 = await self.client(b)
        await c2.query("LOCK TABLES t READ")
        self.assertEqual(await self.served_by(c2, "SELECT * FROM t"), "primary")
        await c2.query("UNLOCK TABLES")
        self.assertEqual(await self.served_by(c2, "SELECT * FROM t"), "replica")

    async def test_read_your_writes_table_scope(self):
        b = await self.bridge(read_your_writes_grace_sec="0.4", read_your_writes_scope="table")
        a = await self.client(b)
        w = await self.client(b)
        await w.query("INSERT INTO tw_accounts (Username) VALUES ('x')")
        # another connection (game thread pool) reading the written table -> primary
        self.assertEqual(await self.served_by(a, "SELECT ID FROM tw_accounts ORDER BY ID DESC LIMIT 1"),
                         "primary")
        self.assertEqual(await self.served_by(a, "SELECT * FROM tw_guilds"), "replica")
        # the writing connection itself is pinned for the window
        self.assertEqual(await self.served_by(w, "SELECT * FROM tw_guilds"), "primary")
        await asyncio.sleep(0.5)
        self.assertEqual(await self.served_by(a, "SELECT ID FROM tw_accounts"), "replica")
        await w.query("CREATE TABLE IF NOT EXISTS z (a int)")         # unattributable write
        self.assertEqual(await self.served_by(a, "SELECT * FROM tw_guilds"), "primary")

    async def test_read_your_writes_connection_scope(self):
        b = await self.bridge(read_your_writes_grace_sec="5", read_your_writes_scope="connection")
        a = await self.client(b)
        w = await self.client(b)
        await w.query("INSERT INTO tw_accounts (Username) VALUES ('x')")
        self.assertEqual(await self.served_by(a, "SELECT ID FROM tw_accounts"), "replica")
        self.assertEqual(await self.served_by(w, "SELECT ID FROM tw_accounts"), "primary")

    async def test_write_inside_transaction_marked_at_commit(self):
        b = await self.bridge(read_your_writes_grace_sec="0.3", read_your_writes_scope="table")
        a = await self.client(b)
        w = await self.client(b)
        await w.query("BEGIN")
        await w.query("UPDATE tw_items SET a = 1 WHERE ID = 2")
        await asyncio.sleep(0.4)
        await w.query("COMMIT")
        self.assertEqual(await self.served_by(a, "SELECT * FROM tw_items"), "primary")

    async def test_use_is_per_session(self):
        b = await self.bridge()
        c1 = await self.client(b, db="mmotee")
        c2 = await self.client(b, db="mmotee")
        await c1.query("USE other")
        rows1 = await c1.rows("SELECT 1")
        rows2 = await c2.rows("SELECT 1")
        self.assertEqual(rows1[0][:2], ["replica", "other"])
        self.assertEqual(rows2[0][:2], ["replica", "mmotee"])
        await c2.query("USE `other`")
        self.assertEqual((await c2.rows("SELECT 2"))[0][1], "other")

    async def test_unknown_db_on_replica_falls_back(self):
        self.primary.databases.add("primaryonly")
        b = await self.bridge()
        c = await self.client(b)
        await c.query("USE primaryonly")
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")

    async def test_session_set_replayed_on_replica(self):
        b = await self.bridge()
        c = await self.client(b)
        await c.query("SET NAMES latin1")
        await c.query("SET @user_var = 5")                            # not replayed
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        replayed = [q for _, q in self.replica.queries if q.startswith("SET")]
        self.assertEqual(replayed, ["SET NAMES latin1"])
        c2 = await self.client(b)
        self.assertEqual(await self.served_by(c2, "SELECT 1"), "replica")
        profiles = {p[3] for p in b.pool._idle}
        self.assertIn((b"SET NAMES latin1",), profiles)
        self.assertIn((), profiles)

    async def test_charset_is_mirrored(self):
        b = await self.bridge()
        c = await self.client(b, charset=8)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        self.assertEqual(self.replica.sessions[-1].charset, 8)

    async def test_prepared_statements_and_ping(self):
        b = await self.bridge(read_your_writes_grace_sec="1")
        c = await self.client(b)
        sid = await c.prepare("SELECT ? FROM t")
        res = await c.execute(sid)
        self.assertEqual(res[0][0], "rows")
        self.assertTrue(await c.ping())
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        wid = await c.prepare("INSERT INTO t VALUES (?)")
        res = await c.execute(wid)
        self.assertEqual(res[0][0], "ok")
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")     # write via execute

    async def test_pipelined_commands_keep_order(self):
        b = await self.bridge()
        c = await self.client(b)
        self.primary.delay = 0.05
        data = b""
        for q in (b"INSERT INTO t VALUES (1)", b"SELECT 1", b"SELECT 2"):
            data += pkt(0, b"\x03" + q)[0]
        c.w.write(data)
        await c.w.drain()
        r1 = await c.read_results()
        r2 = await c.read_results()
        r3 = await c.read_results()
        self.assertEqual(r1[0][0], "ok")
        self.assertEqual(r2[0][2][0][2], "SELECT 1")
        self.assertEqual(r3[0][2][0][2], "SELECT 2")

    async def test_local_infile_is_relayed_not_interpreted(self):
        b = await self.bridge()
        c = await self.client(b)
        evil = b"\x03SELECT 1" + b"\x01" + b"x" * 40000
        res = await c.query("LOAD DATA LOCAL INFILE 'data.csv' INTO TABLE t", infile_data=evil)
        self.assertEqual(res[0][0], "ok")
        self.assertEqual(res[0][1], len(evil))
        self.assertEqual(b"".join(self.primary.infile_data), evil)
        self.assertFalse(self.replica.queries_matching("SELECT 1"))
        self.assertEqual(await self.served_by(c, "SELECT 2"), "replica")

    async def test_change_user_switches_to_relay(self):
        b = await self.bridge()
        c = await self.client(b)
        await c.send_command(b"\x11game\x00")
        await c.read_results()
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")

    async def test_route_disabled_is_pure_proxy(self):
        b = await self.bridge(route_selects_to_replica="false")
        c = await self.client(b)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        self.assertEqual(self.replica.total, 0)


# --- integration: failure handling ----------------------------------------------------
class TestFailover(BridgeTestCase):
    async def test_replica_error_falls_back(self):
        b = await self.bridge()
        c = await self.client(b)
        self.primary.databases.add("x")
        res = await c.query("SELECT fail")
        self.assertEqual(res[0][0], "err")                       # primary's error, correctly framed
        self.assertEqual(len(self.primary.queries_matching("SELECT fail")), 1)
        self.assertEqual(mb.STATS["fallback"], 1)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_replica_down_fast_fallback_and_recovery(self):
        port = self.replica.port
        await self.replica.stop()
        b = await self.bridge()
        c = await self.client(b)
        t0 = time.monotonic()
        for _ in range(20):
            self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        self.assertLess(time.monotonic() - t0, 2.0)
        self.assertTrue(b.pool.is_down)
        self.replica = await FakeMySQLServer("replica", read_only=True).start(port=port)
        await asyncio.sleep(0.6)
        for _ in range(20):
            if await self.served_by(c, "SELECT 1") == "replica":
                break
            await asyncio.sleep(0.2)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_replica_hanging_at_login_is_bounded(self):
        async def blackhole(r, w):
            await asyncio.sleep(10)
            w.close()
        bh = await asyncio.start_server(blackhole, "127.0.0.1", 0)
        try:
            conf = make_conf(self.primary.port, bh.sockets[0].getsockname()[1],
                             replica_opts={"connect_timeout_sec": "0.3"})
            b = mb.Bridge(conf)
            await b.start()
            self.bridges.append(b)
            c = await self.client(b)
            t0 = time.monotonic()
            self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
            self.assertLess(time.monotonic() - t0, 1.5)
        finally:
            bh.close()

    async def test_replica_killed_between_queries_is_retried(self):
        b = await self.bridge()
        c = await self.client(b)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        self.replica.kill_all()
        await asyncio.sleep(0.05)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_replica_slow_times_out_to_primary(self):
        b = await self.bridge(replica_query_timeout_sec="0.2")
        c = await self.client(b)
        self.replica.delay = 1.0
        t0 = time.monotonic()
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        self.assertLess(time.monotonic() - t0, 0.9)

    async def test_error_mid_stream_is_forwarded_framed(self):
        b = await self.bridge()
        c = await self.client(b)
        res = await c.query("SELECT errmid")
        self.assertEqual([r[0] for r in res], ["rows", "err"])
        self.assertEqual(res[1][1], 1317)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_stream_broken_drops_only_that_client(self):
        b = await self.bridge(replica_buffer_kb=16)
        c = await self.client(b)
        await c.send_command(b"\x03SELECT ROWS 200000 100")
        await asyncio.sleep(0.05)
        self.replica.kill_all()
        with self.assertRaises((asyncio.IncompleteReadError, ConnectionError, AssertionError)):
            await asyncio.wait_for(c.read_results(), 5)
        c2 = await self.client(b)
        self.assertIn(await self.served_by(c2, "SELECT 1"), ("replica", "primary"))

    async def test_pool_exhaustion_falls_back(self):
        b = await self.bridge(replica_pool_size="1", replica_acquire_timeout_sec="0.1")
        c1 = await self.client(b)
        c2 = await self.client(b)
        slow = asyncio.ensure_future(c1.rows("SELECT SLOW 0.5"))
        await asyncio.sleep(0.1)
        t0 = time.monotonic()
        self.assertEqual(await self.served_by(c2, "SELECT 1"), "primary")
        self.assertLess(time.monotonic() - t0, 0.4)
        self.assertEqual((await slow)[0][0], "replica")
        self.assertEqual(await self.served_by(c2, "SELECT 1"), "replica")
        self.assertEqual(b.pool.total, 1)

    async def test_pool_survives_cancellation_storm(self):
        b = await self.bridge(replica_pool_size="3", replica_acquire_timeout_sec="0.2")
        pool = b.pool
        profile = (mb.CLIENT_DEPRECATE_EOF | mb.CLIENT_MULTI_RESULTS, 0, 45, ())
        rnd = random.Random(7)

        async def user():
            conn = await pool.acquire(profile)
            if conn is not None:
                try:
                    await asyncio.sleep(rnd.random() * 0.02)
                finally:
                    pool.release(conn)
        for _ in range(5):
            tasks = [asyncio.ensure_future(user()) for _ in range(40)]
            await asyncio.sleep(rnd.random() * 0.03)
            for t in tasks:
                if rnd.random() < 0.5:
                    t.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            idle = sum(len(d) for d in pool._idle.values())
            self.assertLessEqual(pool.total, 3)
            self.assertEqual(pool.total, idle)               # no leaked slots
            self.assertFalse(pool._waiters)
        c = await self.client(b)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_pool_profiles_share_slots(self):
        b = await self.bridge(replica_pool_size="1")
        c1 = await self.client(b, charset=8)
        c2 = await self.client(b, charset=45)
        for _ in range(3):
            self.assertEqual(await self.served_by(c1, "SELECT 1"), "replica")
            self.assertEqual(await self.served_by(c2, "SELECT 1"), "replica")
        self.assertEqual(b.pool.total, 1)

    async def test_concurrent_load_is_consistent(self):
        b = await self.bridge(replica_pool_size="3")
        clients = [await self.client(b) for _ in range(12)]

        async def worker(c, i):
            for j in range(25):
                if j % 5 == 0:
                    res = await c.query("UPDATE t SET a = %d" % j)
                    assert res[0][0] == "ok"
                else:
                    rows = await c.rows("SELECT %d_%d" % (i, j))
                    assert rows[0][2] == "SELECT %d_%d" % (i, j), rows
        await asyncio.wait_for(asyncio.gather(*(worker(c, i) for i, c in enumerate(clients))), 30)
        self.assertLessEqual(b.pool.total, 3)

    async def test_lag_monitor(self):
        b = await self.bridge(max_replica_lag_sec="5", replica_lag_check_interval_sec="0.1")
        c = await self.client(b)
        await asyncio.sleep(0.3)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        self.replica.lag = 100
        await asyncio.sleep(0.3)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        self.replica.lag = None                                  # replication broken
        await asyncio.sleep(0.3)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        self.replica.lag = "none"                                # not a replica: no lag
        await asyncio.sleep(0.3)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        self.replica.lag_error = True                            # missing privilege
        await asyncio.sleep(0.3)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")

    async def test_primary_unreachable_gives_error(self):
        port = self.primary.port
        await self.primary.stop()
        b = await self.bridge()
        c = MiniClient()
        with self.assertRaises(ClientError) as cm:
            await asyncio.wait_for(c.connect(b.ports[0]), 5)
        self.assertEqual(cm.exception.code, 2003)
        self.primary = await FakeMySQLServer("primary").start(port=port)

    async def test_shutdown_with_active_clients(self):
        b = await self.bridge()
        await self.client(b)
        await self.client(b)
        t0 = time.monotonic()
        await b.stop()
        self.bridges.remove(b)
        self.assertLess(time.monotonic() - t0, 3)


# --- integration: security ----------------------------------------------------------
class TestSecurity(BridgeTestCase):
    async def test_bad_login_never_touches_replica(self):
        b = await self.bridge()
        c = MiniClient()
        with self.assertRaises(ClientError) as cm:
            await asyncio.wait_for(c.connect(b.ports[0], password="wrong"), 5)
        self.assertEqual(cm.exception.code, 1045)
        self.assertEqual(self.replica.total, 0)

    async def test_pre_auth_query_goes_to_primary(self):
        b = await self.bridge()
        r, w = await asyncio.open_connection("127.0.0.1", b.ports[0])
        await mb.read_packet(r)                                  # greeting
        w.write(pkt(0, b"\x03SELECT * FROM tw_accounts")[0])   # no login at all
        await w.drain()
        await asyncio.sleep(0.2)
        w.close()
        self.assertEqual(self.replica.total, 0)

    async def test_greeting_hides_tls_and_compression(self):
        self.primary.tls_ctx = None
        b = await self.bridge()
        c = await self.client(b)
        self.assertFalse(c.greeting_caps & (mb.CLIENT_SSL | mb.CLIENT_COMPRESS))

    async def test_client_tls_and_compression_requests_rejected(self):
        b = await self.bridge()
        c = MiniClient()
        await c.connect(b.ports[0], request_ssl=True)
        self.assertEqual(await asyncio.wait_for(c.r.read(), 3), b"")
        c2 = MiniClient()
        with self.assertRaises(ClientError):
            await asyncio.wait_for(c2.connect(b.ports[0], compress=True), 3)

    async def test_max_clients(self):
        b = await self.bridge(max_clients="2")
        await self.client(b)
        await self.client(b)
        c = MiniClient()
        with self.assertRaises(ClientError) as cm:
            await asyncio.wait_for(c.connect(b.ports[0]), 3)
        self.assertEqual(cm.exception.code, 1040)
        self.assertEqual(self.primary.total, 2)

    async def test_allowed_clients(self):
        b = await self.bridge(allowed_clients="10.0.0.0/8")
        c = MiniClient()
        with self.assertRaises(ClientError) as cm:
            await asyncio.wait_for(c.connect(b.ports[0]), 3)
        self.assertEqual(cm.exception.code, 1130)
        b2 = await self.bridge(allowed_clients="10.0.0.0/8, 127.0.0.1/32")
        await self.client(b2)

    async def test_auth_timeout_and_idle_timeout(self):
        b = await self.bridge(auth_timeout_sec="0.5", client_idle_timeout_sec="1.5")
        r, w = await asyncio.open_connection("127.0.0.1", b.ports[0])
        await mb.read_packet(r)
        t0 = time.monotonic()
        self.assertEqual(await asyncio.wait_for(r.read(), 5), b"")
        self.assertLess(time.monotonic() - t0, 2.5)
        c = await self.client(b)
        await asyncio.sleep(3.0)
        with self.assertRaises((asyncio.IncompleteReadError, ConnectionError, AssertionError)):
            await asyncio.wait_for(c.rows("SELECT 1"), 3)
        self.assertEqual(len(b.sessions), 0)

    async def test_long_query_not_killed_by_idle_timeout(self):
        b = await self.bridge(client_idle_timeout_sec="1", route_selects_to_replica="true")
        c = await self.client(b)
        rows = await asyncio.wait_for(c.rows("SELECT SLOW 2.5 FOR UPDATE"), 6)
        self.assertEqual(rows[0][0], "primary")


@unittest.skipUnless(shutil.which("openssl"), "openssl CLI needed to create test certificates")
class TestTLS(BridgeTestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        cls.cert = os.path.join(cls.tmp, "cert.pem")
        cls.key = os.path.join(cls.tmp, "key.pem")
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
                        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
                        "-keyout", cls.key, "-out", cls.cert], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def server_ctx(self):
        ctx = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
        ctx.load_cert_chain(self.cert, self.key)
        return ctx

    async def test_starttls_to_primary_and_replica(self):
        self.primary.tls_ctx = self.server_ctx()
        self.primary.require_tls = True
        self.replica.tls_ctx = self.server_ctx()
        self.replica.require_tls = True
        tls = {"use_tls": "true", "tls_ca_file": self.cert}
        b = await self.bridge(primary_opts=tls, replica_opts=tls)
        for deof in (True, False):
            c = await self.client(b, deprecate_eof=deof)
            self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
            self.assertEqual((await c.query("INSERT INTO t VALUES (1)"))[0][0], "ok")
            self.assertEqual(await self.served_by(c, "SELECT 1 FOR UPDATE"), "primary")
        self.assertTrue(all(l[3] for l in self.primary.logins))
        self.assertTrue(all(l[3] for l in self.replica.logins))

    async def test_starttls_with_auth_switch(self):
        self.primary.tls_ctx = self.server_ctx()
        self.primary.auth_switch = True
        self.replica.auth_switch = True
        b = await self.bridge(primary_opts={"use_tls": "true", "tls_ca_file": self.cert})
        c = await self.client(b)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")
        c2 = MiniClient()
        with self.assertRaises(ClientError):
            await asyncio.wait_for(c2.connect(b.ports[0], password="bad"), 5)

    async def test_certificate_verification(self):
        self.primary.tls_ctx = self.server_ctx()
        b = await self.bridge(primary_opts={"use_tls": "true"})      # system CAs: must fail
        c = MiniClient()
        with self.assertRaises((ClientError, asyncio.IncompleteReadError, ConnectionError)):
            await asyncio.wait_for(c.connect(b.ports[0]), 5)
        b2 = await self.bridge(primary_opts={"use_tls": "true", "tls_verify": "false"})
        await self.client(b2)

    async def test_tls_required_but_unsupported(self):
        self.primary.tls_ctx = None
        b = await self.bridge(primary_opts={"use_tls": "true", "tls_verify": "false"})
        c = MiniClient()
        with self.assertRaises(ClientError) as cm:
            await asyncio.wait_for(c.connect(b.ports[0]), 5)
        self.assertEqual(cm.exception.code, 2026)
        self.assertEqual(self.primary.logins, [])


class TestCompatPaths(TestTLS):
    """Re-run the TLS tests through the Python < 3.11 code paths (timeout
    shim and loop.start_tls upgrade) so they are exercised on any version."""

    async def asyncSetUp(self):
        await super().asyncSetUp()
        saved = (mb._timeout, mb.upgrade_tls)
        mb._timeout = mb._CompatTimeout
        mb.upgrade_tls = mb._upgrade_tls_compat
        self.addCleanup(lambda: (setattr(mb, "_timeout", saved[0]),
                                 setattr(mb, "upgrade_tls", saved[1])))

    async def test_compat_timeout_fires(self):
        with self.assertRaises(asyncio.TimeoutError):
            async with mb._timeout(0.05):
                await asyncio.sleep(1)
        async with mb._timeout(1):
            await asyncio.sleep(0)

    async def test_replica_slow_times_out_to_primary(self):
        b = await self.bridge(replica_query_timeout_sec="0.2")
        c = await self.client(b)
        self.replica.delay = 1.0
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")


if os.environ.get("BRIDGE_TEST_COMPAT"):                     # whole suite via compat paths
    mb._timeout = mb._CompatTimeout
    mb.upgrade_tls = mb._upgrade_tls_compat


class TestBackendAuth(BridgeTestCase):
    async def test_auth_switch_and_empty_password(self):
        self.replica.auth_switch = True
        b = await self.bridge(replica_opts={"user": "nopw", "password": ""})
        c = await self.client(b)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "replica")

    async def test_wrong_replica_password_marks_down(self):
        b = await self.bridge(replica_opts={"password": "wrong"})
        c = await self.client(b)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")
        self.assertTrue(b.pool.is_down)

    async def test_replica_without_deprecate_eof(self):
        self.replica.deprecate_eof = False
        b = await self.bridge()
        c = await self.client(b, deprecate_eof=True)
        self.assertEqual(await self.served_by(c, "SELECT 1"), "primary")   # format mismatch
        c2 = await self.client(b, deprecate_eof=False)
        self.assertEqual(await self.served_by(c2, "SELECT 1"), "replica")


# --- interop with real client libraries / independent server -------------------------
def _have(mod):
    try:
        __import__(mod)
        return True
    except ImportError:
        return False


class TestInterop(BridgeTestCase):
    def run_sync(self, fn):
        return asyncio.get_running_loop().run_in_executor(None, fn)

    @unittest.skipUnless(_have("pymysql"), "PyMySQL not installed")
    async def test_pymysql(self):
        import pymysql
        b = await self.bridge()

        def work():
            conn = pymysql.connect(host="127.0.0.1", port=b.ports[0], user="game", password="gamepw",
                                   database="mmotee", autocommit=True)
            with conn.cursor() as cur:
                cur.execute("SELECT * FROM tw_accounts WHERE ID = %s", (1,))
                a = cur.fetchall()
                cur.execute("INSERT INTO t VALUES (1)")
                cur.execute("SELECT ROWS 3000")
                n = len(cur.fetchall())
            conn.close()
            return a, n
        a, n = await self.run_sync(work)
        self.assertEqual(a[0][0], "replica")
        self.assertEqual(n, 3000)

    @unittest.skipUnless(_have("mariadb"), "MariaDB Connector/Python not installed")
    async def test_mariadb_connector(self):
        import mariadb
        b = await self.bridge()

        def work():
            conn = mariadb.connect(host="127.0.0.1", port=b.ports[0], user="game", password="gamepw",
                                   database="mmotee", autocommit=True)
            cur = conn.cursor()
            cur.execute("SELECT * FROM tw_accounts")
            a = cur.fetchall()
            cur.execute("INSERT INTO t VALUES (1)")
            cur.execute("SELECT ROWS 2000")
            n = len(cur.fetchall())
            conn.close()
            return a, n
        a, n = await self.run_sync(work)
        self.assertEqual(a[0][0], "replica")
        self.assertEqual(n, 2000)

    @unittest.skipUnless(_have("mysql.connector"), "mysql-connector-python not installed")
    async def test_mysql_connector(self):
        import mysql.connector
        b = await self.bridge()

        def work():
            conn = mysql.connector.connect(host="127.0.0.1", port=b.ports[0], user="game",
                                           password="gamepw", database="mmotee", autocommit=True,
                                           use_pure=True, ssl_disabled=True)
            cur = conn.cursor()
            cur.execute("SELECT * FROM tw_accounts")
            a = cur.fetchall()
            conn.close()
            return a
        a = await self.run_sync(work)
        self.assertEqual(a[0][0], "replica")

    @unittest.skipUnless(_have("mysql_mimic") and _have("pymysql"), "mysql-mimic/PyMySQL not installed")
    async def test_against_mysql_mimic_backends(self):
        """Independent server implementation as primary *and* replica."""
        import pymysql
        from mysql_mimic import MysqlServer, Session
        from mysql_mimic.auth import NativePasswordAuthPlugin, User, IdentityProvider

        class Ident(IdentityProvider):
            def get_plugins(self):
                return [NativePasswordAuthPlugin()]

            async def get_user(self, username):
                pw = {"game": "gamepw", "bridge": "bridgepw"}.get(username)
                if pw is None:
                    return None
                return User(name=username, auth_string=NativePasswordAuthPlugin.create_auth_string(pw))

        def session_cls(name):
            class S(Session):
                async def query(self, expression, sql, attrs):
                    return [(name, i) for i in range(500)], ["server", "i"]

                async def schema(self):
                    return {"mmotee": {"t": {"a": "TEXT"}}}
            return S

        # mysql-mimic never reports SERVER_STATUS_AUTOCOMMIT; the bridge then
        # (correctly) assumes autocommit=0 and keeps every read on the primary.
        # Real MariaDB/MySQL always set it, so emulate that here.
        from mysql_mimic import connection as mimic_conn, types as mimic_types
        orig_init = mimic_conn.Connection.__init__

        def patched_init(conn_self, *a, **kw):
            orig_init(conn_self, *a, **kw)
            conn_self.status_flags = mimic_types.ServerStatus.SERVER_STATUS_AUTOCOMMIT
        mimic_conn.Connection.__init__ = patched_init
        self.addCleanup(setattr, mimic_conn.Connection, "__init__", orig_init)

        servers = []
        ports = []
        for name in ("primary", "replica"):
            srv = MysqlServer(session_factory=session_cls(name), identity_provider=Ident())
            await srv.start_server(host="127.0.0.1", port=0)
            servers.append(srv)
            ports.append(srv._server.sockets[0].getsockname()[1])
        try:
            conf = make_conf(ports[0], ports[1])
            b = mb.Bridge(conf)
            await b.start()
            self.bridges.append(b)

            def work():
                conn = pymysql.connect(host="127.0.0.1", port=b.ports[0], user="game",
                                       password="gamepw", database="mmotee", autocommit=True)
                with conn.cursor() as cur:
                    out = []
                    for q in ("SELECT a FROM t", "SELECT a FROM t FOR UPDATE", "SELECT a FROM t"):
                        cur.execute(q)
                        rows = cur.fetchall()
                        out.append((rows[0][0], len(rows)))
                conn.close()
                return out
            out = await self.run_sync(work)
            self.assertEqual(out, [("replica", 500), ("primary", 500), ("replica", 500)])
        finally:
            for srv in servers:
                srv._server.close()


if __name__ == "__main__":
    unittest.main()
