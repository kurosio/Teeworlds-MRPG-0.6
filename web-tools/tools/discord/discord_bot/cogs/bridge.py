# discord_bot/cogs/bridge.py
import asyncio
import discord
from discord.ext import commands
import logging
from typing import cast, Optional, Dict

# Project imports
from config import (WEBHOOK_SERVER_SKIN, WEBHOOK_UNKNOWN_SKIN, DISCORD_CHANNEL_ID,
                    RECONNECT_DELAY, POLL_INTERVAL, SERVER_TAG_TEEWORLDS, COMMAND_PREFIX,
                    ECON_READ_TIMEOUT)
from models.econ import EconMessage, EconChatMessage
from services.teeworlds_client import TeeworldsEconClient
from services.server_status import ServerStatusService
from discord_bot.webhook_manager import WebhookManager
from utils.helpers import normalize_nickname, get_skin_url, clean_message, get_flag_emoji
from discord_bot.bot import DiscordBridgeBot
from discord_bot.cogs.status import StatusCog

logger = logging.getLogger("TeeworldsBridge.BridgeCog")

class BridgeCog(commands.Cog):
    """Handles the chat bridge between Discord and Teeworlds Econ."""

    def __init__(self, bot: DiscordBridgeBot,
                 econ_client: TeeworldsEconClient,
                 status_service: ServerStatusService,
                 webhook_manager: WebhookManager):
        self.bot = bot
        self.econ_client = econ_client
        self.status_service = status_service
        self.webhook_manager = webhook_manager
        self.target_channel: Optional[discord.TextChannel] = None
        self._monitor_task: Optional[asyncio.Task] = None
        self._first_connect_attempt = True
        self._ready_event = asyncio.Event()


    @commands.Cog.listener()
    async def on_ready(self):
        """Called once the bot is ready. Finds channel, initializes webhook, starts monitor."""
        logger.info("Bridge Cog: Bot is ready. Initializing...")

        # search channel (cache first, then API fetch fallback)
        channel = self.bot.get_channel(DISCORD_CHANNEL_ID)
        if channel is None:
            try:
                channel = await self.bot.fetch_channel(DISCORD_CHANNEL_ID)
                logger.debug(f"Bridge Cog fetched channel {DISCORD_CHANNEL_ID} via API.")
            except discord.NotFound:
                logger.error(f"Bridge channel {DISCORD_CHANNEL_ID} was not found via API.")
            except discord.Forbidden:
                logger.error(f"Missing permissions to fetch bridge channel {DISCORD_CHANNEL_ID} via API.")
            except discord.HTTPException as e:
                logger.error(f"HTTP error while fetching bridge channel {DISCORD_CHANNEL_ID}: {e}")

        if not isinstance(channel, discord.TextChannel):
            logger.critical(f"Bridge channel {DISCORD_CHANNEL_ID} not found or not text! Bridge disabled.")
            self.target_channel = None
            self._ready_event.set()
            return

        self.target_channel = channel
        logger.info(f"Bridge Cog found target channel: #{self.target_channel.name} ({DISCORD_CHANNEL_ID})")

        # initialize webhook
        try:
             initialized = await self.webhook_manager.initialize(DISCORD_CHANNEL_ID)
             if initialized and self.webhook_manager.is_ready():
                  logger.info("Webhook manager initialized successfully by Bridge Cog.")
             else:
                  logger.error("Webhook manager failed to initialize (check permissions/logs). Bridge might use fallback.")
        except Exception as e:
             logger.exception(f"Error initializing webhook manager: {e}")

        # run monitoring econ
        if self._monitor_task is None or self._monitor_task.done():
            logger.info("Starting Econ monitor task from on_ready...")
            self._monitor_task = asyncio.create_task(self.monitor_teeworlds_econ_loop(), name="EconMonitorLoop")
            self._monitor_task.add_done_callback(self._log_task_exception)
        else:
             logger.warning("Econ monitor task appears to be already running.")

        self._ready_event.set()
        logger.info("Bridge Cog initialization complete.")


    async def cog_unload(self):
        """Called when the cog is unloaded. Stops the monitor task."""
        logger.info("Bridge Cog unloading...")
        if self._monitor_task and not self._monitor_task.done():
            self._monitor_task.cancel()
            logger.info("Teeworlds Econ monitor task cancellation requested.")
        logger.info("Bridge Cog unloaded.")


    def _log_task_exception(self, task: asyncio.Task):
        """Logs exceptions from the monitor task if it crashes."""
        try:
            if not task.cancelled():
                 task.result()
                 logger.info(f"Task '{task.get_name()}' finished normally.")
        except asyncio.CancelledError:
            logger.info(f"Task '{task.get_name()}' was cancelled successfully.")
        except Exception as e:
            logger.exception(f"💥 Unhandled {type(e).__name__} in task '{task.get_name()}': {e}")


    @commands.Cog.listener()
    async def on_message(self, message: discord.Message):
        """Listens for messages in the bridge channel and relays them to Teeworlds."""
        await self._ready_event.wait()

        # ignore messages from bots or not active channel
        if message.author.bot or not self.target_channel or message.channel.id != self.target_channel.id:
            return

        # ignore commands
        if message.content.startswith(COMMAND_PREFIX): return

        # ignore msg status is same channel
        status_cog = self.bot.get_cog("StatusCog")
        if status_cog and isinstance(status_cog, StatusCog):
             if status_cog.status_message and message.id == status_cog.status_message.id:
                 logger.debug("Ignoring own status message in bridge channel.")
                 return
                 
        # ignore embeds from bots
        if message.embeds and message.author == self.bot.user:
             logger.debug("Ignoring own embed message in bridge channel.")
             return

        author_name = message.author.display_name
        content = clean_message(message.clean_content)

        # attachments
        if message.attachments:
            urls = " ".join([att.url for att in message.attachments])
            if len(urls) < 250:
                 content += f" [Attachments: {urls}]"
            else:
                 content += f" [{len(message.attachments)} Attachment(s)]"

        # clean message by limit
        max_discord_msg_len = 300
        if len(content) > max_discord_msg_len:
             content = content[:max_discord_msg_len] + "..."

        # ignore empty messages after cleanup or cutting
        if not content.strip():
             logger.debug(f"Ignoring effectively empty message from {author_name}")
             return

        # format msg for teeworlds
        # sanitize author display name for single-line relay in game chat
        safe_author_name = clean_message(author_name).replace(":", " ").strip() or "DiscordUser"
        tw_message = f"{SERVER_TAG_TEEWORLDS} {safe_author_name}: {content}"

        logger.info(f"📤 Relaying to Teeworlds: {author_name}: {content[:100]}...")
        if self.econ_client:
            success = await self.econ_client.send_message(tw_message)
            if not success:
                logger.warning("Failed to send message to Teeworlds (Econ send_message returned False).")
                try: await message.add_reaction('❌')
                except discord.HTTPException: pass
        else:
            logger.error("Cannot send message to Teeworlds: Econ client is missing!")


    async def monitor_teeworlds_econ_loop(self):
        """The main loop that connects to Econ and processes incoming messages."""
        logger.info(f"👀 Starting Teeworlds Econ monitor loop (Poll: {POLL_INTERVAL}s, Read Timeout: {ECON_READ_TIMEOUT}s)")
        await self.bot.wait_until_ready()
        logger.info("Econ monitor proceeding as bot is ready.")

        import time
        last_receive_time = time.monotonic()

        while True:
            loop_start_time = time.monotonic()
            try:
                if not self.econ_client.is_connected:
                    if not self._first_connect_attempt:
                        logger.info(f"Econ connection lost/unavailable. Waiting {RECONNECT_DELAY}s before reconnecting...")
                        await asyncio.sleep(RECONNECT_DELAY)
                    else: logger.info("First attempt to connect to Econ...")
                    self._first_connect_attempt = False
                    connected = await self.econ_client.connect()
                    if not connected:
                        logger.warning("Econ connection attempt failed. Loop will retry.")
                        await asyncio.sleep(RECONNECT_DELAY)
                        continue
                    else: logger.info("Econ connection ready. Listening for messages.")

                if self.econ_client.is_connected:
                     receive_start_time = time.monotonic()
                     econ_msg = await self.econ_client.receive_econ_message()
                     receive_duration = time.monotonic() - receive_start_time
                     if receive_duration > 1.0 or logger.isEnabledFor(logging.DEBUG):
                          logger.debug(f"Econ receive call took {receive_duration:.3f}s.")

                     if econ_msg:
                         last_receive_time = time.monotonic()
                         handle_start_time = time.monotonic()
                         await self._handle_econ_message(econ_msg)
                         handle_duration = time.monotonic() - handle_start_time
                         if handle_duration > 0.5 or logger.isEnabledFor(logging.DEBUG):
                             logger.debug(f"Handling econ message took {handle_duration:.3f}s.")
                         await asyncio.sleep(0.05)
                     else:
                         if not self.econ_client.is_connected:
                              logger.info("Econ monitor detected disconnect during receive.")
                         else:
                              time_since_last_msg = time.monotonic() - last_receive_time
                              logger.debug(f"No message received (timeout). Time since last msg: {time_since_last_msg:.1f}s")
                              await asyncio.sleep(POLL_INTERVAL)

            except asyncio.CancelledError:
                logger.info("Teeworlds Econ monitor loop cancellation received. Exiting.")
                await self.econ_client.close()
                break
            except Exception as e:
                logger.exception(f"⚠️ Unexpected error in Econ monitor loop (outer try-except): {e}")
                await self.econ_client.close()
                wait_time = RECONNECT_DELAY * 2
                logger.info(f"Waiting {wait_time}s after unexpected error...")
                await asyncio.sleep(wait_time)

            loop_duration = time.monotonic() - loop_start_time
            if loop_duration > 2.0:
                 logger.warning(f"Econ monitor loop iteration took {loop_duration:.3f}s (Potential blockage).")

        logger.info("🛑 Teeworlds Econ monitor loop has stopped.")


    async def _handle_econ_message(self, econ_msg: EconMessage):
        """Processes a parsed message received from Econ."""
        await self._ready_event.wait()

        msg_type = econ_msg["type"]

        if msg_type == "chat":
            chat_msg = cast(EconChatMessage, econ_msg)
            await self._handle_chat_message(chat_msg)
        elif msg_type == "other":
             logger.debug(f"Received 'other' Econ message: {econ_msg['raw'][:150]}...")


    async def _handle_chat_message(self, msg: EconChatMessage):
        """Handles 'chat' type messages, adds flag emoji, gets skin URL, relays."""
        await self._ready_event.wait()

        if not self.target_channel:
             logger.error("Cannot relay chat message: Target channel is None.")
             return

        nickname = msg["nickname"]
        message_text = msg["message"]

        if message_text.startswith(SERVER_TAG_TEEWORLDS):
            logger.debug(f"Ignoring own bridge message from Teeworlds: {nickname}")
            return

        logger.info(f"📨 Relaying to Discord: {nickname}: {message_text[:100]}...")

        webhook_username: str = nickname
        avatar_url_to_use: Optional[str] = None
        final_message_content: str = message_text

        # system messages
        if nickname == '***':
            webhook_username = "Teeworlds Event"
            try:
                 avatar_url_to_use = get_skin_url(WEBHOOK_SERVER_SKIN, 0, 0)
            except Exception as e: logger.warning(f"Failed system skin URL: {e}")

        # player messages
        else:
            normalized_nick = normalize_nickname(nickname)
            country_code: int = -1
            flag_emoji: str = ""

            player_data: Optional[Dict] = None
            try:
                current_player_cache = await self.status_service.get_player_cache()
                player_data = current_player_cache.get(normalized_nick)
            except Exception as e:
                 logger.error(f"Failed getting player cache for {normalized_nick}: {e}")

            if player_data:
                logger.debug(f"Found player data for '{normalized_nick}' in cache.")
                country_code = player_data.get("country", -1)
                flag_emoji = get_flag_emoji(country_code)
                skin_data = player_data.get("skin", {})
                skin_name = str(player_data.get("skin_name", "default") or "default")
                if skin_name == "default" and isinstance(skin_data, dict):
                    skin_name = str(skin_data.get("name", "default") or "default")
                elif skin_name == "default" and isinstance(skin_data, str) and skin_data.strip():
                    skin_name = skin_data.strip()

                body_color = player_data.get("body_color", 0)
                feet_color = player_data.get("feet_color", 0)

                try:
                    avatar_url_to_use = get_skin_url(
                        skin_name, body_color, feet_color
                    )
                    logger.debug(f"Generated skin URL for '{nickname}': {avatar_url_to_use}")
                except Exception as e:
                    logger.warning(f"Failed skin URL generation for '{nickname}': {e}")
            else:
                logger.debug(f"No player data/skin info found for '{normalized_nick}'. Using defaults.")
                flag_emoji = get_flag_emoji(-1)
                try: avatar_url_to_use = get_skin_url("default", 0, 0)
                except Exception: pass

            potential_username = f"{nickname} ({flag_emoji})"
            if len(potential_username) <= 80:
                 webhook_username = potential_username
            else:
                 max_nick_len = 80 - len(flag_emoji) - 1
                 if max_nick_len > 3:
                     webhook_username = f"{nickname[:max_nick_len]} ({flag_emoji})"
                 else:
                      webhook_username = nickname[:80]

        if avatar_url_to_use is None:
            avatar_url_to_use = get_skin_url(WEBHOOK_UNKNOWN_SKIN, 0, 0)

        await self.webhook_manager.send(
            username=webhook_username,
            content=final_message_content,
            avatar_url=avatar_url_to_use
        )


# setup
async def setup(bot: commands.Bot):
    if isinstance(bot, DiscordBridgeBot):
        if not bot.teeworlds_client: logger.error("Bridge Cog setup aborted: Missing TeeworldsEconClient"); return
        if not bot.status_service: logger.error("Bridge Cog setup aborted: Missing ServerStatusService"); return
        if not bot.webhook_manager: logger.error("Bridge Cog setup aborted: Missing WebhookManager"); return
        await bot.add_cog(BridgeCog(bot, bot.teeworlds_client, bot.status_service, bot.webhook_manager))
    else:
        logger.critical("Bridge Cog setup failed: Bot instance is not DiscordBridgeBot type.")
