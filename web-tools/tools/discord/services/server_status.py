# services/server_status.py
"""
Server status of every configured game server, taken from the master server.

The master list is downloaded once per cycle and all servers are looked up in
it. The players of each server are cached per server, the bridge uses that
cache for flags and skins.
"""
import asyncio
import logging
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple, Union

import aiohttp

from config import BridgeConfig, ServerConfig
from utils.helpers import normalize_nickname

PlayerCache = Dict[str, Dict[str, Any]]


@dataclass(frozen=True)
class ServerStatus:
    """Result of one status cycle for one server."""
    server: ServerConfig
    found: bool = False                              # listed on the master server
    info: Optional[Dict[str, Any]] = None            # "info" object of the master entry
    players: PlayerCache = field(default_factory=dict)
    master_available: bool = True                    # False when the master list could not be fetched


def strip_address_scheme(address: str) -> str:
    """'tw-0.6+udp://1.2.3.4:8303' -> '1.2.3.4:8303'."""
    return address.split("://", 1)[-1].strip().lower()


class ServerStatusService:
    """Fetches the master server list and keeps per-server player caches."""

    def __init__(self, session: aiohttp.ClientSession, config: BridgeConfig):
        self.http_session = session
        self.master_url = config.status.master_url
        self.servers: List[ServerConfig] = [server for server in config.servers if server.game_address]
        self.logger = logging.getLogger("TeeworldsBridge.StatusService")
        self.logger.setLevel(config.logging.status_level)
        self._player_caches: Dict[str, PlayerCache] = {}
        self._cache_lock = asyncio.Lock()

    @staticmethod
    def _resolve_skin_name(skin_raw: Any) -> str:
        """Returns normalized skin name from API payload."""
        if isinstance(skin_raw, dict):
            return str(skin_raw.get("name", "default") or "default")
        if isinstance(skin_raw, str) and skin_raw.strip():
            return skin_raw.strip()
        return "default"

    @staticmethod
    def _resolve_skin_colors(client: Dict[str, Any], skin_raw: Any) -> Tuple[int, int]:
        """Returns body/feet colors from top-level or nested skin fields."""
        body_color = client.get("color_body", client.get("body_color"))
        feet_color = client.get("color_feet", client.get("feet_color"))

        if isinstance(skin_raw, dict):
            if body_color is None:
                body_color = skin_raw.get("color_body", skin_raw.get("body_color", 0))
            if feet_color is None:
                feet_color = skin_raw.get("color_feet", skin_raw.get("feet_color", 0))

        try:
            return int(body_color or 0), int(feet_color or 0)
        except (TypeError, ValueError):
            return 0, 0

    async def _fetch_master_server_data(self) -> Optional[Union[Dict, List]]:
        """Fetches the full server list from the master server API."""
        try:
            self.logger.debug(f"Fetching server list from {self.master_url}")
            async with self.http_session.get(self.master_url, timeout=aiohttp.ClientTimeout(total=20)) as response:
                response.raise_for_status()

                content_type = (response.content_type or "").lower()
                if "json" not in content_type:
                    self.logger.warning(f"Master server returned non-JSON content type: {content_type}")
                    return None

                return await response.json(content_type=None)

        except aiohttp.ClientResponseError as e:
            self.logger.error(f"Master server HTTP error: status {e.status}, message: {e.message}")
        except aiohttp.ClientError as e:
            self.logger.error(f"Master server connection error: {e}")
        except asyncio.TimeoutError:
            self.logger.error(f"Master server request timed out ({self.master_url}).")
        except Exception as e:
            self.logger.error(f"Unexpected error fetching/parsing master server data: {e}", exc_info=True)
        return None

    @staticmethod
    def _index_by_address(data: Optional[Union[Dict, List]]) -> Dict[str, Dict[str, Any]]:
        """Maps 'host:port' to the master entry of the server registered with that address."""
        servers_list = data.get("servers") if isinstance(data, dict) else data if isinstance(data, list) else None
        index: Dict[str, Dict[str, Any]] = {}
        for entry in servers_list or []:
            if not isinstance(entry, dict):
                continue
            for address in entry.get("addresses") or []:
                if isinstance(address, str):
                    index.setdefault(strip_address_scheme(address), entry)
        return index

    def _build_player_cache(self, server_info: Optional[Dict[str, Any]]) -> PlayerCache:
        """Builds the player cache of one server from its master info."""
        players: PlayerCache = {}
        clients = server_info.get("clients") if isinstance(server_info, dict) else None
        for client in clients or []:
            if not isinstance(client, dict):
                continue
            player_name = client.get("name")
            if not player_name:
                continue

            skin_raw = client.get("skin", {})
            body_color, feet_color = self._resolve_skin_colors(client, skin_raw)
            players[normalize_nickname(player_name)] = {
                "name": player_name,
                "clan": client.get("clan", ""),
                "country": client.get("country", -1),
                "skin": skin_raw,
                "skin_name": self._resolve_skin_name(skin_raw),
                "body_color": body_color,
                "feet_color": feet_color,
            }
        return players

    async def update_status(self) -> List[ServerStatus]:
        """Fetches the master list once and returns the status of every server with a game address."""
        if not self.servers:
            return []

        self.logger.debug("Starting server status update cycle...")
        master_data = await self._fetch_master_server_data()
        master_available = master_data is not None
        index = self._index_by_address(master_data)
        if master_available and not index:
            self.logger.warning("Master server data contains no servers.")

        statuses: List[ServerStatus] = []
        new_caches: Dict[str, PlayerCache] = {}
        for server in self.servers:
            entry = index.get(strip_address_scheme(server.game_address))
            info = entry.get("info") if entry else None
            if not isinstance(info, dict):
                info = None
            players = self._build_player_cache(info)
            new_caches[server.key] = players
            statuses.append(ServerStatus(server=server, found=entry is not None, info=info,
                                         players=players, master_available=master_available))
            if master_available and entry is None:
                self.logger.warning(f"{server.label} {server.game_address} is not on the master server list.")

        async with self._cache_lock:
            if master_available:
                self._player_caches = new_caches
            # a failed fetch keeps the previous caches: flags/skins stay until the next good cycle

        summary = ", ".join(f"{status.server.label} {len(status.players)}" for status in statuses)
        self.logger.info(f"Server status updated (players online: {summary}).")
        return statuses

    async def get_player(self, tag: str, nickname: str) -> Optional[Dict[str, Any]]:
        """Returns cached data of a player of the given server (None when unknown)."""
        async with self._cache_lock:
            cache = self._player_caches.get(tag.casefold(), {})
            return cache.get(normalize_nickname(nickname)) or cache.get(nickname)

    async def get_player_cache(self, tag: str) -> PlayerCache:
        """Returns a copy of the player cache of the given server."""
        async with self._cache_lock:
            return dict(self._player_caches.get(tag.casefold(), {}))
