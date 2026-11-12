# services/econ_client.py
"""
Econ (external console) connection to one game server.

Every configured server gets its own EconClient: it keeps the connection alive
(reconnecting with a backoff), turns the console output into events and lets
the bridge send `say` commands to that server.
"""
import asyncio
import logging
import re
import time
from typing import Awaitable, Callable, List, Optional

from config import EconSettings, ServerConfig
from models.econ import EconBridgeToggle, EconChatMessage, EconEvent, SYSTEM_NICKNAME
from utils.helpers import clean_message

EventHandler = Callable[[EconEvent], Awaitable[None]]

# A console line is "<timestamp> <level> <system>: <text>" ("[<timestamp>][<system>]: <text>"
# on older servers). The match is anchored at the line start on purpose: the system name is
# written by the server, everything after it may be player-controlled text.
_LINE_RE = re.compile(
    r"^(?:"
    r"\d{4}-\d\d-\d\d \d\d:\d\d:\d\d [EWIDT] (?P<new>[^\s:\]]+): "
    r"|\[[^\]]*\]\[(?P<old>[^\]]+)\]: "
    r")(?P<text>.*)$",
    re.DOTALL,
)
# "<client id>:<mode>:<nickname>: <message>"
_PLAYER_CHAT_RE = re.compile(r"^-?\d+:-?\d+:(?P<nick>.+?): (?P<message>.*)$", re.DOTALL)
_SYSTEM_CHAT_PREFIX = SYSTEM_NICKNAME + " "
_BRIDGE_TOGGLE_RE = re.compile(r"^\[BRIDGE_(?P<action>DISABLE|ENABLE)\] (?P<nick>.+)$", re.DOTALL)

class EconAuthError(Exception):
    """The server rejected the Econ password."""


_AUTH_OK_MARKER = "authentication successful"
_AUTH_FAIL_MARKERS = ("wrong password", "authentication failed")


def parse_econ_line(line: str, discord_tag: str) -> Optional[EconEvent]:
    """Parses one console line. Returns None for everything the bridge does not care about."""
    match = _LINE_RE.match(line)
    if not match:
        return None

    system = match.group("new") or match.group("old")
    text = match.group("text")

    if system == "chat":
        if text.startswith(_SYSTEM_CHAT_PREFIX):
            message = text[len(_SYSTEM_CHAT_PREFIX):].strip()
            # `say` sent by the bridge itself comes back as a server message
            if not message or message.startswith(discord_tag):
                return None
            return EconChatMessage(nickname=SYSTEM_NICKNAME, message=message, is_system=True)

        chat = _PLAYER_CHAT_RE.match(text)
        if not chat:
            return None
        nickname = chat.group("nick").strip()
        message = chat.group("message").strip()
        if not nickname or not message:
            return None
        return EconChatMessage(nickname=nickname, message=message)

    if system == "bridge":
        toggle = _BRIDGE_TOGGLE_RE.match(text)
        if not toggle:
            return None
        nickname = toggle.group("nick").strip()
        if not nickname:
            return None
        return EconBridgeToggle(nickname=nickname, enabled=toggle.group("action") == "ENABLE")

    return None


def escape_console_string(text: str) -> str:
    """
    Makes text safe to put between double quotes in a console command.

    Econ is line based and the console splits commands on ';' outside of quotes, so the text
    must stay on one line and must not be able to close the quotes: line breaks become spaces,
    backslashes are doubled and double quotes are replaced.
    """
    text = clean_message(text)
    text = text.replace("\r", " ").replace("\n", " ").replace("\t", " ").strip()
    return text.replace("\\", "\\\\").replace('"', "'")


class EconClient:
    """Connection to the Econ of one game server."""

    def __init__(self, server: ServerConfig, settings: EconSettings, discord_tag: str):
        self.server = server
        self.settings = settings
        self.discord_tag = discord_tag
        self.logger = logging.getLogger(f"TeeworldsBridge.Econ.{server.tag}")

        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._connected = False
        self._write_lock = asyncio.Lock()
        self._task: Optional[asyncio.Task] = None
        self._stopping = False
        self._password_rejected = False

        self.connected_since: Optional[float] = None
        self.last_error: Optional[str] = None

    @property
    def endpoint(self) -> str:
        return f"{self.server.econ_host}:{self.server.econ_port}"

    @property
    def is_connected(self) -> bool:
        return (
            self._connected
            and self._reader is not None and not self._reader.at_eof()
            and self._writer is not None and not self._writer.is_closing()
        )

    @property
    def uptime(self) -> Optional[float]:
        """Seconds since the current connection was authenticated."""
        if not self.is_connected or self.connected_since is None:
            return None
        return time.monotonic() - self.connected_since

    @staticmethod
    def _decode(data: bytes) -> str:
        # the server terminates lines with "\n" (or "\r\n") followed by NUL padding
        return clean_message(data.decode("utf-8", "replace"))

    # --- connection -------------------------------------------------------------------

    async def connect(self) -> bool:
        """Opens the connection and authenticates. Returns True when the console is ready."""
        if self.is_connected:
            return True
        await self._drop("reconnecting", quiet=True)

        self.logger.info(f"🔗 Connecting to Econ {self.endpoint}...")
        self._password_rejected = False
        try:
            self._reader, self._writer = await asyncio.wait_for(
                asyncio.open_connection(self.server.econ_host, self.server.econ_port),
                timeout=self.settings.connect_timeout,
            )
            await asyncio.wait_for(self._authenticate(), timeout=self.settings.auth_timeout)
        except asyncio.TimeoutError:
            await self._drop("timed out while connecting/authenticating", failed=True)
            return False
        except EconAuthError as e:
            self._password_rejected = True
            await self._drop(str(e), failed=True)
            return False
        except (OSError, EOFError, ValueError) as e:
            await self._drop(f"connection failed: {e or type(e).__name__}", failed=True)
            return False

        self._connected = True
        self.connected_since = time.monotonic()
        self.last_error = None
        self.logger.info("✅ Econ connected and authenticated.")
        return True

    async def _authenticate(self):
        """Sends the password and waits for the verdict of the server."""
        self._writer.write(f"{self.server.econ_password}\n".encode("utf-8"))
        await self._writer.drain()

        while True:
            data = await self._reader.readline()
            if not data:
                raise EOFError("connection closed during authentication (banned or too many econ clients?)")
            line = self._decode(data).lower()
            if _AUTH_OK_MARKER in line:
                return
            if any(marker in line for marker in _AUTH_FAIL_MARKERS):
                raise EconAuthError("authentication failed: wrong econ password")
            # "Enter password:" and blank lines are skipped

    async def _drop(self, reason: str, failed: bool = False, quiet: bool = False):
        """Closes the streams and remembers why."""
        was_connected = self._connected
        writer = self._writer
        self._connected = False
        self._reader = None
        self._writer = None
        self.connected_since = None

        if writer is not None:
            try:
                writer.close()
                await asyncio.wait_for(writer.wait_closed(), timeout=2.0)
            except Exception:
                pass

        if failed or was_connected:
            self.last_error = reason
        if quiet:
            return
        if failed and not was_connected:
            self.logger.error(f"❌ Econ {self.endpoint}: {reason}")
        elif was_connected:
            self.logger.warning(f"🔌 Econ connection closed: {reason}")

    async def close(self):
        """Closes the current connection."""
        async with self._write_lock:
            await self._drop("closed by the bridge", quiet=not self._connected)
        self.last_error = None

    # --- reading ----------------------------------------------------------------------

    @property
    def is_running(self) -> bool:
        return self._task is not None and not self._task.done()

    def start(self, handler: EventHandler) -> asyncio.Task:
        """Starts the background task that keeps the connection up (no-op when it is running)."""
        if not self.is_running:
            self._stopping = False
            self._task = asyncio.create_task(self.run(handler), name=f"Econ[{self.server.tag}]")
            self._task.add_done_callback(self._on_task_done)
        return self._task

    def _on_task_done(self, task: asyncio.Task):
        if task.cancelled():
            return
        error = task.exception()
        if error is not None:
            self.logger.error(f"💥 Econ task crashed: {error!r}", exc_info=error)

    async def stop(self):
        """Stops the background task and closes the connection."""
        self._stopping = True
        task = self._task
        # asyncio.wait_for may swallow a cancellation that races with its result (Python < 3.12),
        # so keep cancelling until the task is really gone
        while task is not None and not task.done():
            task.cancel()
            await asyncio.wait({task}, timeout=0.5)
        self._task = None
        await self.close()

    async def run(self, handler: EventHandler):
        """Keeps the connection up until stopped and feeds parsed events to `handler`."""
        rejections = 0
        try:
            while not self._stopping:
                delay = self.settings.reconnect_delay
                try:
                    if await self.connect():
                        rejections = 0
                        await self._read_until_disconnected(handler)
                    elif self._password_rejected:
                        # back off: repeated wrong passwords get the bridge banned (ec_bantime)
                        rejections += 1
                        delay = min(delay * (2 ** min(rejections, 8)), self.settings.reconnect_delay_max)
                except Exception as e:
                    # never let one server's link die for good
                    self.logger.exception(f"💥 Unexpected Econ error: {e}")
                    await self._drop(f"unexpected error: {e!r}", failed=True, quiet=True)
                if self._stopping:
                    break
                self.logger.info(f"Reconnecting to Econ in {delay:.0f}s...")
                await asyncio.sleep(delay)
        finally:
            await self._drop("bridge is shutting down", quiet=True)

    async def _read_until_disconnected(self, handler: EventHandler):
        while self.is_connected and not self._stopping:
            reader = self._reader
            try:
                data = await asyncio.wait_for(reader.readline(), timeout=self.settings.read_timeout)
            except asyncio.TimeoutError:
                # a quiet server and a dead connection look the same: probe it
                if not await self._send_raw(b"\n"):
                    return
                continue
            except ValueError as e:
                self.logger.warning(f"Skipping oversized Econ line: {e}")
                continue
            except (OSError, EOFError) as e:
                await self._drop(f"read error: {e or type(e).__name__}")
                return

            if not data:
                break

            line = self._decode(data)
            if not line:
                continue
            self.logger.debug(f"Econ line: {line}")

            event = parse_econ_line(line, self.discord_tag)
            if event is None:
                continue
            try:
                await handler(event)
            except Exception as e:
                self.logger.exception(f"💥 Econ event handler failed: {e}")

        if self._connected and not self._stopping:
            await self._drop("closed by the server")

    # --- writing ----------------------------------------------------------------------

    async def _send_raw(self, payload: bytes) -> bool:
        async with self._write_lock:
            if not self.is_connected:
                return False
            writer = self._writer
            try:
                writer.write(payload)
                await asyncio.wait_for(writer.drain(), timeout=self.settings.send_timeout)
                return True
            except asyncio.TimeoutError:
                await self._drop("send timed out")
            except OSError as e:
                await self._drop(f"send error: {e or type(e).__name__}")
            return False

    async def say(self, lines: List[str]) -> bool:
        """Says every line in the server chat. Returns False when the server is unreachable."""
        commands = []
        for line in lines:
            escaped = escape_console_string(line)
            if escaped:
                commands.append(f'say "{escaped}"\n')
        if not commands:
            return False
        return await self._send_raw("".join(commands).encode("utf-8"))
