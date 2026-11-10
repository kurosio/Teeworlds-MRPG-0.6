#!/usr/bin/env python3
"""
MySQL/MariaDB read/write-splitting bridge for Teeworlds-MRPG servers.

The game server uses two connections (see docs/mariadb.md):

    shared  (mrpg_shared) -> this bridge -> SELECTs from the local replica,
                             writes to the central primary over the WAN
    local   (mrpg_local)  -> direct to the local MariaDB, no bridge at all

So the bridge only ever serves the shared database. It routes plain SELECTs
to the replica and everything else to the primary:

    game server (location A) --\
    game server (location B) ---+--> local bridge --> [local replica] --\
    game server (location C) --/    (this script)                       +--> replication --> CENTRAL PRIMARY
                                                                      --/

Design notes
------------
* Every game-server connection gets its own connection to the primary; the
  login is relayed to (and verified by) the primary. Nothing is ever sent to
  the replica before the primary has accepted the login.
* A SELECT is only served by the replica when it is provably equivalent to
  running it on the primary session: a single plain SELECT, no locking
  clause, no session state (SET/USE/transactions/temp tables pin the session
  to the primary), and no recent write (configurable grace window, so a
  SELECT right after an INSERT sees the new row). Everything else goes to
  the primary.
* The primary's responses are parsed incrementally (without copying) to know
  exactly when a response has ended, so reads can never overtake an in-flight
  primary response. Responses are forwarded as raw byte chunks - no per-row
  decode/re-encode.
* Replica failures never break the game: a replica error, timeout, or outage
  falls back to the primary transparently, and a circuit breaker keeps the
  game from waiting on a dead replica.

Pure Python 3.8+ standard library (asyncio). uvloop is used automatically if
installed (optional, gives a noticeably higher throughput).
"""

import argparse
import asyncio
import collections
import configparser
import hashlib
import ipaddress
import logging
import os
import re
import signal
import socket
import ssl
import stat
import struct
import sys
import time

__version__ = "3.0"

log = logging.getLogger("mysql_bridge")
monotonic = time.monotonic

# --- MySQL/MariaDB wire protocol constants ----------------------------------
COM_QUIT = 0x01
COM_INIT_DB = 0x02
COM_QUERY = 0x03
COM_PING = 0x0E
COM_RESET_CONNECTION = 0x1F
COM_SET_OPTION = 0x1B

# Commands answered by exactly one packet (OK / ERR / EOF / string).
_SINGLE_PACKET_CMDS = frozenset((0x02, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0C, 0x0D,
                                 0x0E, 0x1A, 0x1B, 0x1F))

# Capability flags.  In the MariaDB protocol bit 0 is CLIENT_MYSQL: when it
# is cleared on both sides, MariaDB extended capabilities (bytes 28..31 of the
# handshake response) are in effect.
CLIENT_LONG_PASSWORD = 0x00000001
CLIENT_LONG_FLAG = 0x00000004
CLIENT_CONNECT_WITH_DB = 0x00000008
CLIENT_COMPRESS = 0x00000020
CLIENT_LOCAL_FILES = 0x00000080
CLIENT_PROTOCOL_41 = 0x00000200
CLIENT_SSL = 0x00000800
CLIENT_TRANSACTIONS = 0x00002000
CLIENT_SECURE_CONNECTION = 0x00008000
CLIENT_MULTI_STATEMENTS = 0x00010000
CLIENT_MULTI_RESULTS = 0x00020000
CLIENT_PS_MULTI_RESULTS = 0x00040000
CLIENT_PLUGIN_AUTH = 0x00080000
CLIENT_PLUGIN_AUTH_LENENC_DATA = 0x00200000
CLIENT_SESSION_TRACK = 0x00800000
CLIENT_DEPRECATE_EOF = 0x01000000
CLIENT_ZSTD_COMPRESSION = 0x04000000
CLIENT_QUERY_ATTRIBUTES = 0x08000000

MARIADB_CLIENT_CACHE_METADATA = 0x10

# Capabilities the bridge hides from the game server: TLS and compression
# would make the stream unparseable for a proxy, and query attributes change
# the COM_QUERY layout.  (Encryption towards the primary is done by the
# bridge itself, see [primary] use_tls.)
STRIPPED_SERVER_CAPS = (CLIENT_SSL | CLIENT_COMPRESS | CLIENT_ZSTD_COMPRESSION
                        | CLIENT_QUERY_ATTRIBUTES)
# Capabilities copied from the game server's session to replica connections
# so the replica produces byte-identical response framing.  Deliberately NOT
# CLIENT_MULTI_STATEMENTS: replica connections refuse multi-statements as a
# second line of defence behind the query classifier.
MIRRORED_CAPS = (CLIENT_LONG_PASSWORD | CLIENT_MULTI_RESULTS | CLIENT_PS_MULTI_RESULTS
                 | CLIENT_SESSION_TRACK | CLIENT_DEPRECATE_EOF)
BASE_CLIENT_CAPS = (CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS
                    | CLIENT_SECURE_CONNECTION | CLIENT_PLUGIN_AUTH)

SERVER_STATUS_IN_TRANS = 0x0001
SERVER_STATUS_AUTOCOMMIT = 0x0002
SERVER_MORE_RESULTS_EXISTS = 0x0008

MAX_PACKET_SIZE = 0xFFFFFF
CHUNK = 256 * 1024                 # raw forwarding read size
STREAM_LIMIT = 512 * 1024          # asyncio StreamReader buffer limit
WRITE_HIGH_WATERMARK = 1024 * 1024

NET_ERRORS = (asyncio.IncompleteReadError, ConnectionError, OSError)
TIMEOUT_ERRORS = (asyncio.TimeoutError, TimeoutError)

STATS = collections.Counter()


class ProtocolError(Exception):
    """Unexpected bytes on the wire."""


class AuthError(ConnectionError):
    """A backend rejected the bridge's own credentials."""


class RetryableError(ConnectionError):
    """A pooled backend connection died before any response byte arrived."""


class StreamBroken(Exception):
    """Part of a response was already forwarded and the rest is lost: the only
    safe reaction is to drop the client connection (the game server retries)."""


# --- small helpers ------------------------------------------------------------
class _CompatTimeout:
    """Minimal asyncio.timeout() replacement for Python < 3.11."""

    def __init__(self, delay):
        self._delay = delay
        self._handle = None
        self._expired = False
        self._task = None

    async def __aenter__(self):
        if self._delay is not None:
            self._task = asyncio.current_task()
            self._handle = asyncio.get_running_loop().call_later(self._delay, self._fire)
        return self

    def _fire(self):
        self._expired = True
        self._task.cancel()

    async def __aexit__(self, exc_type, exc, tb):
        if self._handle is not None:
            self._handle.cancel()
        if exc_type is asyncio.CancelledError and self._expired:
            raise asyncio.TimeoutError()
        return False


_timeout = getattr(asyncio, "timeout", None) or _CompatTimeout


class _RateLimitedLog:
    """Logs a given message key at most once per interval (with a count of
    suppressed repeats) so a misbehaving backend cannot flood the log."""

    def __init__(self, interval=10.0):
        self.interval = interval
        self._last = {}
        self._suppressed = collections.Counter()

    def __call__(self, level, key, msg, *args):
        now = monotonic()
        last = self._last.get(key)
        if last is not None and now - last < self.interval:
            self._suppressed[key] += 1
            return
        self._last[key] = now
        n = self._suppressed.pop(key, 0)
        if n:
            msg += " (%d similar messages suppressed)" % n
        log.log(level, msg, *args)


rl_log = _RateLimitedLog()


def _tune_socket(writer, keepalive=False):
    sock = writer.get_extra_info("socket")
    if sock is None:
        return
    try:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    except (OSError, AttributeError):
        pass
    if keepalive:
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            for opt, val in (("TCP_KEEPIDLE", 30), ("TCP_KEEPINTVL", 10), ("TCP_KEEPCNT", 3)):
                if hasattr(socket, opt):
                    sock.setsockopt(socket.IPPROTO_TCP, getattr(socket, opt), val)
        except OSError:
            pass
    try:
        writer.transport.set_write_buffer_limits(high=WRITE_HIGH_WATERMARK)
    except (AttributeError, NotImplementedError):
        pass


def _close_writer(writer):
    if writer is None:
        return
    try:
        writer.close()
    except Exception:
        pass


PASSWORD_RE = re.compile(
    r"((?:identified\s+(?:by|via|with)\s+(?:password\s+)?|password\s*(?:\(|=)\s*|master_password\s*=\s*)"
    r")(?:'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\")", re.IGNORECASE)


def log_sql(sql_text, limit=160):
    """Shorten SQL for logs, masking passwords."""
    if isinstance(sql_text, bytes):
        sql_text = sql_text.decode("utf-8", "replace")
    text = " ".join(sql_text.split())
    text = PASSWORD_RE.sub(lambda mo: mo.group(1) + "'***'", text)
    return text[:limit] + ("..." if len(text) > limit else "")


# --- packet IO ----------------------------------------------------------------
async def read_packet(reader, limit=64 * 1024 * 1024):
    """Read one logical MySQL packet (joining 16 MB continuation packets)."""
    header = await reader.readexactly(4)
    length = header[0] | (header[1] << 8) | (header[2] << 16)
    seq = header[3]
    payload = await reader.readexactly(length) if length else b""
    if length < MAX_PACKET_SIZE:
        return seq, payload
    parts = [payload]
    total = length
    while True:
        header = await reader.readexactly(4)
        length = header[0] | (header[1] << 8) | (header[2] << 16)
        total += length
        if total > limit:
            raise ProtocolError("packet exceeds max_packet_mb safety limit")
        parts.append(await reader.readexactly(length) if length else b"")
        if length < MAX_PACKET_SIZE:
            return seq, b"".join(parts)


def encode_packet(seq, payload):
    """Encode *payload* as MySQL packet(s), splitting at 16 MB boundaries."""
    n = len(payload)
    if n < MAX_PACKET_SIZE:
        return struct.pack("<I", n | ((seq & 0xFF) << 24)) + payload
    out = bytearray()
    offset = 0
    while True:
        chunk = payload[offset:offset + MAX_PACKET_SIZE]
        out += struct.pack("<I", len(chunk) | ((seq & 0xFF) << 24))
        out += chunk
        seq += 1
        offset += len(chunk)
        if len(chunk) < MAX_PACKET_SIZE:
            return bytes(out)


def err_packet(seq, code, msg, sqlstate="HY000"):
    """ERR packet.  sqlstate=None gives the pre-handshake format."""
    body = b"\xff" + struct.pack("<H", code)
    if sqlstate:
        body += b"#" + sqlstate.encode("ascii")[:5]
    return encode_packet(seq, body + msg.encode("utf-8", "replace"))


def parse_err(payload):
    """(errno, message) of an ERR packet."""
    if len(payload) < 3:
        return 0, ""
    code = payload[1] | (payload[2] << 8)
    msg = payload[3:]
    if msg[:1] == b"#":
        msg = msg[6:]
    return code, msg.decode("utf-8", "replace")


def _lenenc(data, pos):
    """Decode a length-encoded integer -> (value, new_pos); value None for NULL
    (0xFB).  Returns None if *data* is truncated."""
    if pos >= len(data):
        return None
    b = data[pos]
    if b < 0xFB:
        return b, pos + 1
    if b == 0xFB:
        return None, pos + 1
    width = {0xFC: 2, 0xFD: 3, 0xFE: 8}.get(b)
    if width is None or pos + 1 + width > len(data):
        return None
    return int.from_bytes(data[pos + 1:pos + 1 + width], "little"), pos + 1 + width


def _skip_lenenc(data, pos):
    b = data[pos] if pos < len(data) else 0
    if b < 0xFB:
        return pos + 1
    return pos + {0xFC: 3, 0xFD: 4, 0xFE: 9}.get(b, 1)


def ok_status(payload):
    """Server status flags of an OK packet (0x00 or 0xFE header)."""
    pos = _skip_lenenc(payload, _skip_lenenc(payload, 1))
    if pos + 2 <= len(payload):
        return payload[pos] | (payload[pos + 1] << 8)
    return 0


def eof_status(payload):
    if len(payload) >= 5:
        return payload[3] | (payload[4] << 8)
    return 0


# --- incremental response scanner ---------------------------------------------
# Follows server responses in a raw byte stream without copying it. Only
# COM_QUERY results and single-packet commands are understood; anything else
# makes the session fall back to verbatim relaying.
R_RESULT, R_SINGLE = range(2)
(_S_IDLE, _S_FIRST, _S_COLS, _S_COLEOF, _S_ROWS) = range(5)


class ResponseScanner:
    """Follows server responses in a raw byte stream without copying it.

    expect() registers the response of each command sent to the server;
    feed() consumes bytes in arbitrary chunks.  The scanner knows when each
    response is complete (busy) and reports LOCAL INFILE requests.  Only the
    few packets whose content matters (response heads and terminators) are
    looked at; rows are skipped by length.
    """

    __slots__ = ("deprecate_eof", "cache_metadata", "on_infile",
                 "_queue", "_cur", "_state", "_buf", "_skip", "_cont",
                 "_pk_first", "_pk_len", "_remaining", "_fresh",
                 "_first_payload", "first_byte", "desynced", "unsolicited")

    def __init__(self, deprecate_eof=False, cache_metadata=False, on_infile=None):
        self.deprecate_eof = deprecate_eof
        self.cache_metadata = cache_metadata
        self.on_infile = on_infile
        self._queue = collections.deque()
        self._cur = None
        self._state = _S_IDLE
        self._buf = bytearray()
        self._skip = 0
        self._cont = False
        self._pk_first = 0
        self._pk_len = 0
        self._remaining = 0
        self._fresh = False
        self._first_payload = b""
        self.first_byte = -1          # first byte of the latest response head
        self.desynced = None          # reason string once tracking is lost
        self.unsolicited = 0          # packets received while nothing was expected

    @property
    def busy(self):
        return self._state != _S_IDLE

    def expect(self, kind, on_ok=None):
        self._queue.append((kind, on_ok))
        if self._state == _S_IDLE:
            self._next()

    def _next(self):
        if self._queue:
            self._cur = self._queue.popleft()
            self._state = _S_FIRST
            self._fresh = True
        else:
            self._cur = None
            self._state = _S_IDLE

    def _finish(self, ok):
        cb = self._cur[1] if self._cur else None
        first_payload = self._first_payload
        self._next()
        if ok and cb is not None:
            cb(first_payload)

    def _desync(self, reason):
        if self.desynced is None:
            self.desynced = reason
        self._queue.clear()
        self._cur = None
        self._state = _S_IDLE

    def feed(self, data):
        if self.desynced is not None:
            return
        buf = self._buf
        if buf:
            buf += data
            d = buf
        else:
            d = data
        n = len(d)
        pos = 0
        while True:
            if self._skip:                                   # skipping a payload
                avail = n - pos
                if avail < self._skip:
                    self._skip -= avail
                    pos = n
                    break
                pos += self._skip
                self._skip = 0
                if not self._cont:
                    self._packet(self._pk_first, self._pk_len, None)
                continue
            if n - pos < 4:
                break
            length = d[pos] | (d[pos + 1] << 8) | (d[pos + 2] << 16)
            if self._cont:                                   # continuation of a 16 MB packet
                pos += 4
                self._cont = length == MAX_PACKET_SIZE
                if length:
                    self._skip = length
                elif not self._cont:
                    self._packet(self._pk_first, self._pk_len, None)
                continue
            if length == MAX_PACKET_SIZE:                    # first part of a huge packet
                if n - pos < 5:
                    break
                self._pk_first = d[pos + 4]
                self._pk_len = MAX_PACKET_SIZE
                self._cont = True
                self._skip = length
                pos += 4
                continue
            if length == 0:
                pos += 4
                self._packet(-1, 0, b"")
                continue
            if n - pos < 5:
                break
            first = d[pos + 4]
            end = pos + 4 + length
            if self._wants(first, length):
                if end > n:
                    break
                self._packet(first, length, bytes(d[pos + 4:end]))
                pos = end
            elif end <= n:
                pos = end
                self._packet(first, length, None)
            else:
                self._pk_first = first
                self._pk_len = length
                self._skip = end - n
                pos = n
            if self.desynced is not None:
                pos = n
                break
        if d is buf:
            del buf[:pos]
        elif pos < n:
            self._buf = bytearray(d[pos:])

    def _wants(self, first, length):
        st = self._state
        if st == _S_ROWS:
            if first == 0xFE:
                return self.deprecate_eof or length < 9
            return first == 0xFF
        if st == _S_FIRST:
            return True
        return first == 0xFF

    def _packet(self, first, length, payload):
        st = self._state
        if (first == 0xFF and payload is not None and len(payload) >= 3
                and payload[1] == 0xFF and payload[2] == 0xFF):
            return                                           # MariaDB progress report
        if st == _S_ROWS:
            if first == 0xFF:
                self._finish(False)
            elif first == 0xFE and payload is not None:
                status = ok_status(payload) if self.deprecate_eof else eof_status(payload)
                if status & SERVER_MORE_RESULTS_EXISTS:
                    self._state = _S_FIRST
                else:
                    self._finish(True)
            return
        if st == _S_COLS:
            self._remaining -= 1
            if self._remaining <= 0:
                self._state = _S_ROWS if self.deprecate_eof else _S_COLEOF
            return
        if st == _S_COLEOF:
            self._state = _S_ROWS
            return
        if st == _S_IDLE:
            self.unsolicited += 1
            return
        if st == _S_FIRST:
            if self._fresh:
                self._fresh = False
                self.first_byte = first
                self._first_payload = payload if payload is not None else b""
            if payload is None:
                self._desync("oversized packet at the start of a response")
                return
            if first == 0xFF:
                self._finish(False)
                return
            kind = self._cur[0]
            if kind == R_RESULT:
                if first == 0x00:
                    status = ok_status(payload)
                    if not status & SERVER_MORE_RESULTS_EXISTS:
                        self._finish(True)
                    return
                if first == 0xFB:                            # LOCAL INFILE request
                    if self.on_infile is not None:
                        self.on_infile()
                    return
                r = _lenenc(payload, 0)
                if r is None or not r[0]:
                    self._desync("malformed result set header")
                    return
                count, p = r
                if self.cache_metadata and p < len(payload) and payload[p] == 0:
                    self._state = _S_ROWS                    # metadata skipped
                    return
                self._remaining = count
                self._state = _S_COLS
                return
            if kind == R_SINGLE:
                self._finish(True)
                return
            self._desync("unknown response kind")
            return


# --- query classification -------------------------------------------------------
# Only plain SELECTs may go to the replica. Everything else goes to the
# primary. The game server sends SELECT/INSERT/UPDATE/DELETE only, so the
# first word is enough. Literals are blanked out first, so a semicolon or a
# keyword inside a value cannot change the route.
_LITERAL_RE = re.compile(rb"'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\"|`[^`]*`")
_FIRST_WORD_RE = re.compile(rb"[\s(]*([A-Za-z]+)")
_UNSAFE_SELECT_RE = re.compile(
    rb"@|\binto\b|\bfor\s+update\b|\bfor\s+share\b|\block\s+in\s+share\s+mode\b"
    rb"|\bsql_calc_found_rows\b|\b(?:next|previous)\s+value\s+for\b"
    rb"|\bcurrent_(?:user|role)\b|\bsession_user\b|\bsystem_user\b"
    rb"|\bprocesslist\b|\bperformance_schema\b"
    rb"|\b(?:last_insert_id|found_rows|row_count|get_lock|release_lock|release_all_locks"
    rb"|is_free_lock|is_used_lock|connection_id|nextval|lastval|setval|load_file"
    rb"|database|schema|user|master_pos_wait|source_pos_wait)\s*\(")
_WRITE_WORDS = frozenset((b"insert", b"update", b"delete", b"replace"))
# Statements that change session state the replica cannot see: after one of
# them the session stays on the primary.
_PIN_WORDS = frozenset((b"set", b"use", b"begin", b"start", b"lock"))


def _code(sql):
    """The statement with literals blanked out (safe to classify)."""
    if isinstance(sql, memoryview):
        sql = sql.tobytes()
    return _LITERAL_RE.sub(b" ", sql)


def _first_word(code):
    m = _FIRST_WORD_RE.match(code)
    return m.group(1).lower() if m else b""


def is_select(sql):
    """True if the COM_QUERY text is a plain SELECT that may use the replica."""
    code = _code(sql)
    if b";" in code.strip().rstrip(b";"):
        return False                                       # multi-statement
    if _first_word(code) != b"select":
        return False
    return _UNSAFE_SELECT_RE.search(code.lower()) is None


def is_write(sql):
    """True if the COM_QUERY text writes data (opens the read-your-writes window)."""
    return _first_word(_code(sql)) in _WRITE_WORDS


def pins_session(sql):
    """True if the COM_QUERY text changes session state (SET/USE/transaction/locks)."""
    code = _code(sql)
    word = _first_word(code)
    if word in _PIN_WORDS:
        return True
    return word == b"create" and b"temporary" in code.lower()


# --- handshake / authentication -------------------------------------------------
class Greeting:
    __slots__ = ("caps", "ext_caps", "status", "salt", "plugin", "charset",
                 "caps_lo_pos", "caps_hi_pos", "version")


def parse_greeting(p):
    """Parse a HandshakeV10 packet."""
    if not p or p[0] != 10:
        raise ProtocolError("unsupported server handshake (protocol %r)" % (p[:1],))
    g = Greeting()
    nul = p.index(b"\x00", 1)
    g.version = p[1:nul].decode("ascii", "replace")
    pos = nul + 1 + 4                                       # connection id
    salt = p[pos:pos + 8]
    pos += 9                                                # auth part 1 + filler
    g.caps_lo_pos = pos
    g.caps = p[pos] | (p[pos + 1] << 8)
    pos += 2
    g.charset, g.status, g.ext_caps, g.plugin, g.caps_hi_pos = 0x21, 0, 0, "", None
    if pos + 5 <= len(p):
        g.charset = p[pos]
        g.status = p[pos + 1] | (p[pos + 2] << 8)
        g.caps_hi_pos = pos + 3
        g.caps |= (p[pos + 3] | (p[pos + 4] << 8)) << 16
        pos += 5
        auth_len = p[pos] if pos < len(p) else 0
        pos += 1 + 6
        if not g.caps & CLIENT_LONG_PASSWORD and pos + 4 <= len(p):
            g.ext_caps = int.from_bytes(p[pos:pos + 4], "little")
        pos += 4
        if g.caps & CLIENT_SECURE_CONNECTION:
            n = max(13, auth_len - 8)
            salt += p[pos:pos + n].rstrip(b"\x00")
            pos += n
        if g.caps & CLIENT_PLUGIN_AUTH and pos < len(p):
            end = p.find(b"\x00", pos)
            g.plugin = p[pos:end if end >= 0 else len(p)].decode("ascii", "replace")
    g.salt = bytes(salt[:20])
    return g


def patch_greeting(p, g, caps):
    """Return the greeting with its capability flags replaced by *caps*."""
    out = bytearray(p)
    out[g.caps_lo_pos] = caps & 0xFF
    out[g.caps_lo_pos + 1] = (caps >> 8) & 0xFF
    if g.caps_hi_pos is not None:
        out[g.caps_hi_pos] = (caps >> 16) & 0xFF
        out[g.caps_hi_pos + 1] = (caps >> 24) & 0xFF
    return bytes(out)


def native_password_token(password, salt):
    if not password:
        return b""                       # empty password => empty auth response
    if isinstance(password, str):
        password = password.encode("utf-8")
    stage1 = hashlib.sha1(password).digest()
    stage2 = hashlib.sha1(stage1).digest()
    mix = hashlib.sha1(salt[:20] + stage2).digest()
    return bytes(a ^ b for a, b in zip(stage1, mix))


def caching_sha2_token(password, salt):
    if not password:
        return b""
    if isinstance(password, str):
        password = password.encode("utf-8")
    p1 = hashlib.sha256(password).digest()
    p2 = hashlib.sha256(p1).digest()
    p3 = hashlib.sha256(p2 + salt[:20]).digest()
    return bytes(a ^ b for a, b in zip(p1, p3))


_AUTH_PLUGINS = {
    "mysql_native_password": native_password_token,
    "caching_sha2_password": caching_sha2_token,
}


async def _upgrade_tls_compat(writer, ctx, server_hostname):
    """StreamWriter.start_tls() equivalent for Python < 3.11."""
    await writer.drain()
    loop = asyncio.get_running_loop()
    transport = writer.transport
    protocol = transport.get_protocol()
    new_transport = await loop.start_tls(transport, protocol, ctx,
                                         server_hostname=server_hostname)
    writer._transport = new_transport
    if hasattr(protocol, "_transport"):
        protocol._transport = new_transport
    protocol._over_ssl = True


async def upgrade_tls(writer, ctx, server_hostname):
    """Switch an open stream to TLS (MySQL 'SSLRequest' flow)."""
    if hasattr(writer, "start_tls"):                         # Python 3.11+
        await writer.start_tls(ctx, server_hostname=server_hostname)
    else:
        await _upgrade_tls_compat(writer, ctx, server_hostname)


class BackendSettings:
    """Connection settings of the primary or the replica."""

    def __init__(self, host, port, connect_timeout=5.0, use_tls=False, tls_mode="mysql",
                 ssl_context=None, server_name=None, user="", password="", database=None,
                 role="backend"):
        self.host = host
        self.port = port
        self.connect_timeout = connect_timeout
        self.use_tls = use_tls
        self.tls_mode = tls_mode
        self.ssl_context = ssl_context
        self.server_name = server_name or host
        self.user = user
        self.password = password
        self.database = database
        self.role = role

    async def open(self):
        kwargs = {"limit": STREAM_LIMIT}
        if self.use_tls and self.tls_mode == "direct":
            kwargs["ssl"] = self.ssl_context
            kwargs["server_hostname"] = self.server_name
        reader, writer = await asyncio.wait_for(
            asyncio.open_connection(self.host, self.port, **kwargs), self.connect_timeout)
        _tune_socket(writer, keepalive=True)
        return reader, writer

    @property
    def starttls(self):
        return self.use_tls and self.tls_mode == "mysql"


async def backend_login(reader, writer, cfg, req_caps, charset):
    """Log in to a backend as the bridge itself.  Returns negotiated caps."""
    _, greet = await read_packet(reader)
    if greet[:1] == b"\xff":
        raise ConnectionError("%s refused connection: %s" % (cfg.role, parse_err(greet)[1]))
    g = parse_greeting(greet)
    caps = BASE_CLIENT_CAPS | (req_caps & MIRRORED_CAPS & g.caps)
    caps |= req_caps & CLIENT_LONG_PASSWORD                   # mirror CLIENT_MYSQL bit
    db = cfg.database.encode("utf-8") if cfg.database else b""
    if db:
        caps |= CLIENT_CONNECT_WITH_DB
    seq = 1
    if cfg.starttls:
        if not g.caps & CLIENT_SSL:
            raise ConnectionError("%s does not support TLS but use_tls = true" % cfg.role)
        caps |= CLIENT_SSL
        writer.write(encode_packet(seq, struct.pack("<IIB", caps, MAX_PACKET_SIZE, charset)
                                   + b"\x00" * 19 + struct.pack("<I", 0)))
        await writer.drain()
        await upgrade_tls(writer, cfg.ssl_context, cfg.server_name)
        seq = 2
    plugin = g.plugin if g.plugin in _AUTH_PLUGINS else "mysql_native_password"
    token = _AUTH_PLUGINS[plugin](cfg.password, g.salt)
    resp = (struct.pack("<IIB", caps, MAX_PACKET_SIZE, charset) + b"\x00" * 19
            + struct.pack("<I", 0) + cfg.user.encode("utf-8") + b"\x00"
            + bytes([len(token)]) + token)
    if db:
        resp += db + b"\x00"
    resp += plugin.encode("ascii") + b"\x00"
    writer.write(encode_packet(seq, resp))
    await writer.drain()
    while True:
        seq, p = await read_packet(reader)
        head = p[:1]
        if head == b"\x00":
            return caps
        if head == b"\xff":
            raise AuthError("%s rejected login of %r: %s" % (cfg.role, cfg.user, parse_err(p)[1]))
        if head == b"\xfe" and len(p) > 1:                     # auth switch
            nul = p.find(b"\x00", 1)
            plugin = p[1:nul].decode("ascii", "replace")
            salt = p[nul + 1:]
            if salt.endswith(b"\x00"):
                salt = salt[:-1]
            if plugin not in _AUTH_PLUGINS:
                raise AuthError("%s requires unsupported auth plugin %r for %r; use "
                                "mysql_native_password for this account"
                                % (cfg.role, plugin, cfg.user))
            writer.write(encode_packet(seq + 1, _AUTH_PLUGINS[plugin](cfg.password, salt)))
            await writer.drain()
            continue
        if head == b"\x01" and plugin == "caching_sha2_password" and len(p) >= 2:
            if p[1] == 3:                                    # fast auth succeeded
                continue
            if p[1] == 4 and cfg.starttls:                    # full auth over TLS
                writer.write(encode_packet(seq + 1, cfg.password.encode("utf-8") + b"\x00"))
                await writer.drain()
                continue
            raise AuthError("%s: caching_sha2_password full authentication needs TLS "
                            "(or use mysql_native_password for %r)" % (cfg.role, cfg.user))
        raise AuthError("%s: unexpected authentication packet 0x%02x" % (cfg.role, p[0] if p else 0))


async def simple_command(reader, writer, payload, deprecate_eof):
    """Run one command and read its complete response (packet mode).

    Returns (ok, error_message).
    """
    writer.write(encode_packet(0, payload))
    await writer.drain()
    _, first = await read_packet(reader)
    if first[:1] == b"\xff":
        code, msg = parse_err(first)
        return False, "%d: %s" % (code, msg)
    if first[:1] == b"\x00":
        return True, ""
    r = _lenenc(first, 0)
    ncols = r[0] if r else 0
    for _ in range(ncols):
        await read_packet(reader)
    if not deprecate_eof:
        await read_packet(reader)                            # EOF after column definitions
    while True:
        _, p = await read_packet(reader)
        if p[:1] == b"\xff":
            code, msg = parse_err(p)
            return False, "%d: %s" % (code, msg)
        if p[:1] == b"\xfe" and (deprecate_eof or len(p) < 9):
            status = ok_status(p) if deprecate_eof else eof_status(p)
            if status & SERVER_MORE_RESULTS_EXISTS:
                raise ProtocolError("unexpected multi-result response")
            return True, ""


# --- replica connection pool ------------------------------------------------------
class BackendConn:
    __slots__ = ("reader", "writer", "caps", "idle_since", "scanner")

    def __init__(self, reader, writer, caps):
        self.reader = reader
        self.writer = writer
        self.caps = caps
        self.idle_since = monotonic()
        self.scanner = ResponseScanner(deprecate_eof=bool(caps & CLIENT_DEPRECATE_EOF))

    @property
    def deprecate_eof(self):
        return bool(self.caps & CLIENT_DEPRECATE_EOF)

    def is_dead(self):
        return self.reader.at_eof() or self.writer.is_closing()

    def close(self):
        _close_writer(self.writer)

    async def ping(self, timeout):
        try:
            async with _timeout(timeout):
                ok, _ = await simple_command(self.reader, self.writer,
                                             bytes([COM_PING]), self.deprecate_eof)
            return ok
        except Exception:
            return False

    async def forward(self, payload, client_writer, buffer_limit, timeout):
        """Run a COM_QUERY and relay its response.

        Returns None if the response was streamed to the client, otherwise
        (chunks, first_byte) for the caller to forward (or to fall back on
        ERR).  Responses up to *buffer_limit* bytes are buffered completely so
        a replica failure can still fall back to the primary transparently.
        """
        reader = self.reader
        sc = self.scanner
        if self.is_dead():
            raise RetryableError("pooled replica connection was closed")
        sc.expect(R_RESULT)
        chunks = []
        size = 0
        streaming = False
        got = False
        try:
            self.writer.write(encode_packet(0, payload))
            async with _timeout(timeout):
                while sc.busy:
                    data = await reader.read(CHUNK)
                    if not data:
                        if not got:
                            raise RetryableError("replica closed the connection")
                        raise ConnectionError("replica closed the connection mid-response")
                    got = True
                    sc.feed(data)
                    if sc.desynced is not None or sc.unsolicited:
                        raise ProtocolError("replica response desync: %s" % (sc.desynced or "extra data"))
                    if streaming:
                        client_writer.write(data)
                        await client_writer.drain()
                    else:
                        chunks.append(data)
                        size += len(data)
                        if sc.busy and size > buffer_limit and sc.first_byte != 0xFF:
                            streaming = True
                            client_writer.write(b"".join(chunks))
                            chunks = None
                            await client_writer.drain()
        except asyncio.CancelledError:
            raise
        except RetryableError:
            raise
        except BaseException as e:
            if streaming:
                raise StreamBroken("replica response interrupted: %s: %s" % (type(e).__name__, e)) from e
            if not got and isinstance(e, NET_ERRORS) and not isinstance(e, TIMEOUT_ERRORS):
                raise RetryableError(str(e)) from e
            raise
        if streaming:
            return None
        return chunks, sc.first_byte


_POOL_TIMEOUT = object()


def _expire_waiter(fut):
    if not fut.done():
        fut.set_result(_POOL_TIMEOUT)


class ReplicaPool:
    """Persistent replica connections, all identical (shared database only).

    The first connection fixes the charset and the deprecate-EOF framing;
    sessions with a different charset or framing use the primary. All
    bookkeeping is synchronous (single-threaded event loop), so there is no
    lock and no possibility of slot leaks on failed connects: a slot freed
    by a failure is handed straight to the next waiter.
    """

    def __init__(self, settings, size=8, acquire_timeout=0.25, query_timeout=30.0,
                 buffer_limit=1024 * 1024, idle_ping=30.0, idle_close=300.0):
        self.cfg = settings
        self.size = max(1, size)
        self.acquire_timeout = acquire_timeout
        self.query_timeout = query_timeout
        self.buffer_limit = buffer_limit
        self.idle_ping = idle_ping
        self.idle_close = idle_close
        self.total = 0
        self._idle = collections.deque()
        self._waiters = collections.deque()
        self._down_until = 0.0
        self._fails = 0
        self._probing = False
        self.charset = None
        self.deprecate_eof = None
        self._closed = False

    # -- health --------------------------------------------------------------
    @property
    def is_down(self):
        return monotonic() < self._down_until

    def usable(self):
        return not self._closed and monotonic() >= self._down_until

    def _mark_down(self, exc):
        self._fails += 1
        backoff = min(30.0, 0.5 * (2 ** min(self._fails - 1, 6)))
        self._down_until = monotonic() + backoff
        STATS["replica_down_events"] += 1
        level = logging.ERROR if isinstance(exc, AuthError) else logging.WARNING
        rl_log(level, "replica-down", "replica %s:%s unavailable (%s: %s); reads use the "
               "primary, retry in %.1fs", self.cfg.host, self.cfg.port,
               type(exc).__name__, exc, backoff)

    def _mark_up(self):
        if self._fails:
            log.info("replica %s:%s is available again", self.cfg.host, self.cfg.port)
        self._fails = 0
        self._down_until = 0.0

    # -- slots -----------------------------------------------------------------
    def _hand_slot(self):
        """A connection slot became free: give it to a waiter or release it."""
        while self._waiters:
            fut = self._waiters.popleft()
            if not fut.done():
                fut.set_result(None)
                return
        self.total -= 1

    def _discard(self, conn):
        conn.close()
        self._hand_slot()

    def release(self, conn):
        if self._closed or conn.is_dead():
            self._discard(conn)
            return
        conn.idle_since = monotonic()
        if self._waiters:
            fut = self._waiters.popleft()
            if not fut.done():
                fut.set_result(conn)
                return
            self._discard(conn)
            return
        self._idle.append(conn)

    async def acquire(self, charset, deprecate_eof):
        """A replica connection, or None if the replica cannot serve this
        session right now (the caller then uses the primary)."""
        if self.charset is None:
            self.charset = charset
        elif charset != self.charset:
            return None
        if self.deprecate_eof is not None and deprecate_eof != self.deprecate_eof:
            return None
        now = monotonic()
        if self._closed or now < self._down_until:
            return None
        while self._idle:
            conn = self._idle.pop()                          # LIFO: warmest connection
            if conn.is_dead():
                self._discard(conn)
                continue
            if now - conn.idle_since > self.idle_ping and not await conn.ping(2.0):
                self._discard(conn)
                continue
            return conn
        if self.total >= self.size:
            loop = asyncio.get_running_loop()
            fut = loop.create_future()
            self._waiters.append(fut)
            # a timer resolves the future with a sentinel: cheaper than
            # wait_for() and no cancel-vs-hand-off race
            timer = loop.call_later(self.acquire_timeout, _expire_waiter, fut)
            try:
                got = await fut
            except asyncio.CancelledError:
                if fut.done() and not fut.cancelled():
                    got = fut.result()
                    if got is None:
                        self._hand_slot()
                    elif got is not _POOL_TIMEOUT:
                        self.release(got)
                raise
            finally:
                timer.cancel()
                try:
                    self._waiters.remove(fut)
                except ValueError:
                    pass
            if got is _POOL_TIMEOUT:
                STATS["replica_pool_timeouts"] += 1
                return None
            if got is not None:
                return got
            # got None: a slot was handed to us (total already counts it)
        else:
            self.total += 1
        if self._fails and self._probing:                    # half-open: one probe at a time
            self._hand_slot()
            return None
        self._probing = bool(self._fails)
        try:
            conn = await self._connect(charset, deprecate_eof)
        except asyncio.CancelledError:
            self._hand_slot()
            raise
        except Exception as e:
            self._hand_slot()
            self._mark_down(e)
            return None
        finally:
            self._probing = False
        self._mark_up()
        if self.deprecate_eof is None:
            self.deprecate_eof = conn.deprecate_eof
        if conn.deprecate_eof != deprecate_eof:
            self.release(conn)             # framing differs: keep it for matching sessions
            return None
        return conn

    async def _connect(self, charset, deprecate_eof):
        req_caps = CLIENT_LONG_PASSWORD | (CLIENT_DEPRECATE_EOF if deprecate_eof else 0)
        reader, writer = await self.cfg.open()
        try:
            async with _timeout(self.cfg.connect_timeout):
                caps = await backend_login(reader, writer, self.cfg, req_caps, charset)
        except BaseException:
            _close_writer(writer)
            raise
        STATS["replica_connects"] += 1
        return BackendConn(reader, writer, caps)

    # -- queries -----------------------------------------------------------------
    async def read(self, payload, client_writer, charset, deprecate_eof):
        """Serve one COM_QUERY from the replica.  True if the client got the
        response, False if the caller must use the primary instead."""
        for attempt in (0, 1):
            conn = await self.acquire(charset, deprecate_eof)
            if conn is None:
                return False
            try:
                res = await conn.forward(payload, client_writer, self.buffer_limit,
                                         self.query_timeout)
            except RetryableError:
                self._discard(conn)
                if attempt == 0:
                    continue
                return False
            except BaseException:
                self._discard(conn)
                raise
            self.release(conn)
            if res is None:
                return True
            chunks, first = res
            if first == 0xFF:
                code, msg = parse_err(_first_packet_payload(chunks))
                STATS["replica_errors"] += 1
                rl_log(logging.WARNING, "replica-err-%d" % code,
                       "replica returned error %d (%s); query re-run on the primary", code, msg)
                return False
            client_writer.write(chunks[0] if len(chunks) == 1 else b"".join(chunks))
            await client_writer.drain()
            return True
        return False

    # -- maintenance ---------------------------------------------------------------
    def close_idle(self, older_than=None):
        now = monotonic()
        limit = self.idle_close if older_than is None else older_than
        keep = collections.deque()
        while self._idle:
            conn = self._idle.popleft()
            if conn.is_dead() or now - conn.idle_since > limit:
                self._discard(conn)
            else:
                keep.append(conn)
        self._idle = keep

    async def close(self):
        self._closed = True
        for conn in self._idle:
            conn.close()
        self._idle.clear()
        for fut in self._waiters:
            if not fut.done():
                fut.cancel()
        self._waiters.clear()


def _first_packet_payload(chunks):
    data = chunks[0] if len(chunks) == 1 else b"".join(chunks)
    length = data[0] | (data[1] << 8) | (data[2] << 16)
    return data[4:4 + length]


# --- per-client session -------------------------------------------------------------
class _Quit(Exception):
    pass


class ClientSession:
    """One game-server connection: a primary connection plus routing state."""

    def __init__(self, bridge, reader, writer):
        self.b = bridge
        self.cr = reader
        self.cw = writer
        self.peer = writer.get_extra_info("peername")
        self.pr = None
        self.pw = None
        self.started = self.last_activity = monotonic()
        self.authenticated = False
        self.auth_seq_offset = 0
        self.relay_only = not bridge.route_replica
        self.eff_caps = 0
        self.charset = 0x21
        self.sticky = None               # reason why this session must use the primary
        self.infile_active = False
        self.scanner = None
        self.closed = False

    # -- lifecycle -----------------------------------------------------------------
    async def run(self):
        try:
            if not await self._open_primary():
                return
            if not await self._handshake():
                return
            t_up = asyncio.ensure_future(self._c2s())
            t_down = asyncio.ensure_future(self._s2c())
            done, pending = await asyncio.wait({t_up, t_down}, return_when=asyncio.FIRST_COMPLETED)
            for t in pending:
                t.cancel()
            await asyncio.gather(*pending, return_exceptions=True)
            for t in done:
                if t.cancelled():
                    continue
                exc = t.exception()
                if exc is None or isinstance(exc, (_Quit, asyncio.IncompleteReadError)):
                    continue
                if isinstance(exc, StreamBroken):
                    STATS["stream_broken"] += 1
                    rl_log(logging.WARNING, "stream-broken", "client %s: %s; connection closed",
                           self.peer, exc)
                elif isinstance(exc, NET_ERRORS):
                    log.debug("client %s: connection error: %s", self.peer, exc)
                else:
                    STATS["errors"] += 1
                    log.warning("client %s: session error %s: %s", self.peer,
                                type(exc).__name__, exc)
        except asyncio.CancelledError:
            pass
        except NET_ERRORS + TIMEOUT_ERRORS as e:
            log.debug("client %s: %s during login: %s", self.peer, type(e).__name__, e)
        except Exception as e:
            STATS["errors"] += 1
            log.warning("client %s: %s: %s", self.peer, type(e).__name__, e)
        finally:
            self.close()

    def close(self):
        if self.closed:
            return
        self.closed = True
        if (self.pw is not None and self.authenticated and not self.relay_only
                and self.scanner is not None and not self.scanner.busy):
            try:                         # polite COM_QUIT: no "aborted connection" on the primary
                self.pw.write(encode_packet(0, bytes([COM_QUIT])))
            except Exception:
                pass
        _close_writer(self.pw)
        _close_writer(self.cw)
        log.debug("client %s disconnected", self.peer)

    def _reject(self, code, msg, seq=0, sqlstate=None):
        try:
            self.cw.write(err_packet(seq, code, msg, sqlstate))
        except Exception:
            pass

    async def _open_primary(self):
        pcfg = self.b.primary
        try:
            self.pr, self.pw = await pcfg.open()
        except Exception as e:
            STATS["primary_connect_failures"] += 1
            rl_log(logging.WARNING, "primary-connect", "cannot reach primary %s:%s: %s: %s",
                   pcfg.host, pcfg.port, type(e).__name__, e)
            self._reject(2003, "Bridge cannot reach the primary database")
            return False
        return True

    async def _handshake(self):
        pcfg = self.b.primary
        seq, greet = await asyncio.wait_for(read_packet(self.pr), pcfg.connect_timeout)
        if greet[:1] == b"\xff" or not greet:
            self.cw.write(encode_packet(seq, greet))         # e.g. "too many connections"
            return False
        g = parse_greeting(greet)
        if pcfg.starttls and not g.caps & CLIENT_SSL:
            log.error("primary %s:%s does not offer TLS but [primary] use_tls = true; "
                      "refusing to send credentials in clear text", pcfg.host, pcfg.port)
            self._reject(2026, "Bridge: primary does not support TLS")
            return False
        server_caps = g.caps & ~STRIPPED_SERVER_CAPS
        self.cw.write(encode_packet(seq, patch_greeting(greet, g, server_caps)))
        await self.cw.drain()

        seq, resp = await asyncio.wait_for(read_packet(self.cr), self.b.auth_timeout)
        caps = int.from_bytes(resp[:4], "little") if len(resp) >= 4 else 0
        if not caps & CLIENT_PROTOCOL_41 or len(resp) < 32:
            log.warning("client %s uses an unsupported pre-4.1 protocol", self.peer)
            self._reject(1251, "Client does not support the 4.1 protocol", seq + 1)
            return False
        if caps & CLIENT_SSL:
            log.warning("client %s requested TLS, which the bridge does not terminate "
                        "(set sv_sql_use_tls 0; the bridge encrypts towards the primary itself)",
                        self.peer)
            return False
        if caps & (CLIENT_COMPRESS | CLIENT_ZSTD_COMPRESSION):
            self._reject(1157, "Bridge does not support protocol compression", seq + 1, "08S01")
            return False
        self.charset = resp[8]
        client_ext = int.from_bytes(resp[28:32], "little")
        self.eff_caps = caps & server_caps
        if client_ext & MARIADB_CLIENT_CACHE_METADATA:
            self._stick("MariaDB extended metadata")
        if pcfg.starttls:
            ssl_req = struct.pack("<I", caps | CLIENT_SSL) + resp[4:32]
            self.pw.write(encode_packet(seq, ssl_req))
            await self.pw.drain()
            await asyncio.wait_for(upgrade_tls(self.pw, pcfg.ssl_context, pcfg.server_name),
                                   pcfg.connect_timeout)
            self.auth_seq_offset = 1
            self.pw.write(encode_packet(seq + 1, struct.pack("<I", caps | CLIENT_SSL) + resp[4:]))
        else:
            self.pw.write(encode_packet(seq, resp))
        await self.pw.drain()
        self.scanner = ResponseScanner(
            deprecate_eof=bool(self.eff_caps & CLIENT_DEPRECATE_EOF),
            cache_metadata=bool(client_ext & MARIADB_CLIENT_CACHE_METADATA),
            on_infile=self._on_infile)
        return True

    # -- primary -> client ---------------------------------------------------------
    async def _s2c(self):
        pr, cw = self.pr, self.cw
        while not self.authenticated:
            seq, p = await read_packet(pr)
            out_seq = (seq - self.auth_seq_offset) & 0xFF
            if p[:1] == b"\x00":
                self.authenticated = True
                self.auth_seq_offset = 0
                log.debug("client %s authenticated (caps=0x%08x cs=%d deof=%s)",
                          self.peer, self.eff_caps, self.charset,
                          bool(self.eff_caps & CLIENT_DEPRECATE_EOF))
            self.last_activity = monotonic()
            cw.write(encode_packet(out_seq, p))
            await cw.drain()
        scanner = self.scanner
        read = pr.read
        while True:
            data = await read(CHUNK)
            if not data:
                return
            self.last_activity = monotonic()
            if not self.relay_only:
                scanner.feed(data)
                if scanner.desynced is not None:
                    self._relay_only(scanner.desynced)
            cw.write(data)
            await cw.drain()

    def _on_infile(self):
        self.infile_active = True

    # -- client -> primary / replica ------------------------------------------------
    async def _c2s(self):
        cr, pw = self.cr, self.pw
        limit = self.b.max_packet
        while True:
            seq, p = await read_packet(cr, limit)
            self.last_activity = monotonic()
            if not self.authenticated and seq != 0:          # login exchange continues
                pw.write(encode_packet((seq + self.auth_seq_offset) & 0xFF, p))
                await pw.drain()
                continue
            if self.relay_only:
                pw.write(encode_packet(seq, p))
                await pw.drain()
                continue
            if self.infile_active:                           # LOAD DATA LOCAL file content
                if not p:
                    self.infile_active = False
                pw.write(encode_packet(seq, p))
                await pw.drain()
                continue
            await self._command(seq, p)

    async def _forward(self, seq, p, kind=None):
        if kind is not None:
            self.scanner.expect(kind)
        self.pw.write(encode_packet(seq, p))
        await self.pw.drain()

    async def _command(self, seq, p):
        if not p:
            await self._forward(seq, p, R_SINGLE)
            return
        cmd = p[0]
        if cmd == COM_QUERY:
            await self._query(seq, p)
            return
        if cmd == COM_QUIT:
            self.pw.write(encode_packet(seq, p))
            raise _Quit()
        if cmd == COM_INIT_DB:
            db = p[1:].decode("utf-8", "replace")
            if db and db != self.b.shared_db:
                self._stick("database %r" % db)
            await self._forward(seq, p, R_SINGLE)
            return
        if cmd == COM_RESET_CONNECTION:
            await self._forward(seq, p, R_SINGLE)
            self.sticky = None
            return
        if cmd in _SINGLE_PACKET_CMDS:
            await self._forward(seq, p, R_SINGLE)
            return
        # Prepared statements, COM_CHANGE_USER, COM_FIELD_LIST and anything
        # else the bridge does not understand: relay verbatim to the primary.
        self._relay_only("command 0x%02x" % cmd)
        await self._forward(seq, p)

    async def _query(self, seq, p):
        b = self.b
        sql = p[1:]
        if is_select(sql) and seq == 0 and self._replica_allowed():
            served = False
            try:
                served = await b.pool.read(p, self.cw, self.charset,
                                           bool(self.eff_caps & CLIENT_DEPRECATE_EOF))
            except (StreamBroken, asyncio.CancelledError):
                raise
            except Exception as e:
                STATS["replica_errors"] += 1
                rl_log(logging.WARNING, "replica-exc-" + type(e).__name__,
                       "replica read failed (%s: %s); using the primary", type(e).__name__, e)
            if served:
                STATS["replica"] += 1
                if log.isEnabledFor(logging.DEBUG):
                    log.debug("replica <- %s", log_sql(sql))
                return
            STATS["fallback"] += 1
        STATS["primary"] += 1
        if log.isEnabledFor(logging.DEBUG):
            log.debug("primary <- %s", log_sql(sql))
        if is_write(sql):
            b.note_write()
        if pins_session(sql):
            self._stick("session statement")
        await self._forward(seq, p, R_RESULT)

    def _replica_allowed(self):
        if (self.sticky is not None or self.infile_active or self.scanner.busy
                or not self.authenticated):
            return False
        b = self.b
        if not b.pool.usable():
            return False
        if b.grace > 0 and monotonic() - b.last_write < b.grace:
            return False
        return True

    # -- session state bookkeeping ----------------------------------------------------
    def _stick(self, reason):
        if self.sticky is None:
            log.debug("client %s: reads pinned to primary (%s)", self.peer, reason)
            self.sticky = reason

    def _relay_only(self, reason):
        if not self.relay_only:
            log.debug("client %s: relaying verbatim from now on (%s)", self.peer, reason)
        self.relay_only = True
        self.sticky = reason


# --- bridge server ----------------------------------------------------------------
class Bridge:
    def __init__(self, conf):
        self.conf = conf
        self.primary = conf.primary
        self.route_replica = conf.route_replica
        self.grace = conf.grace
        self.last_write = -1e18
        self.shared_db = conf.replica.database
        self.auth_timeout = conf.auth_timeout
        self.idle_timeout = conf.idle_timeout
        self.max_clients = conf.max_clients
        self.max_packet = conf.max_packet
        self.allowed = conf.allowed_clients
        self.pool = ReplicaPool(
            conf.replica, size=conf.pool_size, acquire_timeout=conf.acquire_timeout,
            query_timeout=conf.replica_query_timeout, buffer_limit=conf.replica_buffer)
        self.sessions = set()
        self.servers = []
        self._tasks = []
        self.ports = []

    def note_write(self):
        self.last_write = monotonic()

    def _allowed(self, peer):
        if not self.allowed:
            return True
        try:
            ip = ipaddress.ip_address(peer[0].split("%", 1)[0])
        except (ValueError, TypeError, IndexError):
            return False
        if ip.version == 6 and ip.ipv4_mapped is not None:
            ip = ip.ipv4_mapped
        return any(ip in net for net in self.allowed)

    async def _on_client(self, reader, writer):
        peer = writer.get_extra_info("peername") or ("?", 0)
        _tune_socket(writer)
        if not self._allowed(peer):
            STATS["rejected"] += 1
            rl_log(logging.WARNING, "reject-%s" % (peer[0],),
                   "rejected client %s: not in allowed_clients", peer[0])
            writer.write(err_packet(0, 1130, "Host is not allowed to connect to this bridge", None))
            _close_writer(writer)
            return
        if len(self.sessions) >= self.max_clients:
            STATS["rejected"] += 1
            rl_log(logging.WARNING, "max-clients", "rejected client %s: max_clients (%d) reached",
                   peer, self.max_clients)
            writer.write(err_packet(0, 1040, "Too many connections (bridge max_clients)", None))
            _close_writer(writer)
            return
        session = ClientSession(self, reader, writer)
        self.sessions.add(session)
        STATS["clients_total"] += 1
        log.debug("client %s connected", peer)
        try:
            await session.run()
        finally:
            self.sessions.discard(session)

    async def start(self):
        conf = self.conf
        hosts = ["127.0.0.1", "::1"] if conf.listen_host == "localhost" else \
            [h.strip() for h in conf.listen_host.split(",") if h.strip()]
        for h in hosts:
            try:
                srv = await asyncio.start_server(self._on_client, h, conf.listen_port,
                                                 backlog=512, limit=STREAM_LIMIT,
                                                 reuse_address=True)
            except OSError as e:
                log.warning("cannot listen on %s:%s: %s", h, conf.listen_port, e)
                continue
            self.servers.append(srv)
            for s in srv.sockets:
                self.ports.append(s.getsockname()[1])
        if not self.servers:
            raise RuntimeError("bridge: no listen address available")
        loop = asyncio.get_running_loop()
        self._tasks.append(loop.create_task(self._housekeeping()))
        if conf.stats_interval > 0:
            self._tasks.append(loop.create_task(self._stats_loop()))
        log.info("bridge %s listening on %s:%s (uvloop=%s, max_clients=%d, pool=%d, "
                 "replica_reads=%s, ryw=%.1fs, primary=%s:%s tls=%s)",
                 __version__, ",".join(hosts), self.ports[0] if self.ports else conf.listen_port,
                 conf.uvloop_active, conf.max_clients, conf.pool_size, conf.route_replica,
                 conf.grace, self.primary.host, self.primary.port,
                 self.primary.use_tls and self.primary.tls_mode)

    async def _housekeeping(self):
        tick = 0
        while True:
            await asyncio.sleep(1.0)
            tick += 1
            now = monotonic()
            for s in list(self.sessions):
                if not s.authenticated and now - s.started > self.auth_timeout:
                    STATS["auth_timeouts"] += 1
                    log.info("client %s: login timed out", s.peer)
                    s.close()
                elif (self.idle_timeout > 0 and now - s.last_activity > self.idle_timeout
                      and (s.relay_only or s.scanner is None or not s.scanner.busy)):
                    log.info("client %s: idle for %.0fs, closing", s.peer, now - s.last_activity)
                    s.close()
            if tick % 30 == 0:
                self.pool.close_idle()

    async def _stats_loop(self):
        while True:
            await asyncio.sleep(self.conf.stats_interval)
            pool = self.pool
            state = "down" if pool.is_down else "up"
            log.info("stats replica=%d primary=%d fallback=%d replica_errors=%d errors=%d "
                     "pool_timeouts=%d clients=%d total_clients=%d rejected=%d pool=%d/%d replica=%s",
                     STATS["replica"], STATS["primary"], STATS["fallback"],
                     STATS["replica_errors"], STATS["errors"], STATS["replica_pool_timeouts"], len(self.sessions),
                     STATS["clients_total"], STATS["rejected"], pool.total, pool.size, state)

    async def stop(self):
        log.info("bridge shutting down")
        for srv in self.servers:
            srv.close()
        for t in self._tasks:
            t.cancel()
        await asyncio.gather(*self._tasks, return_exceptions=True)
        for s in list(self.sessions):
            s.close()
        for srv in self.servers:
            try:
                await asyncio.wait_for(srv.wait_closed(), 5.0)
            except Exception:
                pass
        await self.pool.close()


# --- configuration -----------------------------------------------------------------
class ConfigError(Exception):
    pass


class BridgeConfig:
    pass


def _env(name, fallback):
    val = os.environ.get(name)
    return fallback if val is None or val == "" else val


def _make_ssl_context(cfg, section):
    ca = cfg.get(section, "tls_ca_file", fallback="").strip() or None
    verify = cfg.getboolean(section, "tls_verify", fallback=True)
    ctx = ssl.create_default_context(cafile=ca)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    if not verify:
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        log.warning("[%s] tls_verify = false: the TLS connection is encrypted but the "
                    "server identity is NOT verified (man-in-the-middle possible)", section)
    cert = cfg.get(section, "tls_cert_file", fallback="").strip()
    if cert:
        ctx.load_cert_chain(cert, cfg.get(section, "tls_key_file", fallback="").strip() or None)
    return ctx


def _backend_settings(cfg, section, env_prefix, role, credentials):
    host = _env(env_prefix + "HOST", cfg.get(section, "host", fallback="")).strip()
    if not host:
        raise ConfigError("[%s] host is required" % section)
    port = int(_env(env_prefix + "PORT", cfg.get(section, "port", fallback="3306")))
    if not 0 < port < 65536:
        raise ConfigError("[%s] port out of range" % section)
    use_tls = cfg.getboolean(section, "use_tls", fallback=False)
    tls_mode = cfg.get(section, "tls_mode", fallback="mysql").strip().lower()
    if tls_mode not in ("mysql", "direct"):
        raise ConfigError("[%s] tls_mode must be 'mysql' or 'direct'" % section)
    kwargs = dict(
        connect_timeout=cfg.getfloat(section, "connect_timeout_sec",
                                     fallback=5.0 if section == "primary" else 3.0),
        use_tls=use_tls, tls_mode=tls_mode, role=role,
        server_name=cfg.get(section, "tls_server_name", fallback="").strip() or None)
    if use_tls:
        kwargs["ssl_context"] = _make_ssl_context(cfg, section)
    if credentials:
        user = _env(env_prefix + "USER", cfg.get(section, "user", fallback="")).strip()
        if not user:
            raise ConfigError("[%s] user is required" % section)
        password = os.environ.get(env_prefix + "PASSWORD")
        pw_file = _env(env_prefix + "PASSWORD_FILE", cfg.get(section, "password_file", fallback="")).strip()
        if password is None and pw_file:
            try:
                with open(pw_file, "r", encoding="utf-8") as fh:
                    password = fh.readline().rstrip("\r\n")
            except OSError as e:
                raise ConfigError("[%s] cannot read password_file: %s" % (section, e))
        if password is None:
            password = cfg.get(section, "password", fallback="")
        kwargs.update(user=user, password=password,
                      database=cfg.get(section, "database", fallback="").strip() or None)
    return BackendSettings(host, port, **kwargs)


def build_config(cfg):
    """Validate a ConfigParser and return a BridgeConfig (raises ConfigError)."""
    for section in ("bridge", "primary", "replica"):
        if not cfg.has_section(section):
            raise ConfigError("missing [%s] section" % section)
    c = BridgeConfig()
    try:
        g = lambda key, fb: cfg.get("bridge", key, fallback=fb)
        c.listen_host = _env("BRIDGE_LISTEN_HOST", g("listen_host", "127.0.0.1")).strip()
        c.listen_port = int(_env("BRIDGE_LISTEN_PORT", g("listen_port", "33060")))
        c.max_clients = cfg.getint("bridge", "max_clients", fallback=256)
        c.pool_size = cfg.getint("bridge", "replica_pool_size", fallback=16)
        c.route_replica = cfg.getboolean("bridge", "route_selects_to_replica", fallback=True)
        c.grace = cfg.getfloat("bridge", "read_your_writes_grace_sec", fallback=0.0)
        c.idle_timeout = cfg.getfloat("bridge", "client_idle_timeout_sec", fallback=600.0)
        c.auth_timeout = cfg.getfloat("bridge", "auth_timeout_sec", fallback=10.0)
        c.acquire_timeout = cfg.getfloat("bridge", "replica_acquire_timeout_sec", fallback=0.25)
        c.replica_query_timeout = cfg.getfloat("bridge", "replica_query_timeout_sec", fallback=30.0)
        c.replica_buffer = int(cfg.getfloat("bridge", "replica_buffer_kb", fallback=1024) * 1024)
        c.stats_interval = cfg.getfloat("bridge", "stats_interval_sec", fallback=30.0)
        c.max_packet = int(cfg.getfloat("bridge", "max_packet_mb", fallback=64) * 1024 * 1024)
        c.use_uvloop = cfg.getboolean("bridge", "use_uvloop", fallback=True)
        c.log_level = _env("BRIDGE_LOG_LEVEL", g("log_level", "INFO")).strip().upper()
        allowed = g("allowed_clients", "").strip()
        c.allowed_clients = [ipaddress.ip_network(a.strip(), strict=False)
                             for a in allowed.split(",") if a.strip()]
        c.primary = _backend_settings(cfg, "primary", "BRIDGE_PRIMARY_", "primary", False)
        c.replica = _backend_settings(cfg, "replica", "BRIDGE_REPLICA_", "replica", True)
    except (ValueError, configparser.Error, ssl.SSLError, OSError) as e:
        raise ConfigError(str(e))
    if not 0 <= c.listen_port < 65536:
        raise ConfigError("[bridge] listen_port out of range")
    for name in ("max_clients", "pool_size"):
        if getattr(c, name) < 1:
            raise ConfigError("[bridge] %s must be >= 1" % name)
    for name in ("grace", "idle_timeout"):
        if getattr(c, name) < 0:
            raise ConfigError("[bridge] negative value for %s" % name)
    for name in ("auth_timeout", "acquire_timeout", "replica_query_timeout"):
        if getattr(c, name) <= 0:
            raise ConfigError("[bridge] %s must be > 0" % name)
    if not isinstance(logging.getLevelName(c.log_level), int):
        raise ConfigError("unknown log_level %r" % c.log_level)
    if c.max_packet < 1024 * 1024:
        raise ConfigError("[bridge] max_packet_mb must be >= 1")
    c.uvloop_active = False
    return c


def _check_config_permissions(path, cfg):
    try:
        mode = os.stat(path).st_mode
    except OSError:
        return
    if cfg.get("replica", "password", fallback="") and mode & (stat.S_IRWXG | stat.S_IRWXO):
        log.warning("%s contains a password but is readable by other users; run "
                    "'chmod 600 %s' or use BRIDGE_REPLICA_PASSWORD / password_file", path, path)


def _warn_exposure(conf):
    hosts = ["127.0.0.1"] if conf.listen_host == "localhost" else conf.listen_host.split(",")
    for h in hosts:
        h = h.strip()
        try:
            loopback = ipaddress.ip_address(h).is_loopback
        except ValueError:
            loopback = h == "localhost"
        if not loopback and not conf.allowed_clients:
            log.warning("listening on non-loopback address %s without allowed_clients; make "
                        "sure a firewall restricts who can reach the bridge", h)


async def main_async(conf, stop_event=None):
    bridge = Bridge(conf)
    await bridge.start()
    stop = stop_event or asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except (NotImplementedError, RuntimeError, ValueError):
            pass
    try:
        await stop.wait()
    finally:
        await bridge.stop()


def main(argv=None):
    ap = argparse.ArgumentParser(description="MySQL/MariaDB read/write-splitting bridge")
    ap.add_argument("config", nargs="?",
                    default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "bridge.cfg"))
    ap.add_argument("--check-config", action="store_true",
                    help="validate the configuration and exit")
    args = ap.parse_args(argv)
    logging.basicConfig(level=os.environ.get("BRIDGE_LOG_LEVEL", "INFO").upper(),
                        format="%(asctime)s %(levelname)s %(message)s")
    cfg = configparser.ConfigParser(interpolation=None)
    try:
        with open(args.config, "r", encoding="utf-8") as fh:
            cfg.read_file(fh)
        conf = build_config(cfg)
    except (OSError, configparser.Error, ConfigError) as e:
        log.error("invalid config %s: %s", args.config, e)
        return 2
    logging.getLogger().setLevel(conf.log_level)
    _check_config_permissions(args.config, cfg)
    _warn_exposure(conf)
    if args.check_config:
        log.info("config %s OK", args.config)
        return 0
    runner = asyncio.run
    if conf.use_uvloop:
        try:
            import uvloop  # type: ignore
            conf.uvloop_active = True
            if sys.version_info >= (3, 12):
                runner = lambda coro: asyncio.run(coro, loop_factory=uvloop.new_event_loop)  # novermin
            else:
                asyncio.set_event_loop_policy(uvloop.EventLoopPolicy())
        except ImportError:
            pass
    try:
        runner(main_async(conf))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
