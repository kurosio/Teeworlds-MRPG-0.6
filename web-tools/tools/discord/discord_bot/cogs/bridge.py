# discord_bot/cogs/bridge.py
"""
Chat bridge between one Discord channel and any number of game servers.

Game -> Discord: the chat of every server is posted to the channel through a webhook, the
nickname is prefixed with the tag of the server it was written on ("[EU] nickname").
Discord -> game: a message written in the channel is said on all servers at once.
"""
import asyncio
import discord
from discord.ext import commands
import logging
import json
import os
import re
from typing import Any, Dict, Optional, Tuple

# Project imports
from config import BASE_DIR, ServerConfig
from models.econ import EconBridgeToggle, EconChatMessage, EconEvent
from utils.helpers import (clean_message, format_discord_username, format_duration, format_game_lines,
                           get_flag_emoji, get_skin_url)
from discord_bot.bot import DiscordBridgeBot

logger = logging.getLogger("TeeworldsBridge.BridgeCog")

_CUSTOM_EMOJI_RE = re.compile(r"<a?:(\w+):\d+>")
OutboxItem = Tuple[ServerConfig, EconChatMessage]


class BridgeCog(commands.Cog):
    """Handles the chat bridge between Discord and the Econ of every game server."""

    def __init__(self, bot: DiscordBridgeBot):
        self.bot = bot
        self.config = bot.config
        self.settings = bot.config.bridge
        self.hub = bot.server_hub
        self.status_service = bot.status_service
        self.webhook_manager = bot.webhook_manager
        self.target_channel: Optional[discord.TextChannel] = None

        # chat waiting to be posted to Discord, in the order it was received from the servers
        self._outbox: "asyncio.Queue[OutboxItem]" = asyncio.Queue(maxsize=self.settings.queue_size)
        self._sender_task: Optional[asyncio.Task] = None
        self._dropped_messages = 0
        self._ready_event = asyncio.Event()

        self._muted_file = os.path.join(BASE_DIR, "muted_players.json")
        self.muted_players: set = self._load_json_set(self._muted_file)
        self._bridge_disabled_file = os.path.join(BASE_DIR, "bridge_disabled.json")
        self.bridge_disabled_players: set = self._load_json_set(self._bridge_disabled_file)

    # --- persistent name lists ---------------------------------------------------------

    def _load_json_set(self, filepath: str) -> set:
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                return set(json.load(f))
        except Exception:
            return set()

    def _save_json_set(self, filepath: str, data: set):
        try:
            with open(filepath, "w", encoding="utf-8") as f:
                json.dump(sorted(data), f, indent=2, ensure_ascii=False)
        except Exception as e:
            logger.error(f"Failed to save {os.path.basename(filepath)}: {e}")

    # --- slash commands ----------------------------------------------------------------

    def _has_admin_role(self, interaction: discord.Interaction) -> bool:
        if not interaction.guild or not isinstance(interaction.user, discord.Member):
            return False
        admin_role_id = self.config.discord.admin_role_id
        if admin_role_id is None:
            return interaction.user.guild_permissions.administrator
        return any(r.id == admin_role_id for r in interaction.user.roles)

    @discord.app_commands.command(name="bridge_mute", description="[Admin] Mute a player — their in-game messages won't appear on Discord.")
    @discord.app_commands.describe(name="Exact in-game nickname to mute")
    async def bridge_mute(self, interaction: discord.Interaction, name: str):
        if not self._has_admin_role(interaction):
            await interaction.response.send_message("❌ You don't have permission to use this command.", ephemeral=True)
            return
        if name in self.muted_players:
            await interaction.response.send_message(f"⚠️ `{name}` is already muted.", ephemeral=True)
            return
        self.muted_players.add(name)
        self._save_json_set(self._muted_file, self.muted_players)
        logger.info(f"Admin {interaction.user} muted player '{name}' from bridge.")
        await interaction.response.send_message(f"🔇 `{name}` has been muted from the bridge.", ephemeral=True)

    @discord.app_commands.command(name="bridge_unmute", description="[Admin] Unmute a player — their in-game messages will appear on Discord again.")
    @discord.app_commands.describe(name="Exact in-game nickname to unmute")
    async def bridge_unmute(self, interaction: discord.Interaction, name: str):
        if not self._has_admin_role(interaction):
            await interaction.response.send_message("❌ You don't have permission to use this command.", ephemeral=True)
            return
        if name not in self.muted_players:
            await interaction.response.send_message(f"⚠️ `{name}` is not in the mute list.", ephemeral=True)
            return
        self.muted_players.discard(name)
        self._save_json_set(self._muted_file, self.muted_players)
        logger.info(f"Admin {interaction.user} unmuted player '{name}' from bridge.")
        await interaction.response.send_message(f"🔊 `{name}` has been unmuted from the bridge.", ephemeral=True)

    @discord.app_commands.command(name="bridge_mutelist", description="[Admin] List all players currently muted from the bridge.")
    async def bridge_mutelist(self, interaction: discord.Interaction):
        if not self._has_admin_role(interaction):
            await interaction.response.send_message("❌ You don't have permission to use this command.", ephemeral=True)
            return
        if not self.muted_players:
            await interaction.response.send_message("✅ No players are currently muted.", ephemeral=True)
            return
        sorted_list = sorted(self.muted_players)
        chunks = []
        current = ""
        for p in sorted_list:
            entry = f"`{p}`  "
            if len(current) + len(entry) > 1800:
                chunks.append(current)
                current = entry
            else:
                current += entry
        if current:
            chunks.append(current)
        await interaction.response.send_message(f"🔇 **Muted players ({len(self.muted_players)}):**\n{chunks[0]}", ephemeral=True)
        for chunk in chunks[1:]:
            await interaction.followup.send(chunk, ephemeral=True)

    @discord.app_commands.command(name="bridge_servers", description="[Admin] Show the game servers connected to the bridge.")
    async def bridge_servers(self, interaction: discord.Interaction):
        if not self._has_admin_role(interaction):
            await interaction.response.send_message("❌ You don't have permission to use this command.", ephemeral=True)
            return
        await interaction.response.send_message(self.describe_servers()[:2000], ephemeral=True)

    def describe_servers(self) -> str:
        """Human readable list of the servers and the state of their Econ links."""
        states = self.hub.states()
        lines = [f"🌉 **Bridged servers ({sum(1 for s in states if s.connected)}/{len(states)} connected):**"]
        for state in states:
            server = state.server
            title = f"**{discord.utils.escape_markdown(server.label)}** {discord.utils.escape_markdown(server.name)}"
            if state.connected:
                lines.append(f"🟢 {title} — connected for {format_duration(state.uptime or 0)}")
            else:
                reason = f" ({state.last_error})" if state.last_error else ""
                lines.append(f"🔴 {title} — offline{reason}")
        return "\n".join(lines)

    # --- lifecycle ---------------------------------------------------------------------

    @commands.Cog.listener()
    async def on_ready(self):
        """Called once the bot is ready. Finds channel, initializes webhook, starts the server links."""
        logger.info("Bridge Cog: Bot is ready. Initializing...")
        channel_id = self.config.discord.channel_id

        # search channel (cache first, then API fetch fallback)
        channel = self.bot.get_channel(channel_id)
        if channel is None:
            try:
                channel = await self.bot.fetch_channel(channel_id)
                logger.debug(f"Bridge Cog fetched channel {channel_id} via API.")
            except discord.NotFound:
                logger.error(f"Bridge channel {channel_id} was not found via API.")
            except discord.Forbidden:
                logger.error(f"Missing permissions to fetch bridge channel {channel_id} via API.")
            except discord.HTTPException as e:
                logger.error(f"HTTP error while fetching bridge channel {channel_id}: {e}")

        if not isinstance(channel, discord.TextChannel):
            logger.critical(f"Bridge channel {channel_id} not found or not text! Bridge disabled.")
            self.target_channel = None
            self._ready_event.set()
            return

        self.target_channel = channel
        logger.info(f"Bridge Cog found target channel: #{self.target_channel.name} ({channel_id})")

        # initialize webhook
        try:
            initialized = await self.webhook_manager.initialize(channel)
            if initialized and self.webhook_manager.is_ready():
                logger.info("Webhook manager initialized successfully by Bridge Cog.")
            else:
                logger.error("Webhook manager failed to initialize (check permissions/logs). Bridge might use fallback.")
        except Exception as e:
            logger.exception(f"Error initializing webhook manager: {e}")

        # start posting to Discord and connecting to the servers (on_ready may fire again on reconnects)
        if self._sender_task is None or self._sender_task.done():
            self._sender_task = asyncio.create_task(self._discord_sender_loop(), name="DiscordSenderLoop")
            self._sender_task.add_done_callback(self._log_task_exception)
        self.hub.start(self._on_server_event)

        self._ready_event.set()
        logger.info("Bridge Cog initialization complete.")

    async def cog_unload(self):
        """Called when the cog is unloaded. Stops the server links and the sender task."""
        logger.info("Bridge Cog unloading...")
        await self.hub.stop()
        if self._sender_task and not self._sender_task.done():
            self._sender_task.cancel()
        logger.info("Bridge Cog unloaded.")

    def _log_task_exception(self, task: asyncio.Task):
        """Logs exceptions from a background task if it crashes."""
        if task.cancelled():
            return
        error = task.exception()
        if error is not None:
            logger.error(f"💥 Unhandled {type(error).__name__} in task '{task.get_name()}': {error}", exc_info=error)

    # --- Discord -> game servers -------------------------------------------------------

    @commands.Cog.listener()
    async def on_message(self, message: discord.Message):
        """Listens for messages in the bridge channel and relays them to every game server."""
        await self._ready_event.wait()

        # ignore messages from bots (including the bridge webhook) or outside of the bridge channel
        if message.author.bot or not self.target_channel or message.channel.id != self.target_channel.id:
            return

        # ignore commands
        if message.content.startswith(self.config.discord.command_prefix):
            return

        author_name = message.author.display_name
        content = clean_message(_CUSTOM_EMOJI_RE.sub(r":\1:", message.clean_content))

        # attachments
        if message.attachments:
            urls = " ".join([att.url for att in message.attachments])
            if len(urls) < 250:
                content += f" [Attachments: {urls}]"
            else:
                content += f" [{len(message.attachments)} Attachment(s)]"

        # clean message by limit
        max_discord_msg_len = self.settings.max_discord_message_length
        if len(content) > max_discord_msg_len:
            content = content[:max_discord_msg_len] + "..."

        # format for the game chat; every line carries the bridge tag
        lines = format_game_lines(self.settings.discord_tag, author_name, content, self.settings.max_message_length)
        if not lines:
            logger.debug(f"Ignoring effectively empty message from {author_name}")
            return

        logger.info(f"📤 Relaying to Teeworlds: {author_name}: {content[:100]}...")
        results = await self.hub.broadcast(lines)
        delivered = [tag for tag, ok in results.items() if ok]
        missed = [tag for tag, ok in results.items() if not ok]

        if not delivered:
            logger.warning("Failed to send message to Teeworlds: no game server is connected.")
            try:
                await message.add_reaction('❌')
            except discord.HTTPException:
                pass
        elif missed:
            logger.warning(f"Message delivered to {len(delivered)}/{len(results)} servers, offline: {', '.join(missed)}")

    # --- game servers -> Discord -------------------------------------------------------

    async def _on_server_event(self, server: ServerConfig, event: EconEvent):
        """Receives the events of every server (called from the Econ reader tasks, must not block)."""
        if isinstance(event, EconBridgeToggle):
            self._handle_bridge_toggle(server, event)
        elif isinstance(event, EconChatMessage):
            if self._is_suppressed(event):
                return
            self._enqueue((server, event))

    def _handle_bridge_toggle(self, server: ServerConfig, event: EconBridgeToggle):
        if event.enabled:
            self.bridge_disabled_players.discard(event.nickname)
        else:
            self.bridge_disabled_players.add(event.nickname)
        self._save_json_set(self._bridge_disabled_file, self.bridge_disabled_players)
        logger.info(f"Bridge self-{'enabled' if event.enabled else 'disabled'} by player "
                    f"'{event.nickname}' on {server.label}")

    def _is_suppressed(self, message: EconChatMessage) -> bool:
        if message.is_system:
            return False
        if message.nickname in self.muted_players:
            logger.debug(f"Bridge suppressed message from muted player '{message.nickname}'")
            return True
        if message.nickname in self.bridge_disabled_players:
            logger.debug(f"Bridge suppressed message from self-disabled player '{message.nickname}'")
            return True
        return False

    def _enqueue(self, item: OutboxItem):
        """Queues a chat message for Discord; when Discord cannot keep up the oldest ones are dropped."""
        if self._outbox.full():
            try:
                self._outbox.get_nowait()
            except asyncio.QueueEmpty:
                pass
            self._dropped_messages += 1
            if self._dropped_messages == 1 or self._dropped_messages % 50 == 0:
                logger.warning(f"Discord is not keeping up with the game chat: "
                               f"{self._dropped_messages} message(s) dropped so far.")
        self._outbox.put_nowait(item)

    async def _discord_sender_loop(self):
        """Posts the queued chat messages to Discord one by one."""
        logger.info("👀 Discord sender loop started.")
        while True:
            server, message = await self._outbox.get()
            try:
                await self._relay_to_discord(server, message)
            except asyncio.CancelledError:
                raise
            except Exception as e:
                logger.exception(f"⚠️ Failed to relay a message of {server.label} to Discord: {e}")

    async def _lookup_player(self, server: ServerConfig, nickname: str) -> Optional[Dict[str, Any]]:
        """Returns the master server data of a player (flag, skin), if the server is listed there."""
        if not server.game_address:
            return None
        try:
            return await asyncio.wait_for(
                self.status_service.get_player(server.tag, nickname),
                timeout=self.settings.player_cache_timeout
            )
        except asyncio.TimeoutError:
            logger.warning(f"Player cache lookup timed out for '{nickname}'.")
        except Exception as e:
            logger.error(f"Failed getting player cache for '{nickname}': {e}")
        return None

    def _skin_url(self, skin_name: str, body_color: int = 0, feet_color: int = 0) -> Optional[str]:
        try:
            return get_skin_url(self.settings.skin_api_url, skin_name, body_color, feet_color)
        except Exception as e:
            logger.warning(f"Failed skin URL generation for skin '{skin_name}': {e}")
            return None

    async def build_discord_identity(self, server: ServerConfig, message: EconChatMessage) -> Tuple[str, Optional[str]]:
        """Returns the webhook name ("[TAG] nickname (flag)") and avatar URL for a chat message."""
        # system messages
        if message.is_system:
            return format_discord_username(server.label, "Server"), self._skin_url(self.settings.server_skin)

        # player messages
        player_data = await self._lookup_player(server, message.nickname)
        if player_data:
            flag_emoji = get_flag_emoji(player_data.get("country", -1))
            avatar_url = self._skin_url(
                str(player_data.get("skin_name", "default") or "default"),
                player_data.get("body_color", 0),
                player_data.get("feet_color", 0),
            )
        else:
            logger.debug(f"No player data/skin info found for '{message.nickname}' on {server.label}. Using defaults.")
            flag_emoji = get_flag_emoji(-1)
            avatar_url = self._skin_url("default")

        return format_discord_username(server.label, message.nickname, flag_emoji), avatar_url

    async def _relay_to_discord(self, server: ServerConfig, message: EconChatMessage):
        """Posts one chat message of a game server to the bridge channel."""
        if not self.target_channel:
            logger.error("Cannot relay chat message: Target channel is None.")
            return

        logger.info(f"📨 Relaying to Discord: {server.label} {message.nickname}: {message.message[:100]}...")
        username, avatar_url = await self.build_discord_identity(server, message)
        if avatar_url is None:
            avatar_url = self._skin_url(self.settings.unknown_skin)

        try:
            await asyncio.wait_for(
                self.webhook_manager.send(username=username, content=message.message, avatar_url=avatar_url),
                timeout=self.settings.webhook_send_timeout
            )
        except asyncio.TimeoutError:
            logger.warning(
                f"Webhook send timed out after {self.settings.webhook_send_timeout}s for user '{username}'."
            )


# setup
async def setup(bot: commands.Bot):
    if isinstance(bot, DiscordBridgeBot):
        if not bot.status_service: logger.error("Bridge Cog setup aborted: Missing ServerStatusService"); return
        if not bot.webhook_manager: logger.error("Bridge Cog setup aborted: Missing WebhookManager"); return
        await bot.add_cog(BridgeCog(bot))
    else:
        logger.critical("Bridge Cog setup failed: Bot instance is not DiscordBridgeBot type.")
