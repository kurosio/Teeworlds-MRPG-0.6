#!/usr/bin/env python3
"""
MySQL/MariaDB read/write-splitting bridge for Teeworlds-MRPG servers.

Routes read-only queries (SELECT/SHOW/DESCRIBE/EXPLAIN) to a local MariaDB
replica and everything else to the central primary over the WAN, so a single
central database can serve game servers in several locations with minimal
added latency:

    game server (location A) --\\
    game server (location B) ---+--> local bridge --> [local replica] --\\
    game server (location C) --/    (this script)                       +--> replication --> CENTRAL PRIMARY
                                                                      --/

Design notes
------------
* Every game-server connection gets its own connection to the primary; the
  login is relayed to (and verified by) the primary.  Nothing is ever sent to
  the replica before the primary has accepted the login.
* A read is only served by the replica when that is provably equivalent to
  running it on the primary session: not inside a transaction, no session
  state the replica cannot see (user variables, temporary tables, LOCK
  TABLES, LAST_INSERT_ID(), SHOW WARNINGS, ...), a single statement, no
  locking clause, and no recent write to a table it references (configurable
  "read your writes" window).  Everything else goes to the primary.
* The primary's responses are parsed incrementally (without copying) to track
  transaction state and to know exactly when a response has ended, so reads
  can never overtake an in-flight primary response.
* Responses are forwarded as raw byte chunks - no per-row decode/re-encode.
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

__version__ = "2.0"

log = logging.getLogger("mysql_bridge")
monotonic = time.monotonic

# --- MySQL/MariaDB wire protocol constants ----------------------------------
COM_QUIT = 0x01
COM_INIT_DB = 0x02
COM_QUERY = 0x03
COM_FIELD_LIST = 0x04
COM_PROCESS_INFO = 0x0A
COM_PING = 0x0E
COM_CHANGE_USER = 0x11
COM_STMT_PREPARE = 0x16
COM_STMT_EXECUTE = 0x17
COM_STMT_SEND_LONG_DATA = 0x18
COM_STMT_CLOSE = 0x19
COM_STMT_RESET = 0x1A
COM_SET_OPTION = 0x1B
COM_RESET_CONNECTION = 0x1F
COM_STMT_BULK_EXECUTE = 0xFA

# Commands answered by exactly one packet (OK / ERR / EOF / string).
_SINGLE_PACKET_CMDS = frozenset((0x02, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0C, 0x0D,
                                 0x0E, 0x1A, 0x1B, 0x1F))
# Commands without any server response.
_NO_RESPONSE_CMDS = frozenset((COM_STMT_SEND_LONG_DATA, COM_STMT_CLOSE))

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

MARIADB_CLIENT_PROGRESS = 0x01
MARIADB_CLIENT_EXTENDED_METADATA = 0x08
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
# Of those, the ones that change the response format and therefore must be
# supported by the replica (otherwise reads for that session use the primary).
FORMAT_CAPS = CLIENT_SESSION_TRACK | CLIENT_DEPRECATE_EOF
FORMAT_EXT_CAPS = MARIADB_CLIENT_EXTENDED_METADATA
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


class ProfileError(Exception):
    """The replica cannot serve sessions with this capability/session profile."""


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
    """Shorten SQL for logs, masking passwords.  Prefer describe_sql() which
    removes every literal (game queries contain password hashes)."""
    text = " ".join(sql_text.split())
    text = PASSWORD_RE.sub(lambda mo: mo.group(1) + "'***'", text)
    return text[:limit] + ("..." if len(text) > limit else "")


def describe_sql(code, limit=160):
    """Log-safe SQL: *code* is the literal-free form from classify_query()."""
    text = " ".join(code.decode("utf-8", "replace").split())
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


async def write_packet(writer, seq, payload):
    writer.write(encode_packet(seq, payload))
    await writer.drain()


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
R_RESULT, R_SINGLE, R_PREPARE, R_FIELDLIST = range(4)
(_S_IDLE, _S_FIRST, _S_COLS, _S_COLEOF, _S_ROWS, _S_PREP_PARAMS, _S_PREP_PEOF,
 _S_PREP_COLS, _S_PREP_CEOF, _S_FIELDS) = range(10)


class ResponseScanner:
    """Follows server responses in a raw byte stream without copying it.

    expect() registers the response of each command sent to the server;
    feed() consumes bytes in arbitrary chunks.  The scanner knows when each
    response is complete (busy), reports server status flags (transaction
    state), LOCAL INFILE requests, and calls the per-command on_ok callback
    when a response ends successfully.  Only the few packets whose content
    matters (response heads and terminators) are looked at; rows are skipped
    by length.
    """

    __slots__ = ("deprecate_eof", "cache_metadata", "on_status", "on_infile",
                 "_queue", "_cur", "_state", "_buf", "_skip", "_cont",
                 "_pk_first", "_pk_len", "_remaining", "_prep_cols", "_fresh",
                 "_first_payload", "first_byte", "desynced", "unsolicited")

    def __init__(self, deprecate_eof=False, cache_metadata=False, on_status=None,
                 on_infile=None):
        self.deprecate_eof = deprecate_eof
        self.cache_metadata = cache_metadata
        self.on_status = on_status
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
        self._prep_cols = 0
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

    def _status(self, status):
        if self.on_status is not None:
            self.on_status(status)

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
        if st == _S_FIELDS:
            return first == 0xFE or first == 0xFF
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
                self._status(status)
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
                    self._status(status)
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
                if first == 0x00:
                    self._status(ok_status(payload))
                self._finish(True)
                return
            if kind == R_PREPARE:
                if first != 0x00 or len(payload) < 9:
                    self._finish(False)
                    return
                self._prep_cols = payload[5] | (payload[6] << 8)
                params = payload[7] | (payload[8] << 8)
                if params:
                    self._remaining = params
                    self._state = _S_PREP_PARAMS
                else:
                    self._prep_columns()
                return
            if kind == R_FIELDLIST:
                if first == 0xFE and (self.deprecate_eof or length < 9):
                    self._finish(True)
                else:
                    self._state = _S_FIELDS
                return
            self._desync("unknown response kind")
            return
        if st == _S_PREP_PARAMS:
            self._remaining -= 1
            if self._remaining <= 0:
                if self.deprecate_eof:
                    self._prep_columns()
                else:
                    self._state = _S_PREP_PEOF
            return
        if st == _S_PREP_PEOF:
            self._prep_columns()
            return
        if st == _S_PREP_COLS:
            self._remaining -= 1
            if self._remaining <= 0:
                if self.deprecate_eof:
                    self._finish(True)
                else:
                    self._state = _S_PREP_CEOF
            return
        if st == _S_PREP_CEOF:
            self._finish(True)
            return
        if st == _S_FIELDS:
            if first == 0xFF:
                self._finish(False)
            elif first == 0xFE:
                self._finish(True)

    def _prep_columns(self):
        if self._prep_cols:
            self._remaining = self._prep_cols
            self._state = _S_PREP_COLS
        else:
            self._finish(True)


# --- query classification -------------------------------------------------------
K_READ = "read"              # may be served by the replica
K_PRIMARY_READ = "pread"     # read-only, but must run on the primary session
K_WRITE = "write"            # modifies data (opens the read-your-writes window)
K_SET = "set"                # SET ...: session state
K_USE = "use"                # USE db
K_TX = "tx"                  # transaction / lock control
K_UNKNOWN = "unknown"        # not parseable with confidence (multi-statement, ...)

# String literals / identifiers / comments.  Executable comments (/*! */,
# /*M! */) are deliberately NOT treated as comments: their content runs.
_LITERAL_RE = re.compile(
    rb"'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\"|`[^`]*`|/\*(?![!M]).*?\*/|(?:#|--(?=\s|$))[^\n]*",
    re.S)
_SIMPLE_IDENT_RE = re.compile(rb"[A-Za-z0-9_$]+\Z")
_FIRST_WORD_RE = re.compile(rb"[\s(]*([A-Za-z]+)")
_WORD_RE = re.compile(rb"[a-z0-9_$]+")


def _literal_sub(m):
    s = m.group()
    c = s[0]
    if c == 0x60:                                            # `identifier`
        inner = s[1:-1]
        return inner if _SIMPLE_IDENT_RE.match(inner) else b"?"
    if c == 0x27 or c == 0x22:
        return b"?"
    return b" "


_UNSAFE_SELECT_RE = re.compile(
    rb"@|\binto\b|\bfor\s+update\b|\bfor\s+share\b|\block\s+in\s+share\s+mode\b"
    rb"|\bsql_calc_found_rows\b|\b(?:next|previous)\s+value\s+for\b"
    rb"|\bcurrent_(?:user|role)\b|\bsession_user\b|\bsystem_user\b"
    rb"|\bprocesslist\b|\bperformance_schema\b"
    rb"|\b(?:last_insert_id|found_rows|row_count|get_lock|release_lock|release_all_locks"
    rb"|is_free_lock|is_used_lock|connection_id|nextval|lastval|setval|load_file"
    rb"|database|schema|user|master_pos_wait|source_pos_wait)\s*\(")
_DML_WORD_RE = re.compile(rb"\b(?:insert|update|delete|replace)\b")
_EXPLAIN_UNSAFE_RE = re.compile(
    rb"@|\b(?:insert|update|delete|replace|analyze|extended|connection)\b")
_SHOW_RE = re.compile(rb"show\s+(?:(?:full|extended)\s+)*([a-z_]+)(?:\s+([a-z_]+))?")
_SHOW_SAFE = frozenset((b"tables", b"columns", b"fields", b"index", b"indexes", b"keys",
                        b"triggers", b"events", b"charset", b"collation", b"engines"))
_SHOW_SAFE_PAIRS = frozenset(((b"table", b"status"), (b"character", b"set")))
_SHOW_CREATE_SAFE = frozenset((b"table", b"view", b"trigger", b"procedure", b"function",
                               b"event", b"sequence", b"database", b"schema"))
_WRITE_WORDS = frozenset((b"insert", b"update", b"delete", b"replace", b"load", b"create",
                          b"alter", b"drop", b"rename", b"truncate", b"call", b"grant",
                          b"revoke", b"do", b"handler", b"optimize", b"repair", b"analyze",
                          b"flush", b"purge", b"reset", b"kill", b"install", b"uninstall",
                          b"execute", b"prepare", b"deallocate", b"backup", b"binlog",
                          b"change", b"stop", b"cache", b"signal", b"resignal"))
_TX_WORDS = frozenset((b"begin", b"start", b"commit", b"rollback", b"savepoint",
                       b"release", b"xa", b"lock", b"unlock"))
_PRIMARY_READ_WORDS = frozenset((b"values", b"table", b"help", b"check", b"checksum"))

_WRITE_TABLE_RE = re.compile(
    rb"\s*(?:(?:insert|replace)\s+(?:(?:low_priority|delayed|high_priority|ignore)\s+)*(?:into\s+)?"
    rb"([a-z0-9_$.]+)"
    rb"|update\s+(?:(?:low_priority|ignore)\s+)*([a-z0-9_$.]+)\s+set\b"
    rb"|delete\s+(?:(?:low_priority|quick|ignore)\s+)*from\s+([a-z0-9_$.]+)(?![^;]*\busing\b))")
_TEMP_TABLE_RE = re.compile(rb"\s*create\s+(?:or\s+replace\s+)?temporary\b")
_LOCK_TABLES_RE = re.compile(rb"\s*lock\s+tables?\b")
_UNLOCK_TABLES_RE = re.compile(rb"\s*unlock\s+tables?\b")
_USE_RE = re.compile(rb"\s*use\s+([^\s;]+)\s*\Z", re.I)
_SET_NOT_REPLAYABLE_RE = re.compile(
    rb"(?<!@)@(?!@)|\bglobal\b|@@global\.[a-z0-9_$.]+\s*:?=|\btransaction\b|\bpassword\b"
    rb"|\brole\b|\bstatement\b")


def _strip_literals(sql):
    """Replace literals/comments; None if the text cannot be lexed safely."""
    code = _LITERAL_RE.sub(_literal_sub, sql)
    if b"'" in code or b'"' in code or b"`" in code or b"/*" in code:
        return None                    # unterminated literal or executable comment
    return code


def classify_query(sql):
    """Classify a COM_QUERY text (bytes).

    Returns (kind, code, low): *code* is the statement with literals and
    comments removed (safe to log, original case), *low* its lowercase form.
    """
    if isinstance(sql, memoryview):
        sql = sql.tobytes()
    code = _strip_literals(sql)
    if code is None:
        return K_UNKNOWN, b"", b""
    code = code.strip().rstrip(b" \t\r\n\f\v;")
    if b";" in code:
        return K_UNKNOWN, code, code.lower()                 # multi-statement
    m = _FIRST_WORD_RE.match(code)
    if not m:
        return K_UNKNOWN, code, code.lower()
    low = code.lower()
    word = m.group(1).lower()
    if word == b"select" or word == b"with":
        if _UNSAFE_SELECT_RE.search(low):
            return K_PRIMARY_READ, code, low
        if word == b"with" and _DML_WORD_RE.search(low):
            return K_WRITE, code, low
        return K_READ, code, low
    if word == b"show":
        sm = _SHOW_RE.match(low, m.start(1))
        if sm and b"@" not in low:
            w1, w2 = sm.group(1), sm.group(2)
            if (w1 in _SHOW_SAFE or (w1, w2) in _SHOW_SAFE_PAIRS
                    or (w1 == b"create" and w2 in _SHOW_CREATE_SAFE)):
                return K_READ, code, low
        return K_PRIMARY_READ, code, low
    if word in (b"desc", b"describe", b"explain"):
        if _EXPLAIN_UNSAFE_RE.search(low):
            return K_PRIMARY_READ, code, low
        return K_READ, code, low
    if word == b"set":
        return K_SET, code, low
    if word == b"use":
        return K_USE, code, low
    if word in _TX_WORDS:
        return K_TX, code, low
    if word in _PRIMARY_READ_WORDS:
        return K_PRIMARY_READ, code, low
    return K_WRITE, code, low


def _norm_table(name):
    return name.rsplit(b".", 1)[-1]


def write_tables(low):
    """Target table of a simple single-table INSERT/REPLACE/UPDATE/DELETE, as
    a 1-tuple, or None when it cannot be determined (treated as 'any table')."""
    m = _WRITE_TABLE_RE.match(low)
    if not m:
        return None
    name = m.group(1) or m.group(2) or m.group(3)
    return (_norm_table(name),) if name else None


class WriteTracker:
    """Bridge-wide memory of recent writes for the read-your-writes window.

    The game server uses a pool of connections, so a SELECT that depends on
    an INSERT usually arrives on a *different* connection (e.g. account
    registration: INSERT on worker A, SELECT ... ORDER BY ID DESC on worker
    B).  With scope 'table', reads that reference a table written within the
    grace window go to the primary; unattributable writes (DDL, CALL, ...)
    send all reads to the primary for the window.
    """

    def __init__(self, grace, scope):
        self.grace = grace
        self.scope = scope
        self.last_any = -1e18
        self.tables = {}
        self._next_prune = 0.0

    def note(self, tables, now):
        if self.grace <= 0 or self.scope == "connection":
            return
        if tables is None or self.scope == "global":
            self.last_any = now
            return
        for t in tables:
            self.tables[t] = now

    def blocks(self, low, now):
        if self.grace <= 0 or self.scope == "connection":
            return False
        horizon = now - self.grace
        if self.last_any > horizon:
            return True
        tables = self.tables
        if not tables:
            return False
        if now >= self._next_prune:
            self._next_prune = now + self.grace
            for t in [t for t, ts in tables.items() if ts <= horizon]:
                del tables[t]
            if not tables:
                return False
        for tok in set(_WORD_RE.findall(low)):
            ts = tables.get(tok)
            if ts is not None and ts > horizon:
                return True
        return False


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


def parse_handshake_db(p):
    """Database name from a HandshakeResponse41 (None if absent/unparseable)."""
    try:
        caps = int.from_bytes(p[:4], "little")
        if not caps & CLIENT_CONNECT_WITH_DB:
            return None
        pos = p.index(b"\x00", 32) + 1                       # user name
        if caps & CLIENT_PLUGIN_AUTH_LENENC_DATA:
            n, pos = _lenenc(p, pos)
            pos += n
        elif caps & CLIENT_SECURE_CONNECTION:
            pos += 1 + p[pos]
        else:
            pos = p.index(b"\x00", pos) + 1
        end = p.find(b"\x00", pos)
        db = p[pos:end if end >= 0 else len(p)]
        return db.decode("utf-8") if db else None
    except (ValueError, IndexError, TypeError, UnicodeDecodeError):
        return None


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


async def backend_login(reader, writer, cfg, req_caps, req_ext, charset):
    """Log in to a backend as the bridge itself.  Returns (caps, ext_caps)."""
    _, greet = await read_packet(reader)
    if greet[:1] == b"\xff":
        raise ConnectionError("%s refused connection: %s" % (cfg.role, parse_err(greet)[1]))
    g = parse_greeting(greet)
    missing = req_caps & FORMAT_CAPS & ~g.caps
    if missing:
        raise ProfileError("%s lacks protocol capabilities 0x%08x" % (cfg.role, missing))
    caps = BASE_CLIENT_CAPS | (req_caps & MIRRORED_CAPS & g.caps)
    caps |= req_caps & CLIENT_LONG_PASSWORD                   # mirror CLIENT_MYSQL bit
    ext = 0
    if not caps & CLIENT_LONG_PASSWORD and not g.caps & CLIENT_LONG_PASSWORD:
        if req_ext & FORMAT_EXT_CAPS & ~g.ext_caps:
            raise ProfileError("%s lacks MariaDB extended metadata support" % cfg.role)
        ext = req_ext & FORMAT_EXT_CAPS
    elif req_ext & FORMAT_EXT_CAPS:
        raise ProfileError("%s does not support MariaDB extended capabilities" % cfg.role)
    db = cfg.database.encode("utf-8") if cfg.database else b""
    if db:
        caps |= CLIENT_CONNECT_WITH_DB
    seq = 1
    if cfg.starttls:
        if not g.caps & CLIENT_SSL:
            raise ConnectionError("%s does not support TLS but use_tls = true" % cfg.role)
        caps |= CLIENT_SSL
        writer.write(encode_packet(seq, struct.pack("<IIB", caps, MAX_PACKET_SIZE, charset)
                                   + b"\x00" * 19 + struct.pack("<I", ext)))
        await writer.drain()
        await upgrade_tls(writer, cfg.ssl_context, cfg.server_name)
        seq = 2
    plugin = g.plugin if g.plugin in _AUTH_PLUGINS else "mysql_native_password"
    token = _AUTH_PLUGINS[plugin](cfg.password, g.salt)
    resp = (struct.pack("<IIB", caps, MAX_PACKET_SIZE, charset) + b"\x00" * 19
            + struct.pack("<I", ext) + cfg.user.encode("utf-8") + b"\x00"
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
            return caps, ext
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


async def simple_command(reader, writer, payload, deprecate_eof, want_rows=False):
    """Run one command and read its complete response (packet mode).

    Returns (ok, error_message, column_names, rows) - rows only if want_rows.
    """
    writer.write(encode_packet(0, payload))
    await writer.drain()
    _, first = await read_packet(reader)
    if first[:1] == b"\xff":
        code, msg = parse_err(first)
        return False, "%d: %s" % (code, msg), [], []
    if first[:1] == b"\x00":
        return True, "", [], []
    r = _lenenc(first, 0)
    ncols = r[0] if r else 0
    names = []
    for _ in range(ncols):
        _, coldef = await read_packet(reader)
        pos = 0
        for _ in range(4):                                   # catalog, schema, table, org_table
            ln, pos = _lenenc(coldef, pos)
            pos += ln or 0
        ln, pos = _lenenc(coldef, pos)
        names.append(coldef[pos:pos + (ln or 0)].decode("utf-8", "replace"))
    if not deprecate_eof:
        await read_packet(reader)                            # EOF after column definitions
    rows = []
    while True:
        _, p = await read_packet(reader)
        if p[:1] == b"\xff":
            code, msg = parse_err(p)
            return False, "%d: %s" % (code, msg), names, rows
        if p[:1] == b"\xfe" and (deprecate_eof or len(p) < 9):
            status = ok_status(p) if deprecate_eof else eof_status(p)
            if status & SERVER_MORE_RESULTS_EXISTS:
                raise ProtocolError("unexpected multi-result response")
            return True, "", names, rows
        if want_rows:
            row, pos = [], 0
            for _ in range(ncols):
                ln, pos = _lenenc(p, pos)
                if ln is None:
                    row.append(None)
                else:
                    row.append(p[pos:pos + ln].decode("utf-8", "replace"))
                    pos += ln
            rows.append(row)


# --- replica connection pool ------------------------------------------------------
class BackendConn:
    __slots__ = ("reader", "writer", "profile", "caps", "db", "idle_since", "scanner")

    def __init__(self, reader, writer, profile, caps, db):
        self.reader = reader
        self.writer = writer
        self.profile = profile
        self.caps = caps
        self.db = db
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
                ok, _, _, _ = await simple_command(self.reader, self.writer,
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
    """Persistent replica connections keyed by session profile.

    The profile is (caps, ext_caps, charset, session SET statements): replica
    connections must produce byte-identical framing and the same session
    semantics as the game server's own session.  The default database is
    tracked per connection and switched with COM_INIT_DB when needed.

    All bookkeeping is synchronous (single-threaded event loop), so there is
    no lock and no possibility of slot leaks on failed connects: a slot freed
    by a failure is handed straight to the next waiter.
    """

    def __init__(self, settings, size=8, acquire_timeout=0.25, query_timeout=30.0,
                 buffer_limit=1024 * 1024, idle_ping=30.0, idle_close=300.0,
                 max_lag=0.0, lag_interval=2.0):
        self.cfg = settings
        self.size = max(1, size)
        self.acquire_timeout = acquire_timeout
        self.query_timeout = query_timeout
        self.buffer_limit = buffer_limit
        self.idle_ping = idle_ping
        self.idle_close = idle_close
        self.max_lag = max_lag
        self.lag_interval = lag_interval
        self.total = 0
        self._idle = {}                  # profile -> deque[BackendConn]
        self._waiters = collections.deque()
        self._down_until = 0.0
        self._fails = 0
        self._probing = False
        self._bad_profiles = {}
        self._bad_dbs = {}
        self.lag = None
        self.lag_ok = max_lag <= 0
        self._closed = False

    # -- health --------------------------------------------------------------
    @property
    def is_down(self):
        return monotonic() < self._down_until

    def usable(self):
        return not self._closed and self.lag_ok and monotonic() >= self._down_until

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
            _, fut = self._waiters.popleft()
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
        other_waiter = False
        for i, (profile, fut) in enumerate(self._waiters):
            if fut.done():
                continue
            if profile == conn.profile:
                del self._waiters[i]
                fut.set_result(conn)
                return
            other_waiter = True
        if other_waiter:                 # recycle the slot for another profile
            self._discard(conn)
            return
        self._idle.setdefault(conn.profile, collections.deque()).append(conn)

    def _pop_idle_other(self, profile):
        for p, dq in self._idle.items():
            if p != profile and dq:
                return dq.popleft()
        return None

    async def acquire(self, profile):
        """A connection for *profile*, or None if the replica cannot serve it
        right now (the caller then uses the primary)."""
        now = monotonic()
        if self._closed or now < self._down_until or not self.lag_ok:
            return None
        bad = self._bad_profiles.get(profile)
        if bad is not None:
            if now < bad:
                return None
            del self._bad_profiles[profile]
        dq = self._idle.get(profile)
        while dq:
            conn = dq.pop()                                  # LIFO: warmest connection
            if conn.is_dead():
                self._discard(conn)
                continue
            if now - conn.idle_since > self.idle_ping and not await conn.ping(2.0):
                self._discard(conn)
                continue
            return conn
        if self.total >= self.size:
            victim = self._pop_idle_other(profile)
            if victim is not None:
                victim.close()                               # reuse its slot
            else:
                loop = asyncio.get_running_loop()
                fut = loop.create_future()
                waiter = (profile, fut)
                self._waiters.append(waiter)
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
                        self._waiters.remove(waiter)
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
            conn = await self._connect(profile)
        except asyncio.CancelledError:
            self._hand_slot()
            raise
        except ProfileError as e:
            self._hand_slot()
            self._bad_profiles[profile] = monotonic() + 60.0
            rl_log(logging.WARNING, "replica-profile", "replica cannot serve a session "
                   "profile (%s); those reads use the primary for 60s", e)
            return None
        except Exception as e:
            self._hand_slot()
            self._mark_down(e)
            return None
        finally:
            self._probing = False
        self._mark_up()
        return conn

    async def _connect(self, profile):
        caps, ext, charset, sets = profile
        reader, writer = await self.cfg.open()
        try:
            async with _timeout(self.cfg.connect_timeout):     # TLS + login + SET replay
                neg_caps, _ = await backend_login(reader, writer, self.cfg, caps, ext, charset)
                deof = bool(neg_caps & CLIENT_DEPRECATE_EOF)
                replay = list(sets)
                if any(b"autocommit" in s.lower() for s in sets):
                    replay.append(b"SET autocommit=1")
                for stmt in replay:
                    ok, err, _, _ = await simple_command(reader, writer, bytes([COM_QUERY]) + stmt, deof)
                    if not ok:
                        raise ProfileError("session statement failed on replica (%s)" % err)
        except BaseException:
            _close_writer(writer)
            raise
        STATS["replica_connects"] += 1
        return BackendConn(reader, writer, profile, neg_caps, self.cfg.database)

    # -- queries -----------------------------------------------------------------
    async def read(self, payload, profile, db, client_writer):
        """Serve one COM_QUERY from the replica.  True if the client got the
        response, False if the caller must use the primary instead."""
        if db is None:
            db = self.cfg.database       # session without a default db: use the configured one
        if db is not None:
            bad = self._bad_dbs.get(db)
            if bad is not None and monotonic() < bad:
                return False
        for attempt in (0, 1):
            conn = await self.acquire(profile)
            if conn is None:
                return False
            try:
                if db is not None and conn.db != db:
                    async with _timeout(5.0):
                        ok, err, _, _ = await simple_command(
                            conn.reader, conn.writer,
                            bytes([COM_INIT_DB]) + db.encode("utf-8"), conn.deprecate_eof)
                    if not ok:
                        self.release(conn)
                        self._bad_dbs[db] = monotonic() + 30.0
                        rl_log(logging.WARNING, "replica-db", "replica cannot use database "
                               "%r (%s); its reads use the primary", db, err)
                        return False
                    conn.db = db
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
        for dq in self._idle.values():
            keep = collections.deque()
            while dq:
                conn = dq.popleft()
                if conn.is_dead() or now - conn.idle_since > limit:
                    self._discard(conn)
                else:
                    keep.append(conn)
            dq.extend(keep)
        for p in [p for p, dq in self._idle.items() if not dq]:
            del self._idle[p]

    async def lag_loop(self):
        """Measure replication lag; reads go to the primary while it is too high
        or unknown (replication stopped, monitor failing)."""
        conn = None
        stmt = None
        candidates = [b"SHOW ALL SLAVES STATUS", b"SHOW REPLICA STATUS", b"SHOW SLAVE STATUS"]
        while not self._closed:
            lag = None
            try:
                if conn is None or conn.is_dead():
                    if conn is not None:
                        conn.close()
                    conn = await self._connect((CLIENT_LONG_PASSWORD, 0, 0x21, ()))
                async with _timeout(10.0):
                    for q in ([stmt] if stmt else candidates):
                        ok, err, names, rows = await simple_command(
                            conn.reader, conn.writer, bytes([COM_QUERY]) + q,
                            conn.deprecate_eof, want_rows=True)
                        if ok:
                            stmt = q
                            break
                    else:
                        raise ConnectionError("cannot read replication status (%s); grant "
                                              "REPLICA MONITOR (or REPLICATION CLIENT) to %r"
                                              % (err, self.cfg.user))
                lag = _replication_lag(names, rows)
            except asyncio.CancelledError:
                break
            except Exception as e:
                rl_log(logging.WARNING, "lag-monitor", "replica lag check failed: %s: %s",
                       type(e).__name__, e)
                if conn is not None:
                    conn.close()
                    conn = None
            ok = lag is not None and lag <= self.max_lag
            if ok != self.lag_ok:
                if ok:
                    log.info("replica lag %.0fs within limit (%.0fs); reads use the replica again",
                             lag, self.max_lag)
                else:
                    log.warning("replica lag %s exceeds limit (%.0fs); reads use the primary",
                                "unknown" if lag is None else "%.0fs" % lag, self.max_lag)
            self.lag, self.lag_ok = lag, ok
            try:
                await asyncio.sleep(self.lag_interval)
            except asyncio.CancelledError:
                break
        if conn is not None:
            conn.close()

    async def close(self):
        self._closed = True
        for dq in self._idle.values():
            for conn in dq:
                conn.close()
        self._idle.clear()
        for _, fut in self._waiters:
            if not fut.done():
                fut.cancel()
        self._waiters.clear()


def _first_packet_payload(chunks):
    data = chunks[0] if len(chunks) == 1 else b"".join(chunks)
    length = data[0] | (data[1] << 8) | (data[2] << 16)
    return data[4:4 + length]


def _replication_lag(names, rows):
    """Max Seconds_Behind_Master over all replication channels.
    0 if the server is not a replica; None if replication is broken."""
    if not rows:
        return 0.0
    idx = None
    for key in ("Seconds_Behind_Master", "Seconds_Behind_Source"):
        if key in names:
            idx = names.index(key)
            break
    if idx is None:
        return None
    worst = 0.0
    for row in rows:
        val = row[idx]
        if val is None:
            return None
        worst = max(worst, float(val))
    return worst


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
        self.client_caps = 0
        self.eff_caps = 0
        self.eff_ext = 0
        self.charset = 0x21
        self.db = None
        self.in_trans = False
        self.autocommit = True
        self.last_write = -1e18
        self.session_sets = []
        self.sticky = None               # reason why this session must use the primary
        self.tx_tables = set()
        self.tx_any = False
        self.infile_active = False
        self.stmts = {}
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
        self.client_caps = caps
        self.charset = resp[8]
        client_ext = int.from_bytes(resp[28:32], "little")
        self.eff_caps = caps & server_caps
        if not caps & CLIENT_LONG_PASSWORD and not g.caps & CLIENT_LONG_PASSWORD:
            self.eff_ext = client_ext & g.ext_caps
        self.db = parse_handshake_db(resp)
        self.in_trans = bool(g.status & SERVER_STATUS_IN_TRANS)
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
            cache_metadata=bool(self.eff_ext & MARIADB_CLIENT_CACHE_METADATA),
            on_status=self._on_status, on_infile=self._on_infile)
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
                self._on_status(ok_status(p))
                log.debug("client %s authenticated (caps=0x%08x ext=0x%x cs=%d deof=%s)",
                          self.peer, self.eff_caps, self.eff_ext, self.charset,
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

    def _on_status(self, status):
        was_in_trans = self.in_trans
        self.in_trans = bool(status & SERVER_STATUS_IN_TRANS)
        self.autocommit = bool(status & SERVER_STATUS_AUTOCOMMIT)
        if was_in_trans and not self.in_trans and (self.tx_tables or self.tx_any):
            self.b.writes.note(None if self.tx_any else tuple(self.tx_tables), monotonic())
            self.tx_tables.clear()
            self.tx_any = False

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

    async def _forward(self, seq, p, kind=None, on_ok=None):
        if kind is not None:
            self.scanner.expect(kind, on_ok)
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
            await self._forward(seq, p, R_SINGLE, lambda _p: self._set_db(db))
            return
        if cmd == COM_RESET_CONNECTION:
            await self._forward(seq, p, R_SINGLE, self._reset_session)
            return
        if cmd == COM_STMT_PREPARE:
            kind, _, low = classify_query(p[1:])
            tables = write_tables(low) if kind == K_WRITE else None
            await self._forward(seq, p, R_PREPARE,
                                lambda rp: self._stmt_prepared(rp, kind, tables))
            return
        if cmd in (COM_STMT_EXECUTE, COM_STMT_BULK_EXECUTE):
            if cmd == COM_STMT_EXECUTE and len(p) > 5 and p[5] & 0x07:
                self._relay_only("server-side cursor")
                await self._forward(seq, p)
                return
            info = self.stmts.get(int.from_bytes(p[1:5], "little"))
            if info is not None and info[0] in (K_READ, K_PRIMARY_READ):
                await self._forward(seq, p, R_RESULT)
            else:
                tables = info[1] if info is not None else None
                await self._forward(seq, p, R_RESULT, lambda _p: self._wrote(tables))
            return
        if cmd in _NO_RESPONSE_CMDS:
            if cmd == COM_STMT_CLOSE:
                self.stmts.pop(int.from_bytes(p[1:5], "little"), None)
            await self._forward(seq, p)
            return
        if cmd in _SINGLE_PACKET_CMDS:
            await self._forward(seq, p, R_SINGLE)
            return
        if cmd == COM_FIELD_LIST:
            await self._forward(seq, p, R_FIELDLIST)
            return
        if cmd == COM_PROCESS_INFO:
            await self._forward(seq, p, R_RESULT)
            return
        # COM_CHANGE_USER, binlog/replication commands, cursors ...: stop
        # interpreting this session and relay it verbatim to the primary.
        self._relay_only("command 0x%02x" % cmd)
        await self._forward(seq, p)

    async def _query(self, seq, p):
        b = self.b
        kind, code, low = classify_query(p[1:])
        if kind == K_READ and seq == 0:
            if self._replica_allowed(low):
                served = False
                try:
                    served = await b.pool.read(p, self._profile(), self.db, self.cw)
                except (StreamBroken, asyncio.CancelledError):
                    raise
                except Exception as e:
                    STATS["replica_errors"] += 1
                    rl_log(logging.WARNING, "replica-exc-" + type(e).__name__,
                           "replica read failed (%s: %s); using the primary", type(e).__name__, e)
                if served:
                    STATS["replica"] += 1
                    if log.isEnabledFor(logging.DEBUG):
                        log.debug("replica <- %s", describe_sql(code))
                    return
                STATS["fallback"] += 1
        STATS["primary"] += 1
        if log.isEnabledFor(logging.DEBUG):
            log.debug("primary <- [%s] %s", kind, describe_sql(code))
        on_ok = None
        if kind == K_WRITE:
            tables = write_tables(low)
            temp = low.startswith(b"create") and _TEMP_TABLE_RE.match(low) is not None

            def on_ok(_p, tables=tables, temp=temp):
                self._wrote(tables)
                if temp:
                    self._stick("temporary table")
        elif kind == K_UNKNOWN:
            on_ok = self._wrote_unknown
        elif kind == K_SET:
            if _SET_NOT_REPLAYABLE_RE.search(low) is None:
                stmt = bytes(p[1:]).strip().rstrip(b";").strip()
                on_ok = lambda _p: self._add_session_set(stmt)
        elif kind == K_USE:
            m = _USE_RE.match(code)
            name = m.group(1) if m else b"?"
            if b"?" in name:
                on_ok = lambda _p: self._stick("unparsed USE statement")
            else:
                db = name.decode("utf-8", "replace")
                on_ok = lambda _p: self._set_db(db)
        elif kind == K_TX:
            if _LOCK_TABLES_RE.match(low):
                on_ok = lambda _p: self._stick("LOCK TABLES")
            elif _UNLOCK_TABLES_RE.match(low):
                on_ok = lambda _p: self._unstick("LOCK TABLES")
        await self._forward(seq, p, R_RESULT, on_ok)

    def _replica_allowed(self, low):
        if (self.sticky is not None or self.in_trans or not self.autocommit
                or self.infile_active or self.scanner.busy or not self.authenticated):
            return False
        b = self.b
        if not b.pool.usable():
            return False
        if b.grace > 0:
            now = monotonic()
            if now - self.last_write < b.grace or b.writes.blocks(low, now):
                return False
        return True

    def _profile(self):
        caps = (self.eff_caps & MIRRORED_CAPS & ~CLIENT_LONG_PASSWORD) | (
            self.client_caps & CLIENT_LONG_PASSWORD)
        return (caps, self.eff_ext & FORMAT_EXT_CAPS, self.charset, tuple(self.session_sets))

    # -- session state bookkeeping ----------------------------------------------------
    def _wrote(self, tables):
        now = monotonic()
        self.last_write = now
        if self.in_trans or not self.autocommit:
            if tables is None:
                self.tx_any = True
            else:
                self.tx_tables.update(tables)
            return
        self.b.writes.note(tables, now)

    def _wrote_unknown(self, _p=None):
        self._wrote(None)

    def _set_db(self, db):
        self.db = db

    def _add_session_set(self, stmt):
        if stmt in self.session_sets:
            self.session_sets.remove(stmt)
        self.session_sets.append(stmt)
        if len(self.session_sets) > 16:
            self._stick("too many session SET statements")

    def _stick(self, reason):
        if self.sticky is None:
            log.debug("client %s: reads pinned to primary (%s)", self.peer, reason)
            self.sticky = reason

    def _unstick(self, reason):
        if self.sticky == reason:
            self.sticky = None

    def _relay_only(self, reason):
        if not self.relay_only:
            log.debug("client %s: relaying verbatim from now on (%s)", self.peer, reason)
        self.relay_only = True
        self.sticky = reason

    def _reset_session(self, _p=None):
        self.session_sets = []
        self.stmts.clear()
        self.tx_tables.clear()
        self.tx_any = False
        self.sticky = None

    def _stmt_prepared(self, payload, kind, tables):
        if len(payload) >= 5:
            if len(self.stmts) >= 10000:
                self.stmts.clear()
            self.stmts[int.from_bytes(payload[1:5], "little")] = (kind, tables)


# --- bridge server ----------------------------------------------------------------
class Bridge:
    def __init__(self, conf):
        self.conf = conf
        self.primary = conf.primary
        self.route_replica = conf.route_replica
        self.grace = conf.grace
        self.writes = WriteTracker(conf.grace, conf.ryw_scope)
        self.auth_timeout = conf.auth_timeout
        self.idle_timeout = conf.idle_timeout
        self.max_clients = conf.max_clients
        self.max_packet = conf.max_packet
        self.allowed = conf.allowed_clients
        self.pool = ReplicaPool(
            conf.replica, size=conf.pool_size, acquire_timeout=conf.acquire_timeout,
            query_timeout=conf.replica_query_timeout, buffer_limit=conf.replica_buffer,
            max_lag=conf.max_lag, lag_interval=conf.lag_interval)
        self.sessions = set()
        self.servers = []
        self._tasks = []
        self.ports = []

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
        if conf.max_lag > 0 and conf.route_replica:
            self._tasks.append(loop.create_task(self.pool.lag_loop()))
        log.info("bridge %s listening on %s:%s (uvloop=%s, max_clients=%d, pool=%d, "
                 "replica_reads=%s, ryw=%s/%.1fs, primary=%s:%s tls=%s)",
                 __version__, ",".join(hosts), self.ports[0] if self.ports else conf.listen_port,
                 conf.uvloop_active, conf.max_clients, conf.pool_size, conf.route_replica,
                 conf.ryw_scope, conf.grace, self.primary.host, self.primary.port,
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
            if pool.is_down:
                state = "down"
            elif not pool.lag_ok:
                state = "lagging"
            else:
                state = "up"
            log.info("stats replica=%d primary=%d fallback=%d replica_errors=%d errors=%d "
                     "pool_timeouts=%d clients=%d total_clients=%d rejected=%d pool=%d/%d replica=%s%s",
                     STATS["replica"], STATS["primary"], STATS["fallback"],
                     STATS["replica_errors"], STATS["errors"], STATS["replica_pool_timeouts"], len(self.sessions),
                     STATS["clients_total"], STATS["rejected"], pool.total, pool.size, state,
                     "" if pool.lag is None else " lag=%.0fs" % pool.lag)

    async def serve_until(self, stop_event):
        await stop_event.wait()

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
        c.ryw_scope = g("read_your_writes_scope", "table").strip().lower()
        c.idle_timeout = cfg.getfloat("bridge", "client_idle_timeout_sec", fallback=600.0)
        c.auth_timeout = cfg.getfloat("bridge", "auth_timeout_sec", fallback=10.0)
        c.acquire_timeout = cfg.getfloat("bridge", "replica_acquire_timeout_sec", fallback=0.25)
        c.replica_query_timeout = cfg.getfloat("bridge", "replica_query_timeout_sec", fallback=30.0)
        c.replica_buffer = int(cfg.getfloat("bridge", "replica_buffer_kb", fallback=1024) * 1024)
        c.max_lag = cfg.getfloat("bridge", "max_replica_lag_sec", fallback=0.0)
        c.lag_interval = cfg.getfloat("bridge", "replica_lag_check_interval_sec", fallback=2.0)
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
    if c.ryw_scope not in ("connection", "table", "global"):
        raise ConfigError("[bridge] read_your_writes_scope must be connection, table or global")
    for name in ("max_clients", "pool_size"):
        if getattr(c, name) < 1:
            raise ConfigError("[bridge] %s must be >= 1" % name)
    for name in ("grace", "idle_timeout", "max_lag"):
        if getattr(c, name) < 0:
            raise ConfigError("[bridge] negative value for %s" % name)
    for name in ("auth_timeout", "acquire_timeout", "replica_query_timeout", "lag_interval"):
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
