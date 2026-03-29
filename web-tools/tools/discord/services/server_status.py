# services/server_status.py
import asyncio
import aiohttp
import logging
import urllib.parse
from typing import Optional, Dict, Any, Tuple, List, Union
from config import MASTER_SERVER_API_URL, GAME_SERVER_ADDRESS, STATUS_UPDATE_LOG_LEVEL
from utils.helpers import normalize_nickname

class ServerStatusService:
    """Fetches and processes Teeworlds server status from the master server."""

    def __init__(self, session: aiohttp.ClientSession):
        self.http_session = session
        self.logger = logging.getLogger("TeeworldsBridge.StatusService")
        self.logger.setLevel(STATUS_UPDATE_LOG_LEVEL)
        self.player_cache: Dict[str, Dict[str, Any]] = {}
        self._cache_lock = asyncio.Lock()


    async def _fetch_master_server_data(self) -> Optional[Union[Dict, List]]:
        """Fetches the full server list from the master server API."""
        try:
            self.logger.debug(f"Fetching server list from {MASTER_SERVER_API_URL}")
            async with self.http_session.get(MASTER_SERVER_API_URL, timeout=20) as response:
                self.logger.debug(f"API response status: {response.status}")
                response.raise_for_status() # Вызовет исключение для 4xx/5xx

                content_type = response.content_type.lower()
                if 'application/json' not in content_type:
                    self.logger.warning(f"API returned non-JSON content type: {content_type}")
                    text = await response.text()
                    self.logger.warning(f"API Response Text (first 500 chars): {text[:500]}")
                    return None

                data = await response.json()
                self.logger.debug(f"Successfully fetched and parsed JSON data from API.")
                return data

        except aiohttp.ClientResponseError as e:
             self.logger.error(f"API HTTP error fetching server list: Status {e.status}, Message: {e.message}, URL: {e.request_info.url}")
             return None
        except aiohttp.ClientConnectionError as e:
             self.logger.error(f"API connection error fetching server list: {e}")
             return None
        except asyncio.TimeoutError:
            self.logger.error(f"API request timed out fetching server list ({MASTER_SERVER_API_URL}).")
            return None
        except Exception as e:
            self.logger.error(f"Unexpected error fetching/parsing master server data: {e}", exc_info=True)
            return None


    def _find_our_server(self, data: Optional[Union[Dict, List]]) -> Tuple[Optional[Dict[str, Any]], bool]:
        """Finds the specific game server info within the master server data."""
        if not data: return None, False

        servers_list = data.get("servers") if isinstance(data, dict) else data if isinstance(data, list) else []
        if not servers_list:
            self.logger.warning("Master server data is valid but contains no 'servers' list or list is empty.")
            return None, False

        target_addr_simple = GAME_SERVER_ADDRESS
        target_addr_full = f"tw-0.6+udp://{GAME_SERVER_ADDRESS}"

        self.logger.debug(f"Searching for server address: {target_addr_simple} or {target_addr_full}")

        for server in servers_list:
            addresses = server.get("addresses", [])
            if target_addr_full in addresses or target_addr_simple in addresses:
                server_name_log = server.get("info", {}).get("name", "Unnamed")
                self.logger.debug(f"Found matching server: '{server_name_log}' at {GAME_SERVER_ADDRESS}")
                server_info = server.get("info")
                if server_info:
                    return server_info, True
                else:
                    self.logger.warning(f"Server found but 'info' field is missing or null.")
                    return None, True

        self.logger.warning(f"Server address {GAME_SERVER_ADDRESS} not found in the master server list.")
        return None, False


    async def _update_player_cache(self, server_info: Optional[Dict]):
        """Updates the internal player cache based on server info."""
        new_player_data: Dict[str, Dict[str, Any]] = {}
        log_sample = True

        # update cache
        if server_info and "clients" in server_info:
            clients = server_info.get("clients", [])
            if not clients:
                 self.logger.debug("Server info present but no clients listed.")
            else:
                for client in clients:
                    player_name = client.get("name")
                    if player_name:
                        if log_sample:
                            self.logger.debug(f"Sample client data from API: {client}")
                            log_sample = False

                        normalized_name = normalize_nickname(player_name)
                        new_player_data[normalized_name] = {
                            "name": player_name,
                            "clan": client.get("clan", ""),
                            "country": client.get("country", -1),
                            "skin": client.get("skin", {}),
                            "use_custom_color": client.get("use_custom_color", False),
                            "body_color": client.get("color_body", 0),
                            "feet_color": client.get("color_feet", 0),
                        }
        else:
            self.logger.info("No server info or no clients section, clearing player cache.")

        async with self._cache_lock:
             updated_count = len(new_player_data)
             old_count = len(self.player_cache)
             self.player_cache = new_player_data
             
             if old_count != updated_count:
                 self.logger.info(f"Player cache updated: {old_count} -> {updated_count} players.")
             elif updated_count > 0:
                 self.logger.debug(f"Player cache refreshed: {updated_count} players (count unchanged).")
             else:
                 self.logger.debug("Player cache remains empty.")


    async def update_status(self) -> Tuple[Optional[Dict[str, Any]], bool]:
        """Fetches master server data, finds our server, updates cache, and returns results."""
        self.logger.info("Starting server status update cycle...")
        master_data = await self._fetch_master_server_data()
        server_info, server_found = self._find_our_server(master_data)
        await self._update_player_cache(server_info)
        self.logger.info("Server status update cycle finished.")
        return server_info, server_found


    async def get_player_skin_info(self, normalized_nickname: str) -> Optional[Dict[str, Any]]:
         """Retrieves skin information for a player from the cache."""
         async with self._cache_lock:
            player_data = self.player_cache.get(normalized_nickname)

         if player_data:
             self.logger.debug(f"Cache hit for skin info: '{normalized_nickname}'")
             skin_data = player_data.get("skin", {})
             skin_name = str(skin_data.get("name", "default") if isinstance(skin_data, dict) else "default")
             return {
                 "skin_name": skin_name,
                 "use_custom_color": player_data.get("use_custom_color", False),
                 "body_color": player_data.get("body_color", 0),
                 "feet_color": player_data.get("feet_color", 0),
             }
         else:
             self.logger.debug(f"Cache miss for skin info: '{normalized_nickname}'")
             return None


    async def get_player_cache(self) -> Dict[str, Dict[str, Any]]:
        """Returns a copy of the current player cache."""
        async with self._cache_lock:
            return self.player_cache.copy()
