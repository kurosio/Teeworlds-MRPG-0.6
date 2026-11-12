# discord_bot/cogs/status.py
"""
Server status message: one message in the status channel with an embed per game server.
The same cycle refreshes the per-server player caches the bridge uses for flags and skins.
"""
import discord
from discord.ext import commands, tasks
import logging
from typing import List, Optional
import asyncio
import urllib.parse

# Project imports
from services.server_status import PlayerCache, ServerStatus
from discord_bot.bot import DiscordBridgeBot

logger = logging.getLogger("TeeworldsBridge.StatusCog")

STATUS_TITLE_PREFIX = "📊 Server Status:"
MAX_STATUS_EMBEDS = 10          # Discord: embeds per message
MAX_MESSAGE_EMBED_CHARS = 5900  # Discord: 6000 characters over all embeds of a message
MAX_FIELD_CHARS = 1020          # Discord: 1024 characters per field value
MAX_DESCRIPTION_CHARS = 4000    # Discord: 4096 characters per description
MAX_JOIN_BUTTONS = 25           # Discord: components per message


class StatusCog(commands.Cog):
    """Handles updating the server status message in a dedicated channel."""

    def __init__(self, bot: DiscordBridgeBot):
        self.bot = bot
        self.config = bot.config
        self.settings = bot.config.status
        self.status_service = bot.status_service
        self.status_channel: Optional[discord.TextChannel] = None
        self.status_message: Optional[discord.Message] = None
        self._ready_event = asyncio.Event()
        logger.setLevel(bot.config.logging.status_level)
        self.update_server_status_loop.change_interval(minutes=self.settings.update_interval_minutes)


    @commands.Cog.listener()
    async def on_ready(self):
        """Called once the bot is ready. Finds channel, message, starts loop."""
        logger.info("Status Cog: Bot is ready. Initializing...")
        channel_id = self.settings.channel_id

        if not self.status_service.servers:
            logger.info("Status updates disabled: no server has game_address set.")
            self._ready_event.set()
            return

        if channel_id is None:
            logger.info("Status message disabled: [status] channel_id is not set (player data is still refreshed).")
        else:
            await self._find_status_channel(channel_id)
            if self.status_channel and self.status_message is None:
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
            logger.debug("Status update loop already running.")

        self._ready_event.set()
        logger.info("Status Cog initialization complete.")

    async def _find_status_channel(self, channel_id: int):
        """Resolves the status channel (cache first, then API fetch fallback)."""
        channel = self.bot.get_channel(channel_id)
        if channel is None:
            try:
                channel = await self.bot.fetch_channel(channel_id)
                logger.debug(f"Status Cog fetched channel {channel_id} via API.")
            except discord.NotFound:
                logger.error(f"Status channel {channel_id} was not found via API.")
            except discord.Forbidden:
                logger.error(f"Missing permissions to fetch status channel {channel_id} via API.")
            except discord.HTTPException as e:
                logger.error(f"HTTP error while fetching status channel {channel_id}: {e}")

        if not isinstance(channel, discord.TextChannel):
            logger.critical(f"Status channel {channel_id} not found or not text! Status message disabled.")
            self.status_channel = None
            return

        self.status_channel = channel
        logger.info(f"Status Cog found target channel: #{self.status_channel.name} ({channel_id})")

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
        status_message_id = self.settings.message_id
        if status_message_id:
            logger.debug(f"Attempting to fetch status message by ID: {status_message_id}")
            try:
                 if self.status_channel.permissions_for(self.status_channel.guild.me).read_message_history:
                    self.status_message = await self.status_channel.fetch_message(status_message_id)
                    logger.info(f"Fetched status message {self.status_message.id} by ID.")
                    return
                 else:
                     logger.warning("Missing 'Read Message History' permission for ID fetch.")
            except discord.NotFound:
                logger.warning(f"Configured status message ID {status_message_id} not found.")
            except discord.Forbidden:
                logger.error("Permission error (Forbidden) fetching message ID.")
            except discord.HTTPException as e:
                logger.error(f"HTTP error fetching message ID {status_message_id}: {e}")

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
                    if embed.title and embed.title.startswith(STATUS_TITLE_PREFIX):
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


    @tasks.loop(minutes=2)
    async def update_server_status_loop(self):
        """Periodically fetches the status of every server and updates the Discord message."""
        logger.debug("--- Running periodic server status update ---")
        statuses: List[ServerStatus] = []
        error_occurred = False

        try:
            statuses = await self.status_service.update_status()
        except Exception as e:
            logger.exception(f"💥 Unexpected error during status update fetch/process: {e}")
            error_occurred = True

        if not self.status_channel:
            return

        # creating and send message
        try:
            if error_occurred:
                error_embed = discord.Embed(
                    title=f"{STATUS_TITLE_PREFIX} Update Error",
                    description="An internal error occurred while fetching server status. Please check the logs.",
                    color=discord.Color.dark_red()
                )
                error_embed.timestamp = discord.utils.utcnow()
                embeds, view = [error_embed], None
            else:
                embeds = self.build_status_embeds(statuses)
                view = self.build_join_view(statuses)

            await self._update_or_send_status_message(embeds, view)

        except Exception as e:
            logger.exception(f"💥 Unexpected error during status message update/send: {e}")

        logger.debug("--- Status update cycle finished ---")


    @update_server_status_loop.before_loop
    async def before_status_update_loop(self):
        """Waits until the cog's on_ready has finished."""
        await self._ready_event.wait()
        logger.info("Status update loop starting.")


    # --- embeds ------------------------------------------------------------------------

    def _connect_url(self, status: ServerStatus) -> str:
        return f"https://ddnet.org/connect-to/?addr={status.server.game_address}"

    def _bridge_state(self, status: ServerStatus) -> str:
        return "🟢 connected" if self.bot.server_hub.is_connected(status.server.tag) else "🔴 offline"

    @staticmethod
    def _map_name(info: dict) -> str:
        map_data = info.get("map", "Unknown Map")
        return str(map_data.get("name", "Unknown Map")) if isinstance(map_data, dict) else str(map_data)

    def _build_server_embed(self, status: ServerStatus) -> discord.Embed:
        """Creates the embed of one server (without the player list)."""
        server = status.server
        address = server.game_address
        bridge_line = f"**Bridge:** {self._bridge_state(status)}"

        if status.info is None:
            if not status.master_available:
                problem, color = "The master server list is unavailable right now.", discord.Color.orange()
            elif not status.found:
                problem, color = f"The server `{address}` was not found on the master server list.", discord.Color.red()
            else:
                problem, color = f"Found server `{address}` but could not retrieve details.", discord.Color.orange()
            return discord.Embed(
                title=f"{STATUS_TITLE_PREFIX} {server.label} {server.name}"[:256],
                description=f"{problem}\n{bridge_line}",
                color=color
            )

        info = status.info
        player_count = len(status.players)
        max_players = info.get("max_clients", info.get("max_players", "?"))
        server_name = str(info.get("name") or server.name)
        game_type = info.get("gametype", info.get("game_type", "Unknown"))

        description = "\n".join([
            f"**Map:** `{self._map_name(info)}` (`{game_type}`)",
            f"**Online:** {player_count} / {max_players}",
            f"**Connect:** [`{address}`]({self._connect_url(status)})",
            bridge_line,
        ])
        return discord.Embed(
            title=f"{STATUS_TITLE_PREFIX} {server.label} {server_name}"[:256],
            description=description,
            color=discord.Color.green() if player_count > 0 else discord.Color.blue()
        )

    def _format_player_list(self, players: PlayerCache, max_length: int) -> str:
        """Comma separated player names (linked when player_url is set) within `max_length` characters."""
        if not players:
            return "*Server is currently empty*"

        player_url = self.settings.player_url
        entries: List[str] = []
        current_length = 0
        for index, norm_nick in enumerate(sorted(players.keys(), key=str.lower)):
            original_name = players[norm_nick].get("name", norm_nick)
            entry = discord.utils.escape_markdown(original_name)
            if player_url:
                url = player_url.replace("{name}", urllib.parse.quote(original_name, safe=""))
                entry = "[{}]({})".format(entry.replace("[", "\\[").replace("]", "\\]"), url)

            remaining = len(players) - index
            suffix = f"... and {remaining} more"
            entry_length = len(entry) + (2 if entries else 0)
            # keep room for the "and N more" note unless this is the last player
            reserve = len(suffix) + 2 if remaining > 1 else 0
            if current_length + entry_length + reserve > max_length:
                entries.append(suffix)
                break
            entries.append(entry)
            current_length += entry_length

        return ", ".join(entries)[:MAX_FIELD_CHARS]

    def _build_compact_embed(self, statuses: List[ServerStatus]) -> discord.Embed:
        """One-line-per-server overview, used when there are more servers than a message has embeds."""
        lines: List[str] = []
        total_players = 0
        for status in statuses:
            server = status.server
            label = discord.utils.escape_markdown(server.label)
            if status.info is None:
                state = "master unavailable" if not status.master_available else "not listed"
                lines.append(f"🔴 **{label}** {discord.utils.escape_markdown(server.name)} — {state}")
                continue
            info = status.info
            total_players += len(status.players)
            max_players = info.get("max_clients", info.get("max_players", "?"))
            lines.append(
                f"{'🟢' if status.players else '🔵'} **{label}** "
                f"[{discord.utils.escape_markdown(str(info.get('name') or server.name))}]({self._connect_url(status)})"
                f" — {len(status.players)}/{max_players} · `{self._map_name(info)}`"
            )

        description = ""
        for index, line in enumerate(lines):
            if len(description) + len(line) + 1 > MAX_DESCRIPTION_CHARS:
                description += f"... and {len(lines) - index} more"
                break
            description += line + "\n"

        return discord.Embed(
            title=f"{STATUS_TITLE_PREFIX} {len(statuses)} servers, {total_players} players online",
            description=description,
            color=discord.Color.green() if total_players > 0 else discord.Color.blue()
        )

    def build_status_embeds(self, statuses: List[ServerStatus]) -> List[discord.Embed]:
        """Creates the embeds of the status message: one per server, the player lists share Discord's size limit."""
        footer_text = f"Last updated | Updates every {self.settings.update_interval_minutes:g} min"

        if len(statuses) > MAX_STATUS_EMBEDS:
            embeds = [self._build_compact_embed(statuses)]
        else:
            embeds = [self._build_server_embed(status) for status in statuses]
            listed = [(embed, status) for embed, status in zip(embeds, statuses) if status.info is not None]
            if listed:
                field_names = {id(embed): f"Players Online ({len(status.players)})" for embed, status in listed}
                used = sum(len(embed) for embed in embeds) + sum(len(name) for name in field_names.values())
                budget = (MAX_MESSAGE_EMBED_CHARS - used - len(footer_text)) // len(listed)
                budget = max(min(budget, MAX_FIELD_CHARS), 64)
                for embed, status in listed:
                    embed.add_field(name=field_names[id(embed)],
                                    value=self._format_player_list(status.players, budget), inline=False)

        if embeds:
            embeds[-1].timestamp = discord.utils.utcnow()
            embeds[-1].set_footer(text=footer_text)
        return embeds

    def build_join_view(self, statuses: List[ServerStatus]) -> Optional[discord.ui.View]:
        """Creates the 'Join' link buttons, one per listed server."""
        if not self.settings.show_join_button:
            return None
        listed = [status for status in statuses if status.found][:MAX_JOIN_BUTTONS]
        if not listed:
            return None

        view = discord.ui.View(timeout=None)
        for status in listed:
            label = "Join Server" if len(statuses) == 1 else f"Join {status.server.tag}"
            view.add_item(discord.ui.Button(
                label=label[:80], style=discord.ButtonStyle.url, url=self._connect_url(status), emoji="🎮"
            ))
        return view


    async def _update_or_send_status_message(self, embeds: List[discord.Embed], message_view: Optional[discord.ui.View]):
        """Edits the existing status message or sends a new one."""
        if not self.status_channel:
            logger.error("Cannot update status message: Status channel unavailable.")
            return

        my_perms = self.status_channel.permissions_for(self.status_channel.guild.me)

        # try edit
        if self.status_message:
            logger.debug(f"Attempting to edit status message {self.status_message.id}")
            if not (my_perms.send_messages and my_perms.embed_links):
                logger.error(f"Cannot edit status message {self.status_message.id}: Missing Send/Embed permission.")
                return
            try:
                await self.status_message.edit(embeds=embeds, view=message_view)
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
                new_message = await self.status_channel.send(embeds=embeds, view=message_view)
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
                     if pin.embeds and pin.embeds[0].title and pin.embeds[0].title.startswith(STATUS_TITLE_PREFIX):
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
        await bot.add_cog(StatusCog(bot))
    else:
        logger.critical("Status Cog setup failed: Bot instance is not DiscordBridgeBot type.")
