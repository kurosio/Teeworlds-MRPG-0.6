#!/usr/bin/env python3
"""
MySQL/MariaDB bridge for Teeworlds-MRPG servers.

Routes read-only queries (SELECT/SHOW/DESCRIBE/EXPLAIN) to a local MariaDB
replica, everything else to the central primary over the WAN. A single
central database can serve multiple game servers in different locations
with minimal added latency.

    game server (location A) --\
    game server (location B) ---+--> local bridge --> [local replica] --\
    game server (location C) --/    (this script)                       +--> replication --> CENTRAL PRIMARY
                                                                      --/

Pure Python 3 standard library (asyncio). uvloop is used automatically if
installed (optional - gives ~2-4x throughput on hot loops).
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

try:                                                   # optional speed-up
    import uvloop  # type: ignore
    uvloop.install()
    _UVLOOP = True
except Exception:
    _UVLOOP = False

log = logging.getLogger("mysql_bridge")

# --- MySQL/MariaDB wire protocol constants ----------------------------------
COM_QUIT = 0x01
COM_QUERY = 0x03
COM_PING = 0x0E
SERVER_MORE_RESULTS_EXISTS = 0x08

# Note: in MariaDB protocol bit 0 (0x01) is CLIENT_MYSQL. When 0, the server
# enables MariaDB-specific extended capabilities in handshake bytes 28..31.
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
MIRRORED_CAPS = (CLIENT_LONG_PASSWORD | CLIENT_DEPRECATE_EOF | CLIENT_MULTI_STATEMENTS
                 | CLIENT_MULTI_RESULTS | CLIENT_PS_MULTI_RESULTS | CLIENT_SESSION_TRACK)

# "Reads" that must never run on a replica (file writes / row locks):
UNSAFE_READ_RE = re.compile(
    r"\b(into\s+(outfile|dumpfile)|load_file\s*\(|lock\s+tables|for\s+(update|share)\b)",
    re.IGNORECASE,
)

NET_ERRORS = (asyncio.IncompleteReadError, ConnectionResetError, BrokenPipeError, OSError)

CLIENT_IDLE_TIMEOUT = 600      # s; close game-server connections idle this long
PRIMARY_CONNECT_TIMEOUT = 5    # s; fail fast if the WAN primary is slow
MAX_PACKET = 64 * 1024 * 1024
WRITE_HIGH_WATERMARK = 2 * 1024 * 1024
HEALTH_CHECK_IDLE = 5.0        # s; ping pooled conns idle longer than this

# Pre-compiled read-keyword detection as a byte-level trie (one pass, no decode)
# Keywords: select show describe desc explain
#
# strip_comments_and_ws() returns the SQL payload with leading whitespace,
# comments and opening parentheses removed; then we lowercase-compare the
# first bytes against these keyword prefixes.
_READ_PREFIXES = (b"select", b"show", b"describe", b"desc", b"explain")
_WRITE_PREFIXES = (b"insert", b"update", b"delete", b"replace")

STATS = {"replica": 0, "primary": 0, "fallback": 0, "clients": 0, "errors": 0}


# --- Socket tuning -----------------------------------------------------------
def _tune_socket(writer, keepalive=False):
    sock = writer.get_extra_info("socket")
    if sock is None:
        return
    try:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except Exception:
        pass
    if keepalive:
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            if hasattr(socket, "TCP_KEEPIDLE"):
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 30)
            if hasattr(socket, "TCP_KEEPINTVL"):
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 10)
            if hasattr(socket, "TCP_KEEPCNT"):
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)
        except Exception:
            pass


def _safe(coro, what):
    """Fire-and-forget wrapper that never leaks a traceback."""
    async def runner():
        try:
            await coro
        except asyncio.CancelledError:
            raise
        except NET_ERRORS:
            log.debug("%s: peer closed", what)
        except Exception as e:
            log.warning("%s ended with %s: %s", what, type(e).__name__, e)
    return asyncio.ensure_future(runner())


async def stats_loop():
    while True:
        await asyncio.sleep(30)
        log.info("stats replica=%d primary=%d fallback=%d errors=%d clients=%d",
                 STATS["replica"], STATS["primary"], STATS["fallback"],
                 STATS["errors"], STATS["clients"])


PASSWORD_RE = re.compile(r"(identified\s+by\s+(?:password\s+)?)[\x22\x27][^\x22\x27]*[\x22\x27]",
                         re.IGNORECASE)


def log_sql(sql_text, limit=120):
    text = " ".join(sql_text.split())
    text = PASSWORD_RE.sub(lambda mo: mo.group(1) + "'***'", text)
    return text[:limit] + ("..." if len(text) > limit else "")


# --- Packet IO --------------------------------------------------------------
async def read_packet(reader):
    """Read one MySQL packet (joining 16 MB continuation packets if any)."""
    header = await reader.readexactly(4)
    length = header[0] | (header[1] << 8) | (header[2] << 16)
    seq = header[3]
    if length < 0xFFFFFF:
        payload = await reader.readexactly(length)
        return seq, payload
    # Rare path: large payload split across 16 MB chunks.
    parts = [await reader.readexactly(length)]
    total = length
    while True:
        header = await reader.readexactly(4)
        length = header[0] | (header[1] << 8) | (header[2] << 16)
        total += length
        if total > MAX_PACKET:
            raise ValueError("packet exceeds safety limit")
        parts.append(await reader.readexactly(length))
        if length < 0xFFFFFF:
            break
    return seq, b"".join(parts)


def _encode_packet(seq, payload, out):
    """Append a MySQL packet to bytearray *out*, splitting at 16 MB boundaries."""
    offset = 0
    n = len(payload)
    if n == 0:
        out += b"\x00\x00\x00" + bytes([seq & 0xFF])
        return (seq + 1) & 0xFF
    while offset < n:
        chunk = payload[offset:offset + 0xFFFFFF]
        clen = len(chunk)
        out += clen.to_bytes(3, "little")
        out += bytes([seq & 0xFF])
        out += chunk
        seq = (seq + 1) & 0xFF
        offset += clen
        if clen < 0xFFFFFF:
            break
    return seq


async def write_packet(writer, seq, payload):
    """Write one MySQL packet. Batches internally for continuation packets."""
    buf = bytearray()
    _encode_packet(seq, payload, buf)
    writer.write(bytes(buf))
    await writer.drain()


async def write_packets_batched(writer, packets, start_seq=1):
    """Write a sequence of packets in a single socket write (big latency win
    for result sets: 1 syscall / 1 drain() instead of N)."""
    buf = bytearray()
    seq = start_seq & 0xFF
    for _, payload in packets:
        seq = _encode_packet(seq, payload, buf)
    writer.write(bytes(buf))
    # drain() is cheap when under the high-water mark
    await writer.drain()


# --- lenenc parsing ---------------------------------------------------------
def _lenenc_len(data, pos):
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
    pos = 0
    for _ in range(col_count):
        r = _lenenc_len(payload, pos)
        if r is None:
            pos += 1
            continue
        length, pos = r
        if pos + length > len(payload):
            return False
        pos += length
    return pos == len(payload)


def _is_ok_packet(payload):
    if not payload or payload[0] != 0x00:
        return False
    pos = 1
    r = _lenenc_len(payload, pos)
    if r is None:
        return False
    _, pos = r
    r = _lenenc_len(payload, pos)
    if r is None:
        return False
    _, pos = r
    return (len(payload) - pos) >= 4


def _is_terminator(payload, col_count, deprecate_eof=False):
    if not payload:
        return True
    if payload[0] == 0xFE and len(payload) < 9:
        return True
    if deprecate_eof and _is_ok_packet(payload) and not _parses_as_row(payload, col_count):
        return True
    return False


async def read_command_response(reader, deprecate_eof=False):
    """Read the full response of one COM_* command as list[(seq, payload)]."""
    packets = []
    append = packets.append
    while True:                                          # loop over result sets
        seq, first = await read_packet(reader)
        append((seq, first))
        if not first:
            return packets
        if first[0] in (0x00, 0xFF):                     # OK or ERR: done
            return packets
        r = _lenenc_len(first, 0)
        col_count = r[0] if r else 0
        for _ in range(col_count):
            append(await read_packet(reader))
        append(await read_packet(reader))                # EOF/OK ending columns
        while True:                                      # rows
            p = await read_packet(reader)
            append(p)
            _, payload = p
            if _is_terminator(payload, col_count, deprecate_eof):
                if len(payload) >= 5 and (payload[3] & SERVER_MORE_RESULTS_EXISTS):
                    break                                # more result sets
                return packets


# --- Query classification (byte-level, no decode) ---------------------------
_WS = b" \t\r\n("


def _strip_ws_and_comments(sql):
    """sql: bytes. Returns the suffix starting at the first keyword byte."""
    i = 0
    n = len(sql)
    while i < n:
        c = sql[i]
        if c in _WS:
            i += 1
            continue
        if c == 0x2D and i + 1 < n and sql[i + 1] == 0x2D:         # --
            nl = sql.find(b"\n", i + 2)
            if nl == -1:
                return b""
            i = nl + 1
            continue
        if c == 0x23:                                              # #
            nl = sql.find(b"\n", i + 1)
            if nl == -1:
                return b""
            i = nl + 1
            continue
        if c == 0x2F and i + 1 < n and sql[i + 1] == 0x2A:         # /*
            end = sql.find(b"*/", i + 2)
            if end == -1:
                return b""
            i = end + 2
            continue
        break
    return sql[i:]


def _startswith_ci(buf, prefixes):
    """Case-insensitive ASCII startswith against a tuple of lowercase bytes."""
    for p in prefixes:
        if len(buf) >= len(p) and buf[:len(p)].lower() == p:
            return p
    return None


def classify_query(sql):
    """Return ('read' | 'write' | 'use' | 'other', stripped_lowercased_head)."""
    s = _strip_ws_and_comments(sql)
    if not s:
        return "other", b""
    head = s[:16].lower()
    # 'use <db>'
    if head.startswith(b"use ") or head == b"use":
        return "use", head
    # data-write
    for p in _WRITE_PREFIXES:
        if head.startswith(p):
            return "write", head
    # read
    for p in _READ_PREFIXES:
        if head.startswith(p):
            # excluded: @@ session vars, SHOW VARIABLES, locks, outfile, etc.
            if b"@@" in s[:64]:
                return "other", head
            if head.startswith(b"show") and s[:32].lower().startswith(b"show variables"):
                return "other", head
            if UNSAFE_READ_RE.search(s[:256].decode("latin-1", "replace")):
                return "other", head
            return "read", head
    return "other", head


# --- MariaDB auth -----------------------------------------------------------
def parse_handshake(hs):
    pos = hs.index(b"\x00", 1) + 1 + 4
    salt = hs[pos:pos + 8]
    pos += 8 + 1
    pos += 2 + 1 + 2 + 2
    auth_len = hs[pos] if pos < len(hs) else 0
    pos += 1 + 10
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
    stage1 = hashlib.sha1(password.encode()).digest()
    stage2 = hashlib.sha1(stage1).digest()
    return bytes(a ^ b for a, b in zip(stage1, hashlib.sha1(salt[:20] + stage2).digest()))


async def mariadb_handshake(reader, writer, user, password, database, use_tls,
                            mirrored_caps=0, ext_caps=0, charset=0x21):
    _, hs = await read_packet(reader)
    salt, _ = parse_handshake(hs)
    caps = (CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION
            | CLIENT_PLUGIN_AUTH | (mirrored_caps & MIRRORED_CAPS))
    if database:
        caps |= CLIENT_CONNECT_WITH_DB
    token = native_password_token(password, salt)
    filler = b"\x00" * 19 + struct.pack("<I", ext_caps)
    resp = struct.pack("<IIB", caps, 16 * 1024 * 1024, charset) + filler
    resp += user.encode() + b"\x00" + bytes([len(token)]) + token
    if database:
        resp += database.encode() + b"\x00"
    resp += b"mysql_native_password\x00"
    await write_packet(writer, 1, resp)

    seq, payload = await read_packet(reader)
    if payload and payload[0] == 0xFE and len(payload) > 1:
        nul = payload.index(b"\x00", 1)
        plugin = payload[1:nul].decode(errors="replace")
        new_salt = payload[nul + 1:].rstrip(b"\x00")
        if len(new_salt) < 20:
            new_salt = (new_salt + salt)[:20]
        if plugin != "mysql_native_password":
            writer.close()
            raise ConnectionError("unsupported auth plugin after switch: %s" % plugin)
        token = native_password_token(password, new_salt)
        await write_packet(writer, (seq + 1) & 0xFF, token + b"\x00")
        _, payload = await read_packet(reader)
    if not payload or payload[0] == 0xFF:
        writer.close()
        msg = payload[3:].decode(errors="replace") if payload and len(payload) > 3 else repr(payload)
        raise ConnectionError("auth failed: %s" % msg)


# --- Replica connection pool ------------------------------------------------
class PooledConn:
    __slots__ = ("reader", "writer", "profile", "idle_since", "in_use", "broken")

    def __init__(self, reader, writer, profile):
        self.reader = reader
        self.writer = writer
        self.profile = profile
        self.idle_since = time.monotonic()
        self.in_use = False
        self.broken = False


class ReplicaPool:
    """Pool keyed by (mirrored_caps, ext_caps, charset).

    Avoids the classic "waiter starvation on failed connect" deadlock by
    having `acquire` always notify waiters on release, and by tracking the
    number of outstanding connections precisely.
    """

    def __init__(self, host, port, user, password, database, size, use_tls=False):
        self._cfg = (host, port, user, password, database, use_tls)
        self._size = size
        self._conns = []                       # list[PooledConn]
        self._lock = asyncio.Lock()
        self._cond = asyncio.Condition(self._lock)

    async def _new_conn(self, profile):
        mirrored_caps, ext_caps, charset = profile
        host, port, user, password, database, use_tls = self._cfg
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(host, port, ssl=(True if use_tls else None)), timeout=10)
        _tune_socket(writer)
        try:
            await mariadb_handshake(reader, writer, user, password, database, use_tls,
                                    mirrored_caps=mirrored_caps, ext_caps=ext_caps,
                                    charset=charset)
        except Exception:
            try:
                writer.close()
            except Exception:
                pass
            raise
        return PooledConn(reader, writer, profile)

    async def acquire(self, profile):
        async with self._cond:
            while True:
                for c in self._conns:
                    if not c.in_use and not c.broken and c.profile == profile:
                        c.in_use = True
                        return c
                if len(self._conns) < self._size:
                    # Reserve a slot BEFORE awaiting so no two coroutines over-provision.
                    placeholder = PooledConn(None, None, profile)
                    placeholder.in_use = True
                    placeholder.broken = True   # replaced on success
                    self._conns.append(placeholder)
                    break
                # Pool is full - evict a healthy idle wrong-profile conn if possible
                for i, c in enumerate(self._conns):
                    if not c.in_use and c.profile != profile and not c.broken:
                        try:
                            c.writer.close()
                        except Exception:
                            pass
                        self._conns.pop(i)
                        break
                else:
                    await self._cond.wait()
                    continue
                # loop again - a slot opened up
                continue

        try:
            real = await self._new_conn(profile)
        except Exception:
            async with self._cond:
                self._conns.remove(placeholder)
                self._cond.notify_all()
            raise
        async with self._cond:
            # replace placeholder with the real connection
            idx = self._conns.index(placeholder)
            self._conns[idx] = real
            real.in_use = True
            return real

    async def release(self, conn, healthy=True):
        async with self._cond:
            if not healthy:
                conn.broken = True
                try:
                    conn.writer.close()
                except Exception:
                    pass
                try:
                    self._conns.remove(conn)
                except ValueError:
                    pass
            else:
                conn.in_use = False
                conn.idle_since = time.monotonic()
            self._cond.notify()

    async def _ping(self, conn):
        try:
            await asyncio.wait_for(write_packet(conn.writer, 0, bytes([COM_PING])), timeout=3)
            _, payload = await asyncio.wait_for(read_packet(conn.reader), timeout=3)
            return bool(payload and payload[0] == 0x00)
        except Exception:
            return False

    async def query(self, payload, profile):
        """Run one COM_QUERY on a pooled connection."""
        for attempt in range(2):
            conn = await asyncio.wait_for(self.acquire(profile), timeout=5)
            # Ping conns idle for a while
            if attempt == 0 and (time.monotonic() - conn.idle_since) > HEALTH_CHECK_IDLE:
                if not await self._ping(conn):
                    await self.release(conn, healthy=False)
                    continue
            deprecate_eof = bool(profile[0] & CLIENT_DEPRECATE_EOF)
            try:
                await write_packet(conn.writer, 0, payload)
                packets = await asyncio.wait_for(
                    read_command_response(conn.reader, deprecate_eof=deprecate_eof),
                    timeout=30)
            except Exception:
                await self.release(conn, healthy=False)
                if attempt == 0:
                    continue
                raise
            await self.release(conn, healthy=True)
            return packets

    async def use_db_all(self, db_sql):
        """Broadcast USE db to every idle conn (best-effort)."""
        async with self._cond:
            victims = [c for c in self._conns if not c.in_use and not c.broken]
            for c in victims:
                c.in_use = True
        for c in victims:
            deprecate_eof = bool(c.profile[0] & CLIENT_DEPRECATE_EOF)
            try:
                await write_packet(c.writer, 0, bytes([COM_QUERY]) + db_sql)
                await asyncio.wait_for(
                    read_command_response(c.reader, deprecate_eof=deprecate_eof),
                    timeout=5)
            except Exception:
                await self.release(c, healthy=False)
            else:
                await self.release(c, healthy=True)

    async def close(self):
        async with self._cond:
            for c in self._conns:
                try:
                    c.writer.close()
                except Exception:
                    pass
            self._conns.clear()
            self._cond.notify_all()


# --- Per-client session ------------------------------------------------------
class ClientSession:
    __slots__ = ("authenticated", "last_write", "grace",
                 "client_caps", "client_ext_caps", "client_charset", "primary_lock")

    def __init__(self, read_your_writes_grace):
        self.authenticated = False
        self.last_write = 0.0
        self.grace = read_your_writes_grace
        self.client_caps = 0
        self.client_ext_caps = 0
        self.client_charset = 0x21
        # Serializes writes to the primary socket so c2s and (rare) direct
        # writes from other coroutines never interleave packets.
        self.primary_lock = asyncio.Lock()

    def reads_to_primary(self):
        return (time.monotonic() - self.last_write) < self.grace

    def replica_profile(self):
        return (self.client_caps & MIRRORED_CAPS,
                self.client_ext_caps,
                self.client_charset)


async def handle_client(client_reader, client_writer, primary_cfg, pool, grace,
                        route_replica=True):
    peer = client_writer.get_extra_info("peername")
    _tune_socket(client_writer)
    STATS["clients"] += 1
    host, port, use_tls = primary_cfg
    try:
        pr, pw = await asyncio.wait_for(
            asyncio.open_connection(host, port, ssl=(True if use_tls else None)),
            timeout=PRIMARY_CONNECT_TIMEOUT)
        _tune_socket(pw, keepalive=True)
    except Exception as e:
        log.warning("client %s: cannot reach primary %s:%s: %s", peer, host, port, e)
        try:
            client_writer.close()
        except Exception:
            pass
        return

    log.info("client %s connected -> primary %s:%s (tls=%s)", peer, host, port, bool(use_tls))
    session = ClientSession(grace)

    async def c2s():
        while True:
            seq, payload = await asyncio.wait_for(
                read_packet(client_reader), timeout=CLIENT_IDLE_TIMEOUT)

            # --- Auth phase: just relay client -> primary verbatim ---
            if not session.authenticated:
                if payload and len(payload) >= 32 and not session.client_caps:
                    # HandshakeResponse41:
                    #   [0:4]   capability flags
                    #   [4:8]   max packet size
                    #   [8]     character set
                    #   [9:28]  19 reserved zero bytes
                    #   [28:32] MariaDB extended capability flags
                    session.client_caps = int.from_bytes(payload[:4], "little")
                    session.client_charset = payload[8]
                    session.client_ext_caps = int.from_bytes(payload[28:32], "little")
                async with session.primary_lock:
                    await write_packet(pw, seq, payload)
                continue

            if not payload:
                async with session.primary_lock:
                    await write_packet(pw, seq, payload)
                continue

            cmd = payload[0]
            if cmd == COM_QUIT:
                break

            if cmd == COM_QUERY:
                sql = payload[1:]
                kind, _ = classify_query(sql)

                if kind == "use":
                    # Serialize USE across the replica pool too, so that any
                    # SELECT routed to the replica after this point sees the
                    # correct default database.
                    await pool.use_db_all(sql)

                if (route_replica and kind == "read"
                        and not session.reads_to_primary()):
                    profile = session.replica_profile()
                    packets = None
                    try:
                        packets = await pool.query(payload, profile)
                    except NET_ERRORS as e:
                        log.debug("replica net err: %s", e)
                    except Exception as e:
                        log.warning("replica err (%s): %s",
                                    type(e).__name__, e)

                    if packets is not None:
                        first_payload = packets[0][1]
                        if first_payload and first_payload[0] == 0xFF:
                            # ERR from replica: fall back to primary so the
                            # client gets a correctly-framed error.
                            errno = (int.from_bytes(first_payload[1:3], "little")
                                     if len(first_payload) >= 3 else 0)
                            log.warning("replica ERR errno=%d, falling back", errno)
                            STATS["fallback"] += 1
                        else:
                            STATS["replica"] += 1
                            await write_packets_batched(
                                client_writer, packets, start_seq=seq + 1)
                            continue
                    else:
                        STATS["fallback"] += 1

                if kind == "write":
                    session.last_write = time.monotonic()
                STATS["primary"] += 1
                async with session.primary_lock:
                    await write_packet(pw, seq, payload)
            else:
                async with session.primary_lock:
                    await write_packet(pw, seq, payload)

    async def s2c():
        while True:
            # First packet (handshake) must arrive quickly; a silent primary
            # would otherwise hang the client forever.
            timeout = PRIMARY_CONNECT_TIMEOUT if not session.authenticated else None
            seq, payload = await asyncio.wait_for(read_packet(pr), timeout=timeout)
            await write_packet(client_writer, seq, payload)
            if payload and payload[0] == 0x00 and not session.authenticated:
                session.authenticated = True
                log.info("client %s authenticated (caps=0x%08x ext=0x%08x cs=0x%02x deof=%s)",
                         peer, session.client_caps, session.client_ext_caps,
                         session.client_charset,
                         bool(session.client_caps & CLIENT_DEPRECATE_EOF))

    t1 = _safe(c2s(), "c2s")
    t2 = _safe(s2c(), "s2c")
    try:
        done, pending = await asyncio.wait({t1, t2}, return_when=asyncio.FIRST_COMPLETED)
        for p in pending:
            p.cancel()
        await asyncio.gather(*pending, return_exceptions=True)
    except Exception as e:
        STATS["errors"] += 1
        log.warning("session %s error: %s: %s", peer, type(e).__name__, e)
    finally:
        for w in (client_writer, pw):
            try:
                w.close()
            except Exception:
                pass
        log.info("client %s disconnected", peer)


# --- Server bootstrap -------------------------------------------------------
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
        cfg.getint("bridge", "replica_pool_size", fallback=16),
        use_tls=cfg.getboolean("replica", "use_tls", fallback=False),
    )
    max_clients = cfg.getint("bridge", "max_clients", fallback=256)
    semaphore = asyncio.Semaphore(max_clients)
    grace = cfg.getfloat("bridge", "read_your_writes_grace_sec", fallback=0.0)
    route_replica = cfg.getboolean("bridge", "route_selects_to_replica", fallback=True)

    async def on_client(r, w):
        async with semaphore:
            try:
                await handle_client(r, w, primary, pool, grace, route_replica)
            except Exception as e:
                STATS["errors"] += 1
                log.warning("client handler: %s: %s", type(e).__name__, e)

    listen_host = env("BRIDGE_LISTEN_HOST", cfg.get("bridge", "listen_host", fallback="127.0.0.1"))
    listen_port = int(env("BRIDGE_LISTEN_PORT", cfg.getint("bridge", "listen_port", fallback=33060)))
    hosts = ["127.0.0.1", "::1"] if listen_host == "localhost" else [listen_host]
    servers = []
    for h in hosts:
        try:
            servers.append(await asyncio.start_server(
                on_client, h, listen_port,
                backlog=512))
        except OSError as e:
            log.warning("cannot listen on %s:%s: %s", h, listen_port, e)
    if not servers:
        raise RuntimeError("bridge: no listen address available")
    log.info("bridge listening on %s:%s (uvloop=%s, max_clients=%d, pool=%d)",
             ", ".join(hosts), listen_port, _UVLOOP, max_clients, pool._size)
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
    await asyncio.wait({serve, stopper}, return_when=asyncio.FIRST_COMPLETED)
    log.info("bridge shutting down")
    for s in servers:
        s.close()
    await asyncio.gather(*(s.wait_closed() for s in servers), return_exceptions=True)
    await pool.close()


def main():
    logging.basicConfig(level=os.environ.get("BRIDGE_LOG_LEVEL", "INFO").upper(),
                        format="%(asctime)s %(levelname)s %(message)s")
    cfg_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(__file__), "bridge.cfg")
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