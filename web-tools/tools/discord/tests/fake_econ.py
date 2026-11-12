"""
Fake Teeworlds external console (Econ) for the bridge tests.

It mimics the parts of the real server the bridge depends on: the password
prompt, the NUL padded line endings, the console log line format and the way
the console splits and parses a `say "..."` command (src/engine/shared/console.cpp),
so the tests can check what a game server would really execute.
"""
import asyncio
from typing import List, Optional, Tuple

TIMESTAMP = "2026-01-01 12:00:00"


def split_commands(line: str) -> List[str]:
    """CConsole::ExecuteLineStroked: split on ';' outside of quotes, '#' starts a comment."""
    commands = []
    pos = 0
    while pos < len(line):
        end = pos
        in_string = False
        next_part = None
        while end < len(line):
            ch = line[end]
            if ch == '"':
                in_string = not in_string
            elif ch == '\\':
                if end + 1 < len(line) and line[end + 1] == '"':
                    end += 1
            elif not in_string:
                if ch == ';':
                    next_part = end + 1
                    break
                if ch == '#':
                    break
            end += 1
        commands.append(line[pos:end])
        if next_part is None:
            break
        pos = next_part
    return [command for command in commands if command.strip()]


def parse_command(command: str) -> Tuple[str, Optional[str]]:
    """CConsole::ParseStart/ParseArgs for a command with one 'r' (rest of the line) argument."""
    command = command.lstrip()
    name, _, rest = command.partition(" ")
    rest = rest.lstrip()
    if not rest:
        return name, None
    if not rest.startswith('"'):
        return name, rest

    out = []
    i = 1
    while True:
        if i >= len(rest):
            return name, None  # unterminated string: the console rejects the command
        ch = rest[i]
        if ch == '"':
            break
        if ch == '\\' and i + 1 < len(rest) and rest[i + 1] in ('\\', '"'):
            i += 1
            ch = rest[i]
        out.append(ch)
        i += 1
    return name, "".join(out)


class FakeEconServer:
    def __init__(self, password: str = "secret", echo_say: bool = True):
        self.password = password
        self.echo_say = echo_say
        self.port = 0
        self.raw_lines: List[str] = []                      # every line received after authentication
        self.executed: List[Tuple[str, Optional[str]]] = []  # (command, argument) the console would run
        self.auth_attempts = 0
        self.connections = 0
        self._server: Optional[asyncio.AbstractServer] = None
        self._clients: List[asyncio.StreamWriter] = []
        self._changed = asyncio.Event()

    async def start(self) -> "FakeEconServer":
        self._server = await asyncio.start_server(self._handle, "127.0.0.1", self.port)
        self.port = self._server.sockets[0].getsockname()[1]
        return self

    async def stop(self):
        if self._server is not None:
            self._server.close()
        await self.drop_clients()
        if self._server is not None:
            await self._server.wait_closed()
            self._server = None

    async def drop_clients(self):
        clients, self._clients = self._clients, []
        for writer in clients:
            writer.close()
        await asyncio.sleep(0)

    @property
    def authed_clients(self) -> int:
        return len(self._clients)

    @property
    def said(self) -> List[str]:
        return [argument for name, argument in self.executed if name == "say"]

    def _write(self, writer: asyncio.StreamWriter, line: str):
        # CConsoleNetConnection::Send always writes three bytes after the line
        writer.write(line.encode("utf-8") + b"\n\x00\x00")

    def emit(self, system: str, text: str):
        """Prints a console log line to every authenticated client."""
        for writer in list(self._clients):
            self._write(writer, f"{TIMESTAMP} I {system}: {text}")

    def emit_raw(self, line: str):
        for writer in list(self._clients):
            self._write(writer, line)

    def player_chat(self, nickname: str, text: str, client_id: int = 0):
        self.emit("chat", f"{client_id}:0:{nickname}: {text}")

    def system_chat(self, text: str):
        self.emit("chat", f"*** {text}")

    async def wait_for(self, predicate, timeout: float = 3.0):
        """Waits until predicate() is true; fails the test on timeout."""
        deadline = asyncio.get_running_loop().time() + timeout
        while not predicate():
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0:
                raise AssertionError("fake econ: condition not met in time")
            self._changed.clear()
            try:
                await asyncio.wait_for(self._changed.wait(), timeout=min(remaining, 0.05))
            except asyncio.TimeoutError:
                pass

    async def _handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        self.connections += 1
        self._changed.set()
        authed = False
        try:
            self._write(writer, "Enter password:")
            while True:
                data = await reader.readline()
                if not data:
                    break
                line = data.decode("utf-8", "replace").strip("\r\n")
                if not line:
                    continue  # the real console skips empty lines
                if not authed:
                    self.auth_attempts += 1
                    if line == self.password:
                        authed = True
                        self._write(writer, "Authentication successful. External console access granted.")
                        self._clients.append(writer)
                    else:
                        self._write(writer, f"Wrong password {self.auth_attempts}/3.")
                    self._changed.set()
                    continue

                self.raw_lines.append(line)
                for command in split_commands(line):
                    name, argument = parse_command(command)
                    self.executed.append((name, argument))
                    if name == "say" and argument is not None and self.echo_say:
                        self.system_chat(argument)
                self._changed.set()
        except (ConnectionError, OSError):
            pass
        finally:
            if writer in self._clients:
                self._clients.remove(writer)
            writer.close()
            self._changed.set()
