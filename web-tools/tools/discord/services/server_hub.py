# services/server_hub.py
"""
All game servers of the bridge behind one object.

The hub owns one Econ connection per configured server, reports their events
together with the server they came from and broadcasts chat lines to every
server at once.
"""
import asyncio
import logging
from dataclasses import dataclass
from typing import Awaitable, Callable, Dict, Iterable, List, Optional

from config import BridgeConfig, ServerConfig
from models.econ import EconEvent
from services.econ_client import EconClient

logger = logging.getLogger("TeeworldsBridge.Hub")

ServerEventHandler = Callable[[ServerConfig, EconEvent], Awaitable[None]]


@dataclass(frozen=True)
class ServerState:
    """Snapshot of the Econ link of one server."""
    server: ServerConfig
    connected: bool
    uptime: Optional[float]
    last_error: Optional[str]


class ServerHub:
    """Manages the Econ connections of every configured game server."""

    def __init__(self, config: BridgeConfig):
        self.config = config
        self.clients: Dict[str, EconClient] = {
            server.key: EconClient(server, config.econ, config.bridge.discord_tag)
            for server in config.servers
        }

    @property
    def servers(self) -> List[ServerConfig]:
        return [client.server for client in self.clients.values()]

    @property
    def is_running(self) -> bool:
        return any(client.is_running for client in self.clients.values())

    def get_client(self, tag: str) -> Optional[EconClient]:
        return self.clients.get(tag.casefold())

    def is_connected(self, tag: str) -> bool:
        client = self.get_client(tag)
        return client is not None and client.is_connected

    def connected_count(self) -> int:
        return sum(1 for client in self.clients.values() if client.is_connected)

    def states(self) -> List[ServerState]:
        return [
            ServerState(server=client.server, connected=client.is_connected,
                        uptime=client.uptime, last_error=client.last_error)
            for client in self.clients.values()
        ]

    def start(self, handler: ServerEventHandler):
        """Starts (or restarts dead) connection loops; `handler` receives the events of all servers."""
        started = []
        for client in self.clients.values():
            if client.is_running:
                continue

            async def on_event(event: EconEvent, _server: ServerConfig = client.server):
                await handler(_server, event)

            client.start(on_event)
            started.append(client.server.label)
        if started:
            logger.info(f"Econ links started for {len(started)} server(s): {', '.join(started)}")

    async def stop(self):
        """Stops all connection loops and closes the connections."""
        was_running = self.is_running
        await asyncio.gather(*(client.stop() for client in self.clients.values()))
        if was_running:
            logger.info("Econ links stopped.")

    async def broadcast(self, lines: List[str], tags: Optional[Iterable[str]] = None) -> Dict[str, bool]:
        """
        Says `lines` in the chat of every server (or only of the servers in `tags`).
        Returns {tag: delivered}; servers that are offline are reported as False.
        """
        if tags is None:
            clients = list(self.clients.values())
        else:
            wanted = {tag.casefold() for tag in tags}
            clients = [client for key, client in self.clients.items() if key in wanted]
        if not clients or not lines:
            return {client.server.tag: False for client in clients}

        results = await asyncio.gather(*(client.say(lines) for client in clients), return_exceptions=True)

        delivered: Dict[str, bool] = {}
        for client, result in zip(clients, results):
            if isinstance(result, BaseException):
                logger.error(f"Sending to {client.server.label} failed: {result!r}")
                result = False
            delivered[client.server.tag] = bool(result)
        return delivered
