# discord_bot/cogs/status.py
import discord
from discord.ext import commands, tasks
import logging
from typing import Optional, Dict, Any
import asyncio
import urllib.parse

# Project imports
from config import (STATUS_CHANNEL_ID, STATUS_MESSAGE_ID, STATUS_UPDATE_INTERVAL_MINUTES,
                    GAME_SERVER_ADDRESS, SHOW_JOIN_BUTTON, STATUS_UPDATE_LOG_LEVEL)
from services.server_status import ServerStatusService
from discord_bot.bot import DiscordBridgeBot

logger = logging.getLogger("TeeworldsBridge.StatusCog")
logger.setLevel(STATUS_UPDATE_LOG_LEVEL)

class StatusCog(commands.Cog):
    """Handles updating the server status message in a dedicated channel."""

    def __init__(self, bot: DiscordBridgeBot, status_service: ServerStatusService):
        self.bot = bot
        self.status_service = status_service
        self.status_channel: Optional[discord.TextChannel] = None
        self.status_message: Optional[discord.Message] = None
        self._ready_event = asyncio.Event()


    @commands.Cog.listener()
    async def on_ready(self):
        """Called once the bot is ready. Finds channel, message, starts loop."""
        logger.info("Status Cog: Bot is ready. Initializing...")

        if STATUS_CHANNEL_ID is None:
            logger.info("Status updates disabled: STATUS_CHANNEL_ID is not set.")
            self._ready_event.set()
            return

        # search channel
        channel = self.bot.get_channel(STATUS_CHANNEL_ID)
        if not isinstance(channel, discord.TextChannel):
            logger.critical(f"Status channel {STATUS_CHANNEL_ID} not found or not text! Status updates disabled.")
            self.status_channel = None
            self._ready_event.set()
            return

        self.status_channel = channel
        logger.info(f"Status Cog found target channel: #{self.status_channel.name} ({STATUS_CHANNEL_ID})")

        # search status message
        await self._fetch_initial_status_message()

        # run updating
        if not self.update_server_status_loop.is_running():
             logger.info("Starting status update loop from on_ready...")
             try:
                 self.update_server_status_loop.start()
                 logger.info("Status update loop started.")
             except RuntimeError as e:
                 logger.error(f"Failed to start status loop (already running?): {e}")
        else:
             logger.warning("Status update loop already running.")

        self._ready_event.set()
        logger.info("Status Cog initialization complete.")

    async def cog_unload(self):
        """Called when the cog is unloaded. Stops the update loop."""
        logger.info("Status Cog unloading...")
        if self.update_server_status_loop.is_running():
             self.update_server_status_loop.cancel()
             logger.info("Server status update loop cancellation requested.")
        logger.info("Status Cog unloaded.")

    async def _fetch_initial_status_message(self):
        """Tries to find the status message on load using ID or pins."""
        if not self.status_channel: return

        # by status message id
        if STATUS_MESSAGE_ID:
            logger.debug(f"Attempting to fetch status message by ID: {STATUS_MESSAGE_ID}")
            try:
                 if self.status_channel.permissions_for(self.status_channel.guild.me).read_message_history:
                    self.status_message = await self.status_channel.fetch_message(STATUS_MESSAGE_ID)
                    logger.info(f"Fetched status message {self.status_message.id} by ID.")
                    return
                 else:
                     logger.warning("Missing 'Read Message History' permission for ID fetch.")
            except discord.NotFound:
                logger.warning(f"Configured status message ID {STATUS_MESSAGE_ID} not found.")
            except discord.Forbidden:
                logger.error("Permission error (Forbidden) fetching message ID.")
            except discord.HTTPException as e:
                logger.error(f"HTTP error fetching message ID {STATUS_MESSAGE_ID}: {e}")

        # by pinned messages
        logger.debug("Checking pinned messages...")
        try:
            my_perms = self.status_channel.permissions_for(self.status_channel.guild.me)
            if not my_perms.read_message_history:
                 logger.warning("Missing 'Read Message History' permission. Cannot check pins.")
                 return

            pins = await self.status_channel.pins()
            logger.debug(f"Found {len(pins)} pinned messages.")
            found_pin = None
            can_manage_pins = my_perms.manage_messages

            for pin in pins:
                if pin.author == self.bot.user and pin.embeds:
                    embed = pin.embeds[0]
                    if embed.title and embed.title.startswith("📊 Server Status:"):
                         if found_pin is None:
                              found_pin = pin
                              logger.info(f"Found potential status message in pins: {found_pin.id}")
                         elif can_manage_pins:
                              logger.warning(f"Found multiple pinned status messages. Unpinning older: {pin.id}")
                              try: await pin.unpin(reason="Removing duplicate status pin")
                              except Exception as unpin_e: logger.error(f"Failed unpinning {pin.id}: {unpin_e}")

            self.status_message = found_pin

        except discord.Forbidden:
            logger.warning("Permission error (Forbidden) fetching pins.")
        except discord.HTTPException as e:
            logger.error(f"HTTP error fetching pins: {e}")
        except Exception as e:
            logger.exception(f"Unexpected error fetching pins: {e}")

        if not self.status_message:
            logger.info("No existing status message found by ID or pins. Will send a new one.")


    @tasks.loop(minutes=STATUS_UPDATE_INTERVAL_MINUTES)
    async def update_server_status_loop(self):
        """Periodically fetches server status and updates the Discord message."""
        logger.info("--- Running periodic server status update ---")
        server_info: Optional[Dict] = None
        server_found: bool = False
        player_cache: Dict = {}
        error_occurred = False

        try:
            server_info, server_found = await self.status_service.update_status()
            player_cache = await self.status_service.get_player_cache()

        except Exception as e:
            logger.exception(f"💥 Unexpected error during status update fetch/process: {e}")
            error_occurred = True

        # creating and send message
        try:
            if error_occurred:
                 embed = discord.Embed(
                     title="📊 Server Status: Update Error",
                     description="An internal error occurred while fetching server status. Please check the logs.",
                     color=discord.Color.dark_red()
                 )
                 embed.timestamp = discord.utils.utcnow()
                 button = None
            else:
                embed = self._build_status_embed(server_info, server_found, player_cache)
                button = self._build_join_button(server_found)

            await self._update_or_send_status_message(embed, button)

        except Exception as e:
             logger.exception(f"💥 Unexpected error during status message update/send: {e}")

        logger.info("--- Status update cycle finished ---")


    @update_server_status_loop.before_loop
    async def before_status_update_loop(self):
        """Waits until the cog's on_ready has finished."""
        await self._ready_event.wait()
        if not self.status_channel and STATUS_CHANNEL_ID is not None:
            logger.error("Status channel not available. Stopping status update loop.")
            self.update_server_status_loop.cancel()
        else:
            logger.info("Status update loop starting.")


    def _build_status_embed(self, server_info: Optional[Dict], server_found: bool, player_cache: Dict) -> discord.Embed:
        """Creates the Discord embed based on current server status."""

        if not server_found:
            return discord.Embed(
                title="📊 Server Status: Not Found",
                description=f"The server `{GAME_SERVER_ADDRESS}` was not found on the master server list.",
                color=discord.Color.red()
            ).set_footer(text="Check server address or master list status.")
        elif not server_info:
            return discord.Embed(
                title="📊 Server Status: Info Unavailable",
                description=f"Found server `{GAME_SERVER_ADDRESS}` but could not retrieve details.",
                color=discord.Color.orange()
            ).set_footer(text="Server might be offline or API unresponsive.")
        else:
            player_count = len(player_cache)
            max_players = server_info.get("max_clients", server_info.get("max_players", "?"))
            map_name_data = server_info.get("map", "Unknown Map")
            map_name = map_name_data.get("name", str(map_name_data)) if isinstance(map_name_data, dict) else str(map_name_data)
            server_name = server_info.get("name", "Unnamed Teeworlds Server")
            game_type = server_info.get("gametype", "Unknown")
            version = server_info.get("version", "")

            description_lines = [
                f"**Map:** `{map_name}` (`{game_type}`)",
                f"**Online:** {player_count} / {max_players}",
            ]
            description_lines.append(f"**Connect:** [`{GAME_SERVER_ADDRESS}`](https://ddnet.org/connect-to/?addr={GAME_SERVER_ADDRESS})")
            description = "\n".join(description_lines)

            embed_color = discord.Color.green() if player_count > 0 else discord.Color.blue()
            embed = discord.Embed(
                title=f"📊 Server Status: {server_name}",
                description=description,
                color=embed_color
            )

            player_list_value = "*Server is currently empty*"
            if player_count > 0:
                player_entries = []
                current_length = 0
                max_field_length = 1020

                # sorting nicknames
                sorted_nicknames = sorted(player_cache.keys(), key=str.lower)

                for norm_nick in sorted_nicknames:
                    player = player_cache[norm_nick]
                    original_name = player.get("name", norm_nick)
                    safe_name = discord.utils.escape_markdown(original_name)
                    try:
                        player_url = f"https://ddnet.org/players/{urllib.parse.quote(original_name)}"
                        entry = f"[{safe_name}]({player_url})"
                    except Exception: entry = safe_name

                    # check limit
                    entry_len = len(entry) + (2 if player_entries else 0)
                    if current_length + entry_len > max_field_length:
                        remaining_count = player_count - len(player_entries)
                        if remaining_count > 0:
                            player_entries.append(f"... and {remaining_count} more")
                        break

                    player_entries.append(entry)
                    current_length += entry_len

                if player_entries: player_list_value = ", ".join(player_entries)

            embed.add_field(name=f"Players Online ({player_count})", value=player_list_value, inline=False)

            embed.timestamp = discord.utils.utcnow()
            embed.set_footer(text=f"Last updated | Updates every {STATUS_UPDATE_INTERVAL_MINUTES} min")
            return embed


    def _build_join_button(self, server_found: bool) -> Optional[discord.ui.Button]:
        """Creates the 'Join Server' button."""
        if SHOW_JOIN_BUTTON and server_found:
            return discord.ui.Button(
                label="Join Server", style=discord.ButtonStyle.url,
                url=f"https://ddnet.org/connect-to/?addr={GAME_SERVER_ADDRESS}", emoji="🎮"
            )
        return None


    async def _update_or_send_status_message(self, embed: discord.Embed, button: Optional[discord.ui.Button]):
        """Edits the existing status message or sends a new one."""
        if not self.status_channel:
            logger.error("Cannot update status message: Status channel unavailable.")
            return

        message_view: Optional[discord.ui.View] = None
        if button:
            message_view = discord.ui.View(timeout=None)
            message_view.add_item(button)

        my_perms = self.status_channel.permissions_for(self.status_channel.guild.me)

        # try edit
        if self.status_message:
            logger.debug(f"Attempting to edit status message {self.status_message.id}")
            if not (my_perms.send_messages and my_perms.embed_links):
                logger.error(f"Cannot edit status message {self.status_message.id}: Missing Send/Embed permission.")
                return
            try:
                await self.status_message.edit(embed=embed, view=message_view)
                logger.debug(f"Edited status message {self.status_message.id}.")
                return
            except discord.NotFound:
                logger.warning(f"Status message {self.status_message.id} not found during edit. Will send new.")
                self.status_message = None
            except discord.Forbidden as e:
                logger.error(f"Permission error editing status message {self.status_message.id}: {e.text}")
                self.status_message = None
                return
            except discord.HTTPException as e:
                logger.error(f"HTTP error editing status message {self.status_message.id}: {e}")
                return

        # try send new
        if not self.status_message:
            if not (my_perms.send_messages and my_perms.embed_links):
                 logger.error("Cannot send new status message: Missing Send/Embed permission.")
                 return

            logger.debug("Attempting to send a new status message.")
            try:
                new_message = await self.status_channel.send(embed=embed, view=message_view)
                self.status_message = new_message
                logger.info(f"Sent new status message {new_message.id} to #{self.status_channel.name}.")
                await self._pin_status_message(new_message)
            except discord.Forbidden as e:
                logger.error(f"Permission error sending new status message: {e.text}")
            except discord.HTTPException as e:
                logger.error(f"HTTP error sending new status message: {e}")
            except Exception as e:
                 logger.exception(f"Unexpected error sending new status message: {e}")


    async def _pin_status_message(self, message: discord.Message):
         """Attempts to pin the status message and unpin old ones."""
         if not self.status_channel: return

         my_perms = self.status_channel.permissions_for(self.status_channel.guild.me)
         if not my_perms.manage_messages:
              logger.warning("Missing 'Manage Messages' permission. Cannot manage pins.")
              return

         logger.debug(f"Managing pins for status message {message.id}...")
         try:
            pins = await self.status_channel.pins()
            # unpin old messages
            for pin in pins:
                if pin.author == self.bot.user and pin.id != message.id:
                     if pin.embeds and pin.embeds[0].title and pin.embeds[0].title.startswith("📊 Server Status:"):
                         logger.info(f"Unpinning old status message {pin.id}.")
                         try: await pin.unpin(reason="Replacing with new status message")
                         except Exception as unpin_e: logger.warning(f"Failed unpinning {pin.id}: {unpin_e}")

            # pin new message
            try:
                await message.pin(reason="Server Status Message")
                logger.info(f"Pinned new status message {message.id}.")
            except discord.HTTPException as e:
                 if e.code == 50083:
                      logger.debug(f"Message {message.id} already pinned.")
                 else: raise
            except discord.Forbidden as e:
                 logger.warning(f"Permission error pinning message {message.id}: {e.text}")
            except Exception as e_pin:
                 logger.exception(f"Unexpected error pinning message {message.id}: {e_pin}")

         except discord.Forbidden as e:
             logger.warning(f"Permission error fetching pins: {e.text}")
         except discord.HTTPException as e:
             logger.error(f"HTTP error fetching pins: {e}")
         except Exception as e:
             logger.exception(f"Unexpected error managing pins: {e}")


# setup
async def setup(bot: commands.Bot):
    if isinstance(bot, DiscordBridgeBot):
        if not bot.status_service: logger.error("Status Cog setup aborted: Missing ServerStatusService"); return
        await bot.add_cog(StatusCog(bot, bot.status_service))
    else:
        logger.critical("Status Cog setup failed: Bot instance is not DiscordBridgeBot type.")