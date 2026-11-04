#!/usr/bin/env python3
"""
MySQL/MariaDB bridge for Teeworlds-MRPG servers.

Lets two or more game servers in different locations work with a single
central database with minimal delay:

    game server (location A) --\
    game server (location B) ---+--> local bridge --> [local replica] --\
    game server (location C) --/    (this script)                       +--> replication --> CENTRAL PRIMARY
                                                                      --/

The bridge relays the MySQL wire protocol to the primary for writes and
answers read-only queries from the local replica, so cross-location
traffic is only writes plus replication lag.

Pure Python 3 standard library (asyncio) - no external dependencies.
"""

import asyncio
import configparser
import hashlib
import logging
import os
import re
import signal
import socket
import struct
import sys
import time

log = logging.getLogger("mysql_bridge")

# --- MySQL/MariaDB wire protocol constants -------------------------------
COM_QUIT = 0x01
COM_QUERY = 0x03
COM_PING = 0x0E
SERVER_MORE_RESULTS_EXISTS = 0x08

CLIENT_LONG_PASSWORD = 0x00000001
CLIENT_CONNECT_WITH_DB = 0x00000008
CLIENT_PROTOCOL_41 = 0x00000200
CLIENT_SSL = 0x00000800
CLIENT_SECURE_CONNECTION = 0x00008000
CLIENT_PLUGIN_AUTH = 0x00080000
CLIENT_DEPRECATE_EOF = 0x01000000
CLIENT_MULTI_STATEMENTS = 0x00010000
CLIENT_MULTI_RESULTS = 0x00020000
CLIENT_PS_MULTI_RESULTS = 0x00040000
CLIENT_SESSION_TRACK = 0x00800000
# Capability flags mirrored from the client into replica connections so the
# response framing (EOF vs OK packets) always matches what the client expects.
MIRRORED_CAPS = (CLIENT_DEPRECATE_EOF | CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS
                 | CLIENT_PS_MULTI_RESULTS | CLIENT_SESSION_TRACK)

READ_KEYWORDS = ("select", "show", "describe", "desc", "explain")
DATA_WRITE_KEYWORDS = ("insert", "update", "delete", "replace")

# "Reads" that must never run on a replica (file writes / locks):
UNSAFE_READ_RE = re.compile(
    r"\b(into\s+(outfile|dumpfile)|load_file\s*\(|lock\s+tables|for\s+(update|share)\b)",
    re.IGNORECASE,
)

NET_ERRORS = (asyncio.IncompleteReadError, ConnectionResetError, BrokenPipeError, OSError)

CLIENT_IDLE_TIMEOUT = 600    # s; close game-server connections idle this long
PRIMARY_CONNECT_TIMEOUT = 5  # s; fail fast if the WAN primary is slow
MAX_PACKET = 64 * 1024 * 1024

STATS = {"replica": 0, "primary": 0, "fallback": 0, "clients": 0}


def _nodelay(writer):
    try:
        writer.get_extra_info("socket").setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except Exception:
        pass


def _safe(coro, what):
    """Fire-and-forget task wrapper: no exception ever becomes a traceback."""
    async def runner():
        try:
            await coro
        except asyncio.CancelledError:
            raise
        except NET_ERRORS:
            log.debug("%s: peer closed connection", what)
        except Exception as e:
            log.warning("%s ended with %s: %s", what, type(e).__name__, e)
    return asyncio.ensure_future(runner())


async def stats_loop():
    while True:
        await asyncio.sleep(30)
        log.info("stats: replica(local)=%d primary(wan)=%d fallback=%d clients=%d",
                 STATS["replica"], STATS["primary"], STATS["fallback"], STATS["clients"])


PASSWORD_RE = re.compile(r"(identified\s+by\s+(?:password\s+)?)[\x22\x27][^\x22\x27]*[\x22\x27]",
                         re.IGNORECASE)


def log_sql(sql_bytes, limit=120):
    try:
        text = sql_bytes.decode("utf-8", "replace")
    except Exception:
        return "<binary>"
    text = " ".join(text.split())
    text = PASSWORD_RE.sub(lambda mo: mo.group(1) + "'***'", text)  # never log credentials
    return text[:limit] + ("..." if len(text) > limit else "")


# --- Packet IO ------------------------------------------------------------
async def read_packet(reader):
    """Read one MySQL packet (handles 16MB continuation packets)."""
    payload = b""
    while True:
        header = await reader.readexactly(4)
        length = header[0] | (header[1] << 8) | (header[2] << 16)
        if len(payload) + length > MAX_PACKET:
            raise ValueError("packet exceeds safety limit")
        payload += await reader.readexactly(length)
        if length < 0xFFFFFF:
            return header[3], payload


async def write_packet(writer, seq, payload):
    """Write one MySQL packet (splits payloads >= 16MB)."""
    offset = 0
    while True:
        chunk = payload[offset:offset + 0xFFFFFF]
        writer.write(len(chunk).to_bytes(3, "little") + bytes([seq & 0xFF]) + chunk)
        await writer.drain()
        offset += len(chunk)
        seq += 1
        if len(chunk) < 0xFFFFFF:
            break


def _lenenc_len(data, pos):
    """Length of a lenenc integer at pos; returns (value, next_pos) or None."""
    if pos >= len(data):
        return None
    b = data[pos]
    if b < 0xFB:
        return b, pos + 1
    if b == 0xFC:
        return int.from_bytes(data[pos + 1:pos + 3], "little"), pos + 3
    if b == 0xFD:
        return int.from_bytes(data[pos + 1:pos + 4], "little"), pos + 4
    if b == 0xFE:
        return int.from_bytes(data[pos + 1:pos + 9], "little"), pos + 9
    return None  # 0xFB = NULL


def _parses_as_row(payload, col_count):
    """True if payload can be parsed as a text-protocol row with col_count
    lenenc fields. Used to tell data rows apart from OK/EOF terminators."""
    pos = 0
    for _ in range(col_count):
        r = _lenenc_len(payload, pos)
        if r is None:  # NULL column
            pos += 1
            continue
        length, pos = r
        if pos + length > len(payload):
            return False
        pos += length
    return pos == len(payload)


def _is_terminator(payload, col_count):
    if not payload:
        return True
    if payload[0] == 0xFE and len(payload) < 9:  # classic EOF
        return True
    if payload[0] == 0x00 and not _parses_as_row(payload, col_count):  # OK packet
        return True
    return False


async def read_command_response(reader):
    """Read a full command response into a list of (seq, payload) packets.

    OK/ERR is a single packet. A result set is: column-count, column
    definitions, EOF/OK, then rows until a terminating EOF/OK. Handles both
    classic EOF framing and CLIENT_DEPRECATE_EOF (OK terminators), and
    multiple result sets when SERVER_MORE_RESULTS_EXISTS is set.
    """
    packets = []
    while True:  # loop over result sets
        seq, first = await read_packet(reader)
        packets.append((seq, first))
        if not first:
            return packets
        if first[0] in (0x00, 0xFF):  # OK or ERR: response complete
            return packets
        r = _lenenc_len(first, 0)
        col_count = r[0] if r else 0
        for _ in range(col_count):
            packets.append(await read_packet(reader))
        packets.append(await read_packet(reader))  # EOF/OK ending column defs
        while True:  # rows until terminating EOF/OK
            p = await read_packet(reader)
            packets.append(p)
            _, payload = p
            if _is_terminator(payload, col_count):
                if len(payload) >= 5 and (payload[4] & SERVER_MORE_RESULTS_EXISTS):
                    break  # another result set follows
                return packets


async def relay_packets(writer, packets):
    # The client expects the response to its COM_QUERY (seq 0) to start at
    # seq 1 - renumber defensively in case the upstream seq drifted.
    for i, (_, payload) in enumerate(packets):
        await write_packet(writer, i + 1, payload)


# --- Query classification ---------------------------------------------------
def strip_comments_and_ws(sql):
    sql = sql.lstrip(" \t\r\n(")
    while sql.startswith(("--", "/*", "#")):
        if sql.startswith("/*"):
            end = sql.find("*/")
            if end == -1:
                return ""
            sql = sql[end + 2:].lstrip(" \t\r\n(")
        else:
            end = sql.find("\n")
            if end == -1:
                return ""
            sql = sql[end + 1:].lstrip(" \t\r\n(")
    return sql


def is_read_query(sql_bytes):
    try:
        sql = sql_bytes.decode("utf-8", "replace").lower()
    except Exception:
        return False
    sql = strip_comments_and_ws(sql)
    if not sql.startswith(READ_KEYWORDS) or UNSAFE_READ_RE.search(sql):
        return False
    # Session/system variables must come from the primary.
    if "@@" in sql or sql.startswith("show variables"):
        return False
    return True


def is_data_write(sql_bytes):
    """True only for data-changing statements (they trigger the read-your-writes
    grace window). SET/USE/etc. do NOT."""
    try:
        sql = sql_bytes.decode("utf-8", "replace").lower()
    except Exception:
        return False
    return strip_comments_and_ws(sql).startswith(DATA_WRITE_KEYWORDS)


def is_use_db(sql_bytes):
    try:
        sql = sql_bytes.decode("utf-8", "replace")
    except Exception:
        return False
    return strip_comments_and_ws(sql.lower()).startswith("use ")


# --- MariaDB auth -----------------------------------------------------------
def parse_handshake(hs):
    """Return (salt[20], auth_plugin_name) from a handshake v10 packet."""
    pos = hs.index(b"\x00", 1) + 1 + 4          # version\0 + thread id
    salt = hs[pos:pos + 8]
    pos += 8 + 1                                  # salt part 1 + filler
    pos += 2 + 1 + 2 + 2                          # caps lo, charset, status, caps hi
    auth_len = hs[pos] if pos < len(hs) else 0
    pos += 1 + 10                                 # salt len + reserved
    salt += hs[pos:pos + 12]
    pos += max(13, auth_len - 8) if auth_len else 13
    plugin = ""
    try:
        end = hs.index(b"\x00", pos)
        plugin = hs[pos:end].decode(errors="replace")
    except ValueError:
        pass
    return salt[:20], plugin


def native_password_token(password, salt):
    """mysql_native_password: SHA1(pw) XOR SHA1(salt + SHA1(SHA1(pw)))."""
    stage1 = hashlib.sha1(password.encode()).digest()
    stage2 = hashlib.sha1(stage1).digest()
    return bytes(a ^ b for a, b in zip(stage1, hashlib.sha1(salt[:20] + stage2).digest()))


async def mariadb_handshake(reader, writer, user, password, database, use_tls,
                            mirrored_caps=0):
    """Authenticate to a MariaDB server; raises ConnectionError on failure."""
    _, hs = await read_packet(reader)
    salt, _ = parse_handshake(hs)
    caps = (CLIENT_LONG_PASSWORD | CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION
            | CLIENT_PLUGIN_AUTH | (mirrored_caps & MIRRORED_CAPS))
    if database:
        caps |= CLIENT_CONNECT_WITH_DB
    token = native_password_token(password, salt)
    resp = struct.pack("<II", caps, 16 * 1024 * 1024) + b"\x21" + b"\x00" * 23
    resp += user.encode() + b"\x00" + bytes([len(token)]) + token
    if database:
        resp += database.encode() + b"\x00"
    resp += b"mysql_native_password\x00"
    await write_packet(writer, 1, resp)

    seq, payload = await read_packet(reader)
    if payload and payload[0] == 0xFE and len(payload) > 1:  # Auth Switch Request
        nul = payload.index(b"\x00", 1)
        plugin = payload[1:nul].decode(errors="replace")
        new_salt = payload[nul + 1:].rstrip(b"\x00")
        if len(new_salt) < 20:
            new_salt = (new_salt + salt)[:20]
        if plugin != "mysql_native_password":
            writer.close()
            raise ConnectionError("unsupported auth plugin after switch: %s "
                                  "(set user to mysql_native_password)" % plugin)
        token = native_password_token(password, new_salt)
        await write_packet(writer, (seq + 1) & 0xFF, token + b"\x00")
        _, payload = await read_packet(reader)
    if not payload or payload[0] == 0xFF:
        writer.close()
        msg = payload[3:].decode(errors="replace") if payload and len(payload) > 3 else repr(payload)
        raise ConnectionError("auth failed: %s" % msg)


# --- Replica connection pool -------------------------------------------------
class ReplicaPool:
    """Pool of persistent, health-checked connections to the read replica.

    Each pooled connection negotiates the same mirrored capability flags as
    the client it serves, so response framing always matches the client.
    """

    def __init__(self, host, port, user, password, database, size, use_tls=False):
        self._cfg = (host, port, user, password, database, use_tls)
        self._idle = asyncio.Queue()  # items: ((reader, writer), caps, idle_since)
        self._size = size
        self._created = 0
        self._lock = asyncio.Lock()

    async def _connect(self, mirrored_caps):
        host, port, user, password, database, use_tls = self._cfg
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(host, port, ssl=(True if use_tls else None)), timeout=10)
        _nodelay(writer)
        try:
            await mariadb_handshake(reader, writer, user, password, database, use_tls,
                                    mirrored_caps=mirrored_caps)
        except Exception:
            writer.close()
            raise
        return (reader, writer), mirrored_caps

    async def acquire(self, mirrored_caps=0):
        async with self._lock:
            if self._created < self._size:
                self._created += 1
                try:
                    conn, flag = await asyncio.wait_for(self._connect(mirrored_caps), timeout=15)
                    return conn, flag, 0.0
                except Exception:
                    self._created -= 1
                    raise
        while True:
            item = await self._idle.get()
            conn, conn_flag, idle_since = item
            if conn_flag != mirrored_caps:
                await self.release(item, healthy=False)
                continue
            # Health-check only connections idle for a while; a freshly used
            # connection is almost certainly still alive.
            if time.monotonic() - idle_since > 5.0:
                reader, writer = conn
                try:
                    await asyncio.wait_for(write_packet(writer, 0, bytes([COM_PING])), timeout=3)
                    _, payload = await asyncio.wait_for(read_packet(reader), timeout=3)
                    if payload and payload[0] == 0x00:
                        return conn, conn_flag, 0.0
                except Exception:
                    pass
                await self.release(item, healthy=False)
            else:
                return conn, conn_flag, 0.0

    async def release(self, item, healthy=True):
        if healthy:
            conn, flag, _ = item
            await self._idle.put((conn, flag, time.monotonic()))
        else:
            try:
                item[0][1].close()
            except Exception:
                pass
            async with self._lock:
                self._created -= 1

    async def query(self, payload, mirrored_caps=0):
        """Run one COM_QUERY on a healthy pooled connection; returns the full
        response as a list of packets. Raises on failure (caller falls back)."""
        item = await asyncio.wait_for(self.acquire(mirrored_caps), timeout=3)
        reader, writer = item[0]
        try:
            await write_packet(writer, 0, payload)
            packets = await asyncio.wait_for(read_command_response(reader), timeout=30)
        except Exception:
            await self.release(item, healthy=False)
            raise
        await self.release(item, healthy=True)
        return packets

    async def use_db_all(self, db_sql):
        try:
            for _ in range(self._idle.qsize()):
                item = await self._idle.get()
                reader, writer = item[0]
                try:
                    await write_packet(writer, 0, bytes([COM_QUERY]) + db_sql)
                    await read_command_response(reader)
                except Exception:
                    await self.release(item, healthy=False)
                else:
                    await self.release(item, healthy=True)
        except Exception as e:
            log.warning("use_db_all failed: %s: %s", type(e).__name__, e)


# --- Per-client session -------------------------------------------------------
class ClientSession:
    def __init__(self, read_your_writes_grace):
        self.authenticated = False
        self.last_write = 0.0
        self.grace = read_your_writes_grace
        self.client_caps = 0

    def reads_to_primary(self):
        return (time.monotonic() - self.last_write) < self.grace


async def handle_client(client_reader, client_writer, primary_cfg, pool, grace,
                        route_replica=True):
    peer = client_writer.get_extra_info("peername")
    _nodelay(client_writer)
    STATS["clients"] += 1
    try:
        pr, pw = await asyncio.wait_for(
            asyncio.open_connection(primary_cfg[0], primary_cfg[1],
                                    ssl=(True if len(primary_cfg) > 2 and primary_cfg[2] else None)),
            timeout=PRIMARY_CONNECT_TIMEOUT)
        _nodelay(pw)
    except Exception as e:
        log.warning("client %s: cannot reach primary %s:%s: %s",
                    peer, primary_cfg[0], primary_cfg[1], e)
        client_writer.close()
        return
    log.info("client %s connected -> primary %s:%s (tls=%s)", peer, primary_cfg[0], primary_cfg[1],
             bool(len(primary_cfg) > 2 and primary_cfg[2]))
    session = ClientSession(grace)

    async def c2s():
        while True:
            seq, payload = await asyncio.wait_for(
                read_packet(client_reader), timeout=CLIENT_IDLE_TIMEOUT)
            if not session.authenticated or not payload:
                if payload and len(payload) >= 4 and not session.client_caps:
                    # Client handshake response: first 4 bytes are capability flags.
                    session.client_caps = struct.unpack("<I", payload[:4])[0]
                await write_packet(pw, seq, payload)
                continue
            cmd = payload[0]
            if cmd == COM_QUIT:
                break
            if cmd == COM_QUERY:
                sql = payload[1:]
                if is_use_db(sql):
                    _safe(pool.use_db_all(sql), "use_db_all")
                if route_replica and not session.reads_to_primary() and is_read_query(sql):
                    # Mirror exactly the client's own capability flags so the
                    # replica's response framing (EOF vs OK terminators)
                    # matches what the client negotiated with the primary.
                    mirrored = session.client_caps & MIRRORED_CAPS
                    packets = None
                    for attempt in range(2):  # retry once on a fresh pooled conn
                        try:
                            packets = await pool.query(payload, mirrored)
                            break
                        except NET_ERRORS as e:
                            log.debug("replica read attempt %d failed (%s: %s): %s",
                                      attempt + 1, type(e).__name__, e, log_sql(sql))
                        except Exception as e:
                            log.warning("replica read failed (%s: %s): %s",
                                        type(e).__name__, e, log_sql(sql))
                            break
                    if packets is not None:
                        STATS["replica"] += 1
                        log.info("LOCAL  replica: %s", log_sql(sql))
                        log.debug("LOCAL reply: %d packets: %s", len(packets),
                                  [(sq, pl[:1].hex(), len(pl)) for sq, pl in packets[:16]])
                        await relay_packets(client_writer, packets)
                        continue
                    STATS["fallback"] += 1
                    log.info("FALLBACK primary: %s", log_sql(sql))
                if is_data_write(sql):
                    session.last_write = time.monotonic()
                STATS["primary"] += 1
                log.info("WAN    primary: %s", log_sql(sql))
                await write_packet(pw, seq, payload)
            else:
                # COM_PING, COM_STMT_*, etc. go to the primary
                log.debug("WAN cmd=0x%02x len=%d", cmd, len(payload))
                await write_packet(pw, seq, payload)

    async def s2p():
        while True:
            # the very first packet (server handshake) must arrive quickly;
            # a blocked/silent primary would otherwise hang the client forever
            timeout = PRIMARY_CONNECT_TIMEOUT if not session.authenticated else None
            seq, payload = await asyncio.wait_for(read_packet(pr), timeout=timeout)
            await write_packet(client_writer, seq, payload)
            if payload and payload[0] == 0x00 and not session.authenticated:
                session.authenticated = True
                log.info("client %s authenticated (client_caps=0x%08x, deprecate_eof=%s)",
                         peer, session.client_caps,
                         bool(session.client_caps & CLIENT_DEPRECATE_EOF))

    try:
        await asyncio.wait(
            [_safe(c2s(), "c2s"), _safe(s2p(), "s2p")],
            return_when=asyncio.FIRST_COMPLETED,
        )
    except Exception as e:
        log.warning("session %s error: %s: %s", peer, type(e).__name__, e)
    finally:
        for w in (client_writer, pw):
            try:
                w.close()
            except Exception:
                pass
        log.info("client %s disconnected", peer)


async def main_async(cfg):
    def env(name, fallback):
        return os.environ.get(name, fallback)

    primary = (
        env("BRIDGE_PRIMARY_HOST", cfg.get("primary", "host")),
        int(env("BRIDGE_PRIMARY_PORT", cfg.getint("primary", "port"))),
        cfg.getboolean("primary", "use_tls", fallback=False),
    )
    pool = ReplicaPool(
        env("BRIDGE_REPLICA_HOST", cfg.get("replica", "host")),
        int(env("BRIDGE_REPLICA_PORT", cfg.getint("replica", "port"))),
        env("BRIDGE_REPLICA_USER", cfg.get("replica", "user")),
        env("BRIDGE_REPLICA_PASSWORD", cfg.get("replica", "password")),
        cfg.get("replica", "database"),
        cfg.getint("bridge", "replica_pool_size", fallback=8),
        use_tls=cfg.getboolean("replica", "use_tls", fallback=False),
    )
    max_clients = cfg.getint("bridge", "max_clients", fallback=64)
    semaphore = asyncio.Semaphore(max_clients)
    grace = cfg.getfloat("bridge", "read_your_writes_grace_sec", fallback=0.0)
    route_replica = cfg.getboolean("bridge", "route_selects_to_replica", fallback=True)

    async def on_client(r, w):
        async with semaphore:
            try:
                await handle_client(r, w, primary, pool, grace, route_replica)
            except Exception as e:
                log.warning("client handler error: %s: %s", type(e).__name__, e)

    listen_host = env("BRIDGE_LISTEN_HOST", cfg.get("bridge", "listen_host", fallback="127.0.0.1"))
    listen_port = int(env("BRIDGE_LISTEN_PORT", cfg.getint("bridge", "listen_port", fallback=33060)))
    # "localhost" may resolve to ::1 (IPv6) on the game server - bind both.
    hosts = ["127.0.0.1", "::1"] if listen_host == "localhost" else [listen_host]
    servers = []
    for h in hosts:
        try:
            servers.append(await asyncio.start_server(on_client, h, listen_port))
        except OSError as e:
            log.warning("cannot listen on %s:%s: %s", h, listen_port, e)
    if not servers:
        raise RuntimeError("bridge: no listen address available")
    log.info("bridge listening on %s:%s (max_clients=%d, replica pool=%d)",
             ", ".join(hosts), listen_port, max_clients, pool._size)
    _safe(stats_loop(), "stats")

    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except (NotImplementedError, RuntimeError):
            pass
    serve = asyncio.gather(*(s.serve_forever() for s in servers))
    stopper = asyncio.ensure_future(stop.wait())
    await asyncio.wait([serve, stopper], return_when=asyncio.FIRST_COMPLETED)
    log.info("bridge shutting down")
    for s in servers:
        s.close()
    await asyncio.gather(*(s.wait_closed() for s in servers), return_exceptions=True)


def main():
    logging.basicConfig(level=os.environ.get("BRIDGE_LOG_LEVEL", "INFO").upper(),
                        format="%(asctime)s %(levelname)s %(message)s")
    cfg_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "bridge.cfg")
    cfg = configparser.ConfigParser()
    cfg.read(cfg_path)
    if not cfg.has_section("bridge"):
        log.error("config %s missing [bridge]/[primary]/[replica] sections", cfg_path)
        sys.exit(1)
    try:
        asyncio.run(main_async(cfg))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()