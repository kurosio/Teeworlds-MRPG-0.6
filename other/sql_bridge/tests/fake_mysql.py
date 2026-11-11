"""
Protocol-level fake MariaDB server and minimal client used by the bridge
tests and benchmarks.  Standard library only.

The server implements the parts of the MariaDB/MySQL client protocol the
bridge has to handle: HandshakeV10 (MariaDB extended capabilities), the
SSLRequest/TLS upgrade, mysql_native_password (optionally via an auth
switch), COM_QUERY with OK/ERR/result sets (with and without
CLIENT_DEPRECATE_EOF), multi-statements and multi-results, server status
flags (transactions/autocommit), LOCAL INFILE, prepared statements, mid-
stream errors, and SHOW SLAVE STATUS for the lag monitor.

Query language understood by the fake (case-insensitive):
  SELECT ROWS <n> [<width>]   n rows x 3 columns (response cached, fast)
  SELECT SLOW <seconds>       sleeps, then one row
  SELECT ERRMID               2 rows, then an ERR packet mid-stream
  SELECT FAIL                 ERR 1146
  SELECT @@...                one row
  other SELECT/SHOW/DESC/...  one row: server name, current db, query text
  BEGIN/START TRANSACTION/COMMIT/ROLLBACK, SET AUTOCOMMIT=0|1, SET ..., USE db
  INSERT/UPDATE/DELETE/...    OK (affected 1), ERR 1290 when read_only
  LOAD DATA LOCAL INFILE 'f'  LOCAL INFILE request, OK with the byte count
  SHOW [ALL] SLAVE STATUS     replication status from `server.lag`
"""

import asyncio
import hashlib
import os
import re
import ssl
import struct

CLIENT_LONG_PASSWORD = 0x1
CLIENT_LONG_FLAG = 0x4
CLIENT_CONNECT_WITH_DB = 0x8
CLIENT_COMPRESS = 0x20
CLIENT_LOCAL_FILES = 0x80
CLIENT_PROTOCOL_41 = 0x200
CLIENT_SSL = 0x800
CLIENT_TRANSACTIONS = 0x2000
CLIENT_SECURE_CONNECTION = 0x8000
CLIENT_MULTI_STATEMENTS = 0x10000
CLIENT_MULTI_RESULTS = 0x20000
CLIENT_PS_MULTI_RESULTS = 0x40000
CLIENT_PLUGIN_AUTH = 0x80000
CLIENT_CONNECT_ATTRS = 0x100000
CLIENT_PLUGIN_AUTH_LENENC = 0x200000
CLIENT_SESSION_TRACK = 0x800000
CLIENT_DEPRECATE_EOF = 0x1000000

ST_IN_TRANS = 0x1
ST_AUTOCOMMIT = 0x2
ST_MORE = 0x8


def lenenc_int(n):
    if n < 0xFB:
        return bytes([n])
    if n < 1 << 16:
        return b"\xfc" + struct.pack("<H", n)
    if n < 1 << 24:
        return b"\xfd" + struct.pack("<I", n)[:3]
    return b"\xfe" + struct.pack("<Q", n)


def lenenc_str(s):
    if s is None:
        return b"\xfb"
    if isinstance(s, str):
        s = s.encode()
    return lenenc_int(len(s)) + s


def pkt(seq, payload):
    out = bytearray()
    while True:
        chunk = payload[:0xFFFFFF]
        payload = payload[0xFFFFFF:]
        out += struct.pack("<I", len(chunk) | ((seq & 0xFF) << 24)) + chunk
        seq += 1
        if len(chunk) < 0xFFFFFF:
            return bytes(out), seq


async def read_pkt(reader):
    hdr = await reader.readexactly(4)
    n = int.from_bytes(hdr[:3], "little")
    seq = hdr[3]
    data = await reader.readexactly(n) if n else b""
    while n == 0xFFFFFF:
        hdr = await reader.readexactly(4)
        n = int.from_bytes(hdr[:3], "little")
        data += await reader.readexactly(n) if n else b""
    return seq, data


def native_token(password, salt):
    if not password:
        return b""
    s1 = hashlib.sha1(password.encode()).digest()
    s2 = hashlib.sha1(s1).digest()
    m = hashlib.sha1(salt + s2).digest()
    return bytes(a ^ b for a, b in zip(s1, m))


def coldef(name, charset=45, ctype=0xFD):
    return (lenenc_str("def") + lenenc_str("db") + lenenc_str("t") + lenenc_str("t")
            + lenenc_str(name) + lenenc_str(name) + b"\x0c" + struct.pack("<H", charset)
            + struct.pack("<I", 1024) + bytes([ctype]) + b"\x00\x00" + b"\x00" + b"\x00\x00")


class TLSStream:
    """Server-side TLS over an existing asyncio stream using ssl.MemoryBIO.

    asyncio's start_tls() loses a ClientHello that was already read into the
    StreamReader buffer together with the SSLRequest packet; real servers do
    not have that problem, so the fake hands such leftover bytes to the TLS
    object explicitly.
    """

    def __init__(self, reader, writer, ctx):
        self.raw_r, self.raw_w = reader, writer
        self.inc, self.out = ssl.MemoryBIO(), ssl.MemoryBIO()
        self.obj = ctx.wrap_bio(self.inc, self.out, server_side=True)
        self.buf = bytearray()
        leftover = bytes(reader._buffer)
        reader._buffer.clear()
        if leftover:
            self.inc.write(leftover)
        self.transport = writer.transport

    async def _fill(self):
        data = await self.raw_r.read(65536)
        if not data:
            raise asyncio.IncompleteReadError(b"", None)
        self.inc.write(data)

    async def _flush(self):
        data = self.out.read()
        if data:
            self.raw_w.write(data)
            await self.raw_w.drain()

    async def handshake(self):
        while True:
            try:
                self.obj.do_handshake()
                break
            except ssl.SSLWantReadError:
                await self._flush()
                await self._fill()
        await self._flush()

    async def readexactly(self, n):
        while len(self.buf) < n:
            try:
                chunk = self.obj.read(65536)
                if not chunk:
                    raise asyncio.IncompleteReadError(bytes(self.buf), n)
                self.buf += chunk
            except ssl.SSLWantReadError:
                await self._flush()
                await self._fill()
            except ssl.SSLZeroReturnError:
                raise asyncio.IncompleteReadError(bytes(self.buf), n)
        data = bytes(self.buf[:n])
        del self.buf[:n]
        return data

    def write(self, data):
        self.obj.write(data)
        out = self.out.read()
        if out:
            self.raw_w.write(out)

    async def drain(self):
        await self.raw_w.drain()

    def close(self):
        self.raw_w.close()


class Session:
    def __init__(self, server, reader, writer, conn_id):
        self.server = server
        self.r = reader
        self.w = writer
        self.id = conn_id
        self.caps = 0
        self.ext = 0
        self.db = None
        self.user = None
        self.autocommit = True
        self.in_trans = False
        self.sets = []
        self.tls = False
        self.stmts = {}
        self.next_stmt = 1
        self.charset = 0

    @property
    def deof(self):
        return bool(self.caps & CLIENT_DEPRECATE_EOF)

    def status(self, more=False):
        st = (ST_AUTOCOMMIT if self.autocommit else 0) | (ST_IN_TRANS if self.in_trans else 0)
        return st | (ST_MORE if more else 0)

    # -- packet builders -----------------------------------------------------
    def ok_payload(self, affected=0, insert_id=0, more=False, header=0x00, info=b""):
        p = bytes([header]) + lenenc_int(affected) + lenenc_int(insert_id)
        p += struct.pack("<HH", self.status(more), 0)
        if info:
            p += lenenc_str(info) if self.caps & CLIENT_SESSION_TRACK else info
        return p

    def eof_payload(self, more=False):
        return b"\xfe" + struct.pack("<HH", 0, self.status(more))

    def terminator(self, more=False):
        if self.deof:
            info = b"x" * 24 if self.server.long_terminator else b""
            return self.ok_payload(header=0xFE, more=more, info=info)
        return self.eof_payload(more)

    def resultset(self, cols, rows, more=False):
        out = [lenenc_int(len(cols))]
        out += [coldef(c) for c in cols]
        if not self.deof:
            out.append(self.eof_payload())
        for row in rows:
            out.append(b"".join(lenenc_str(v) for v in row))
        out.append(self.terminator(more))
        return out

    @staticmethod
    def err_payload(code, msg, state="HY000"):
        return b"\xff" + struct.pack("<H", code) + b"#" + state.encode() + msg.encode()

    async def send(self, payloads, seq=1):
        buf = bytearray()
        for p in payloads:
            data, seq = pkt(seq, p)
            buf += data
        self.w.write(bytes(buf))
        await self.w.drain()
        return seq

    # -- connection ------------------------------------------------------------
    async def run(self):
        srv = self.server
        try:
            await self.handshake()
            while True:
                seq, p = await read_pkt(self.r)
                if not p:
                    await self.send([self.err_payload(1047, "empty packet")])
                    continue
                cmd = p[0]
                srv.commands.append((self.id, cmd, p[1:]))
                if cmd == 0x01:
                    return
                if cmd == 0x03:
                    await self.query(p[1:].decode("utf-8", "replace"))
                elif cmd == 0x02:
                    db = p[1:].decode()
                    if db in srv.databases:
                        self.db = db
                        await self.send([self.ok_payload()])
                    else:
                        await self.send([self.err_payload(1049, "Unknown database '%s'" % db, "42000")])
                elif cmd == 0x0E:
                    await self.send([self.ok_payload()])
                elif cmd == 0x09:
                    await self.send([b"Uptime: 1  Threads: 1"])
                elif cmd == 0x1F:
                    self.sets = []
                    self.in_trans = False
                    self.autocommit = True
                    await self.send([self.ok_payload()])
                elif cmd == 0x16:
                    await self.prepare(p[1:].decode())
                elif cmd == 0x17:
                    await self.execute(p)
                elif cmd == 0x19:
                    self.stmts.pop(int.from_bytes(p[1:5], "little"), None)
                elif cmd == 0x11:                          # COM_CHANGE_USER (simplified)
                    await self.send([self.ok_payload()])
                else:
                    await self.send([self.err_payload(1047, "Unknown command")])
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        finally:
            srv.active -= 1
            try:
                self.w.close()
            except Exception:
                pass

    async def handshake(self):
        srv = self.server
        salt = bytes((b % 94) + 33 for b in os.urandom(20))
        caps = (CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB | CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS
                | CLIENT_SECURE_CONNECTION | CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS
                | CLIENT_PS_MULTI_RESULTS | CLIENT_PLUGIN_AUTH | CLIENT_CONNECT_ATTRS
                | CLIENT_PLUGIN_AUTH_LENENC | CLIENT_LOCAL_FILES | CLIENT_COMPRESS)
        if srv.session_track:
            caps |= CLIENT_SESSION_TRACK
        if srv.deprecate_eof:
            caps |= CLIENT_DEPRECATE_EOF
        if srv.tls_ctx is not None:
            caps |= CLIENT_SSL
        if not srv.mariadb:
            caps |= CLIENT_LONG_PASSWORD
        g = (b"\x0a" + srv.version.encode() + b"\x00" + struct.pack("<I", self.id) + salt[:8]
             + b"\x00" + struct.pack("<H", caps & 0xFFFF) + bytes([45])
             + struct.pack("<H", ST_AUTOCOMMIT) + struct.pack("<H", caps >> 16)
             + bytes([21]) + b"\x00" * 6 + struct.pack("<I", srv.ext_caps if srv.mariadb else 0)
             + salt[8:] + b"\x00" + srv.plugin.encode() + b"\x00")
        await self.send([g], seq=0)
        seq, resp = await read_pkt(self.r)
        ccaps = int.from_bytes(resp[:4], "little")
        if ccaps & CLIENT_SSL and len(resp) == 32:
            if srv.tls_ctx is None:
                raise ConnectionError("client wants TLS")
            tls = TLSStream(self.r, self.w, srv.tls_ctx)
            try:
                await tls.handshake()
            except ssl.SSLError as e:
                raise ConnectionError("TLS handshake failed: %s" % e)
            self.r = self.w = tls
            self.tls = True
            seq, resp = await read_pkt(self.r)
            ccaps = int.from_bytes(resp[:4], "little")
        if srv.require_tls and not self.tls:
            await self.send([self.err_payload(1045, "TLS required", "28000")], seq + 1)
            raise ConnectionError("tls required")
        self.caps = ccaps & caps
        self.ext = int.from_bytes(resp[28:32], "little")
        self.charset = resp[8]
        pos = resp.index(b"\x00", 32)
        self.user = resp[32:pos].decode()
        pos += 1
        if ccaps & CLIENT_PLUGIN_AUTH_LENENC:
            n = resp[pos]
            pos += 1
        else:
            n = resp[pos]
            pos += 1
        token = resp[pos:pos + n]
        pos += n
        if ccaps & CLIENT_CONNECT_WITH_DB:
            end = resp.index(b"\x00", pos)
            self.db = resp[pos:end].decode() or None
            pos = end + 1
        srv.logins.append((self.id, self.user, self.db, self.tls, ccaps))
        password = srv.users.get(self.user)
        if srv.auth_switch:
            salt = bytes((b % 94) + 33 for b in os.urandom(20))
            seq = await self.send([b"\xfe" + b"mysql_native_password\x00" + salt + b"\x00"], seq + 1)
            seq, token = await read_pkt(self.r)
        if password is None or token != native_token(password, salt):
            await self.send([self.err_payload(1045, "Access denied for user '%s'" % self.user, "28000")],
                            seq + 1)
            raise ConnectionError("access denied")
        if self.db is not None and self.db not in srv.databases:
            await self.send([self.err_payload(1049, "Unknown database", "42000")], seq + 1)
            raise ConnectionError("unknown db")
        await self.send([self.ok_payload()], seq + 1)

    # -- queries ------------------------------------------------------------------
    async def query(self, sql):
        srv = self.server
        srv.queries.append((self.id, sql))
        stmts = [s for s in sql.split(";") if s.strip()] if ";" in sql else [sql]
        if len(stmts) > 1 and not self.caps & CLIENT_MULTI_STATEMENTS:
            await self.send([self.err_payload(1064, "You have an error in your SQL syntax", "42000")])
            return
        if not stmts:
            await self.send([self.err_payload(1065, "Query was empty", "42000")])
            return
        seq = 1
        for i, st in enumerate(stmts):
            more = i < len(stmts) - 1
            payloads, stop = await self.statement(st.strip(), more, seq)
            if payloads is None:                         # statement wrote by itself
                seq = stop
                if not more:
                    return
                continue
            seq = await self.send(payloads, seq)
            if stop:
                return

    async def statement(self, st, more, seq):
        srv = self.server
        low = st.lower()
        if srv.delay:
            await asyncio.sleep(srv.delay)
        if low in ("begin", "start transaction"):
            self.in_trans = True
            return [self.ok_payload(more=more)], False
        if low in ("commit", "rollback"):
            self.in_trans = False
            return [self.ok_payload(more=more)], False
        m = re.match(r"set\s+autocommit\s*=\s*(\d)", low)
        if m:
            self.autocommit = m.group(1) == "1"
            if self.autocommit:
                self.in_trans = False
            self.sets.append(st)
            return [self.ok_payload(more=more)], False
        if low.startswith("set "):
            self.sets.append(st)
            return [self.ok_payload(more=more)], False
        m = re.match(r"use\s+`?(\w+)`?", low)
        if m:
            if m.group(1) not in srv.databases:
                return [self.err_payload(1049, "Unknown database", "42000")], True
            self.db = m.group(1)
            return [self.ok_payload(more=more)], False
        m = re.match(r"load\s+data\s+local\s+infile\s+'([^']*)'", low)
        if m:
            seq = await self.send([b"\xfb" + m.group(1).encode()], seq)
            total = 0
            while True:
                seq, data = await read_pkt(self.r)
                seq += 1
                if not data:
                    break
                total += len(data)
                srv.infile_data.append(data)
            seq = await self.send([self.ok_payload(affected=total, more=more)], seq)
            return None, seq
        if low.startswith(("insert", "update", "delete", "replace", "create", "drop", "alter",
                           "call", "truncate", "lock", "unlock", "do ")):
            if srv.read_only:
                return [self.err_payload(1290, "The MariaDB server is running with the "
                                               "--read-only option", "HY000")], True
            if not self.autocommit:
                self.in_trans = True
            srv.insert_id += 1
            return [self.ok_payload(affected=1, insert_id=srv.insert_id, more=more)], False
        m = re.match(r"select\s+rows\s+(\d+)(?:\s+(\d+))?", low)
        if m:
            n = int(m.group(1))
            width = int(m.group(2) or 16)
            key = (n, width, self.deof, more)
            cached = srv.cache.get(key)
            if cached is None:
                rows = [(str(i), "v" * width, None if i % 7 == 0 else "w%d" % i) for i in range(n)]
                cached = self.resultset(["id", "val", "opt"], rows, more)
                srv.cache[key] = cached
            return cached, False
        m = re.match(r"select\s+slow\s+([\d.]+)", low)
        if m:
            await asyncio.sleep(float(m.group(1)))
            return self.resultset(["slow"], [(srv.name,)], more), False
        if low.startswith("select errmid"):
            rs = self.resultset(["a"], [("1",), ("2",)])[:-1]
            return rs + [self.err_payload(1317, "Query execution was interrupted", "70100")], True
        if low.startswith("select fail"):
            return [self.err_payload(1146, "Table 'x' doesn't exist", "42S02")], True
        if re.match(r"show\s+(all\s+)?(slave|replica)s?\s+status", low):
            if srv.lag_error:
                return [self.err_payload(1227, "Access denied; you need REPLICA MONITOR", "42000")], True
            if srv.lag == "none":
                return self.resultset(["Slave_IO_Running", "Seconds_Behind_Master"], [], more), False
            return self.resultset(["Slave_IO_Running", "Seconds_Behind_Master"],
                                  [("Yes", None if srv.lag is None else str(srv.lag))], more), False
        if "@@" in low:
            return self.resultset(["v"], [("x",)], more), False
        if low.startswith(("select", "show", "desc", "explain", "with", "(")):
            return self.resultset(["server", "db", "query"], [(srv.name, self.db, st)], more), False
        return [self.ok_payload(more=more)], False

    async def prepare(self, sql):
        sid = self.next_stmt
        self.next_stmt += 1
        nparams = sql.count("?")
        ncols = 1 if sql.lower().lstrip().startswith("select") else 0
        self.stmts[sid] = (sql, nparams, ncols)
        out = [b"\x00" + struct.pack("<IHHBH", sid, ncols, nparams, 0, 0)]
        if nparams:
            out += [coldef("?")] * nparams
            if not self.deof:
                out.append(self.eof_payload())
        if ncols:
            out.append(coldef("c"))
            if not self.deof:
                out.append(self.eof_payload())
        await self.send(out)

    async def execute(self, p):
        sid = int.from_bytes(p[1:5], "little")
        st = self.stmts.get(sid)
        if st is None:
            await self.send([self.err_payload(1243, "Unknown prepared statement handler")])
            return
        sql, _, ncols = st
        self.server.queries.append((self.id, "EXECUTE " + sql))
        if ncols:
            out = [lenenc_int(1), coldef("c")]
            if not self.deof:
                out.append(self.eof_payload())
            out.append(b"\x00\x00" + lenenc_str(self.server.name))     # binary row
            out.append(self.terminator())
            await self.send(out)
        else:
            self.server.insert_id += 1
            await self.send([self.ok_payload(affected=1, insert_id=self.server.insert_id)])


class FakeMySQLServer:
    def __init__(self, name, users=None, databases=("mrpg_shared", "other"), deprecate_eof=True,
                 session_track=True, mariadb=True, ext_caps=0x04, read_only=False, tls_ctx=None,
                 require_tls=False, plugin="mysql_native_password", auth_switch=False,
                 long_terminator=False, version="11.8.9-MariaDB-fake"):
        self.name = name
        self.users = dict(users or {"game": "gamepw", "bridge": "bridgepw", "nopw": ""})
        self.databases = set(databases)
        self.deprecate_eof = deprecate_eof
        self.session_track = session_track
        self.mariadb = mariadb
        self.ext_caps = ext_caps
        self.read_only = read_only
        self.tls_ctx = tls_ctx
        self.require_tls = require_tls
        self.plugin = plugin
        self.auth_switch = auth_switch
        self.long_terminator = long_terminator
        self.version = version
        self.queries = []
        self.commands = []
        self.logins = []
        self.infile_data = []
        self.sessions = []
        self.cache = {}
        self.insert_id = 0
        self.delay = 0.0
        self.lag = 0
        self.lag_error = False
        self.active = 0
        self.total = 0
        self.server = None
        self.port = None

    async def start(self, host="127.0.0.1", port=0):
        self.server = await asyncio.start_server(self._client, host, port)
        self.port = self.server.sockets[0].getsockname()[1]
        return self

    async def _client(self, reader, writer):
        self.total += 1
        self.active += 1
        s = Session(self, reader, writer, self.total)
        self.sessions.append(s)
        await s.run()

    def kill_all(self):
        for s in self.sessions:
            try:
                s.w.transport.abort()
            except Exception:
                pass

    async def stop(self):
        if self.server is not None:
            self.server.close()
            self.kill_all()
            try:
                await asyncio.wait_for(self.server.wait_closed(), 2)
            except Exception:
                pass
            self.server = None

    def queries_matching(self, text):
        return [q for _, q in self.queries if text.lower() in q.lower()]


# --- minimal client ------------------------------------------------------------
class ClientError(Exception):
    def __init__(self, code, msg):
        super().__init__("%d: %s" % (code, msg))
        self.code = code
        self.msg = msg


class MiniClient:
    """Small strict client: validates sequence numbers and framing."""

    def __init__(self):
        self.r = self.w = None
        self.caps = 0
        self.status = 0
        self.greeting_caps = 0

    async def connect(self, port, user="game", password="gamepw", db="mrpg_shared",
                      deprecate_eof=True, mariadb=True, multi_statements=False,
                      session_track=True, charset=45, host="127.0.0.1", local_files=True,
                      request_ssl=False, compress=False, ext_caps=0):
        self.r, self.w = await asyncio.open_connection(host, port)
        seq, g = await read_pkt(self.r)
        if g[:1] == b"\xff":
            raise ClientError(int.from_bytes(g[1:3], "little"), g[3:].decode(errors="replace"))
        nul = g.index(b"\x00", 1)
        pos = nul + 1 + 4
        salt = g[pos:pos + 8]
        pos += 9
        caps = int.from_bytes(g[pos:pos + 2], "little")
        caps |= int.from_bytes(g[pos + 5:pos + 7], "little") << 16
        pos += 7
        auth_len = g[pos]
        pos += 1 + 10
        salt += g[pos:pos + max(13, auth_len - 8)].rstrip(b"\x00")
        self.greeting_caps = caps
        want = (CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS | CLIENT_SECURE_CONNECTION
                | CLIENT_MULTI_RESULTS | CLIENT_PS_MULTI_RESULTS | CLIENT_PLUGIN_AUTH)
        if not mariadb:
            want |= CLIENT_LONG_PASSWORD
        if deprecate_eof:
            want |= CLIENT_DEPRECATE_EOF
        if session_track:
            want |= CLIENT_SESSION_TRACK
        if multi_statements:
            want |= CLIENT_MULTI_STATEMENTS
        if local_files:
            want |= CLIENT_LOCAL_FILES
        if db:
            want |= CLIENT_CONNECT_WITH_DB
        if request_ssl:
            want |= CLIENT_SSL
        if compress:
            want |= CLIENT_COMPRESS
        self.caps = want & (caps | CLIENT_SSL | CLIENT_COMPRESS)
        self.ext_caps = ext_caps if mariadb else 0
        token = native_token(password, salt[:20])
        head = bytearray(struct.pack("<IIB", self.caps, 1 << 24, charset) + b"\x00" * 23)
        # extended MariaDB capability flags live in bytes 28..31
        struct.pack_into("<I", head, 28, self.ext_caps)
        head = bytes(head)
        if request_ssl:
            data, _ = pkt(1, head)
            self.w.write(data)
            await self.w.drain()
            return
        resp = head + user.encode() + b"\x00" + bytes([len(token)]) + token
        if db:
            resp += db.encode() + b"\x00"
        resp += b"mysql_native_password\x00"
        data, _ = pkt(1, resp)
        self.w.write(data)
        await self.w.drain()
        expect = 2
        while True:
            seq, p = await read_pkt(self.r)
            assert seq == expect, "auth seq %d != %d" % (seq, expect)
            if p[:1] == b"\x00":
                break
            if p[:1] == b"\xff":
                raise ClientError(int.from_bytes(p[1:3], "little"), p[9:].decode(errors="replace"))
            if p[:1] == b"\xfe":
                nul = p.index(b"\x00", 1)
                nsalt = p[nul + 1:].rstrip(b"\x00")
                data, _ = pkt(seq + 1, native_token(password, nsalt[:20]))
                self.w.write(data)
                await self.w.drain()
                expect = seq + 2
                continue
            raise AssertionError("unexpected auth packet %r" % p[:1])
        return self

    @property
    def deof(self):
        return bool(self.caps & CLIENT_DEPRECATE_EOF)

    async def send_command(self, payload):
        data, _ = pkt(0, payload)
        self.w.write(data)
        await self.w.drain()

    async def query(self, sql, infile_data=None):
        await self.send_command(b"\x03" + (sql.encode() if isinstance(sql, str) else sql))
        return await self.read_results(infile_data=infile_data)

    async def read_results(self, infile_data=None, binary=False):
        results = []
        expect = 1
        while True:
            seq, p = await read_pkt(self.r)
            assert seq == expect & 0xFF, "seq %d != %d" % (seq, expect & 0xFF)
            expect += 1
            if p[:1] == b"\xff":
                code = int.from_bytes(p[1:3], "little")
                results.append(("err", code, p[9:].decode(errors="replace")))
                return results
            if p[:1] == b"\x00":
                pos = 1
                aff, pos = _dec_lenenc(p, pos)
                iid, pos = _dec_lenenc(p, pos)
                status = int.from_bytes(p[pos:pos + 2], "little")
                self.status = status
                results.append(("ok", aff, iid, status))
                if not status & ST_MORE:
                    return results
                continue
            if p[:1] == b"\xfb":
                data = infile_data or b""
                out = bytearray()
                s = expect
                for i in range(0, len(data), 16384):
                    chunk, s = pkt(s, data[i:i + 16384])
                    out += chunk
                chunk, s = pkt(s, b"")
                out += chunk
                self.w.write(bytes(out))
                await self.w.drain()
                expect = s
                continue
            ncols, _ = _dec_lenenc(p, 0)
            cols = []
            for _ in range(ncols):
                seq, c = await read_pkt(self.r)
                assert seq == expect & 0xFF
                expect += 1
                pos = 0
                for _ in range(4):
                    n, pos = _dec_lenenc(c, pos)
                    pos += n
                n, pos = _dec_lenenc(c, pos)
                cols.append(c[pos:pos + n].decode())
            if not self.deof:
                seq, e = await read_pkt(self.r)
                assert seq == expect & 0xFF and e[:1] == b"\xfe"
                expect += 1
            rows = []
            while True:
                seq, rp = await read_pkt(self.r)
                assert seq == expect & 0xFF, "row seq %d != %d" % (seq, expect & 0xFF)
                expect += 1
                if rp[:1] == b"\xff":
                    code = int.from_bytes(rp[1:3], "little")
                    results.append(("rows", cols, rows, 0))
                    results.append(("err", code, rp[9:].decode(errors="replace")))
                    return results
                if rp[:1] == b"\xfe" and (self.deof or len(rp) < 9) and len(rp) < 0xFFFFFF:
                    if self.deof:
                        pos = 1
                        _, pos = _dec_lenenc(rp, pos)
                        _, pos = _dec_lenenc(rp, pos)
                        status = int.from_bytes(rp[pos:pos + 2], "little")
                    else:
                        status = int.from_bytes(rp[3:5], "little")
                    self.status = status
                    break
                if binary:
                    rows.append([rp])
                    continue
                row, pos = [], 0
                for _ in range(ncols):
                    if rp[pos] == 0xFB:
                        row.append(None)
                        pos += 1
                        continue
                    n, pos = _dec_lenenc(rp, pos)
                    row.append(rp[pos:pos + n].decode())
                    pos += n
                assert pos == len(rp), "row framing"
                rows.append(row)
            results.append(("rows", cols, rows, status))
            if not status & ST_MORE:
                return results

    async def rows(self, sql):
        res = await self.query(sql)
        for r in res:
            if r[0] == "err":
                raise ClientError(r[1], r[2])
        return [r for r in res if r[0] == "rows"][-1][2]

    async def prepare(self, sql):
        await self.send_command(b"\x16" + sql.encode())
        seq, p = await read_pkt(self.r)
        if p[:1] == b"\xff":
            raise ClientError(int.from_bytes(p[1:3], "little"), p[9:].decode())
        sid, ncols, nparams = struct.unpack("<IHH", p[1:9])
        for _ in range(nparams):
            await read_pkt(self.r)
        if nparams and not self.deof:
            await read_pkt(self.r)
        for _ in range(ncols):
            await read_pkt(self.r)
        if ncols and not self.deof:
            await read_pkt(self.r)
        return sid

    async def execute(self, sid):
        await self.send_command(b"\x17" + struct.pack("<IBI", sid, 0, 1))
        return await self.read_results(binary=True)

    async def ping(self):
        await self.send_command(b"\x0e")
        seq, p = await read_pkt(self.r)
        return p[:1] == b"\x00"

    async def close(self):
        if self.w is None:
            return
        try:
            await self.send_command(b"\x01")
        except Exception:
            pass
        self.w.close()
        self.w = None


def _dec_lenenc(d, pos):
    b = d[pos]
    if b < 0xFB:
        return b, pos + 1
    if b == 0xFC:
        return int.from_bytes(d[pos + 1:pos + 3], "little"), pos + 3
    if b == 0xFD:
        return int.from_bytes(d[pos + 1:pos + 4], "little"), pos + 4
    return int.from_bytes(d[pos + 1:pos + 9], "little"), pos + 9


async def _serve_forever(names):
    servers = []
    for name in names:
        srv = await FakeMySQLServer(name, read_only=(name == "replica")).start()
        servers.append(srv)
    print(" ".join(str(s.port) for s in servers), flush=True)
    await asyncio.Event().wait()


if __name__ == "__main__":                                   # used by bench_bridge.py
    import sys
    try:
        asyncio.run(_serve_forever(sys.argv[1:] or ["primary", "replica"]))
    except KeyboardInterrupt:
        pass
