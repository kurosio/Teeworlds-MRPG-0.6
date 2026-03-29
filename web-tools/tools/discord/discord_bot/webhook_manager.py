# discord_bot/webhook_manager.py
import discord
import logging
import aiohttp
from typing import Optional
import asyncio

from config import WEBHOOK_NAME

logger = logging.getLogger("TeeworldsBridge.WebhookManager")

class WebhookManager:
    """Handles finding, creating, and sending messages via a Discord webhook."""

    def __init__(self, bot: discord.Client, http_session: aiohttp.ClientSession):
        self.bot = bot
        self.http_session = http_session
        self.webhook: Optional[discord.Webhook] = None
        self._target_channel_id: Optional[int] = None
        self._target_channel: Optional[discord.TextChannel] = None
        self._init_lock = asyncio.Lock()

    async def initialize(self, channel_id: int):
        """Sets the target channel and attempts to find or create the webhook."""
        async with self._init_lock:
            if self.webhook and self._target_channel_id == channel_id:
                logger.debug(f"WebhookManager already initialized for channel {channel_id}.")
                return True

            self._target_channel_id = channel_id
            logger.debug(f"WebhookManager: Attempting initialization for channel ID {channel_id}")
            channel = self.bot.get_channel(channel_id)

            if not isinstance(channel, discord.TextChannel):
                self._target_channel = None
                self.webhook = None
                return False

            self._target_channel = channel
            logger.info(f"Webhook Manager targeting channel: #{channel.name} ({channel_id})")
            success = await self._find_or_create_webhook()
            return success


    async def _find_or_create_webhook(self) -> bool:
        """Finds an existing webhook or creates a new one. Returns True on success."""
        if not self._target_channel:
            logger.error("WebhookManager: Cannot find/create webhook: Target channel not set.")
            return False

        logger.debug("WebhookManager: Checking permissions...")
        try:
            my_perms = self._target_channel.permissions_for(self._target_channel.guild.me)
            if not my_perms.manage_webhooks:
                 logger.error(f"WebhookManager: Missing 'Manage Webhooks' permission in #{self._target_channel.name}.")
                 self.webhook = None
                 return False
            logger.debug("WebhookManager: Permissions OK. Fetching webhooks...")

            # search exist webhook by name and owner
            webhooks = await self._target_channel.webhooks()
            logger.debug(f"WebhookManager: Fetched {len(webhooks)} webhooks.")
            for wh in webhooks:
                if wh.name == WEBHOOK_NAME and wh.user == self.bot.user:
                    self.webhook = wh
                    logger.info(f"WebhookManager: Found existing webhook '{wh.name}' ({wh.id}).")
                    return True

            # creating new webhook
            logger.info(f"WebhookManager: Creating new webhook '{WEBHOOK_NAME}'...")
            logger.debug("WebhookManager: Creating webhook via API...")
            self.webhook = await self._target_channel.create_webhook(
                name=WEBHOOK_NAME,
                reason="Teeworlds Bridge Webhook"
            )
            logger.info(f"WebhookManager: Created webhook '{self.webhook.name}' ({self.webhook.id}).")
            return True

        except discord.Forbidden as e:
            logger.error(f"WebhookManager: Permission error (Forbidden) during webhook find/create: {e.text}")
        except discord.HTTPException as e:
            logger.error(f"WebhookManager: HTTP error during webhook find/create (Status: {e.status}): {e.text}")
        except Exception as e:
            logger.exception(f"WebhookManager: Unexpected error during webhook find/create: {e}")

        self.webhook = None
        return False


    async def send(self, username: str, content: str, avatar_url: Optional[str] = None):
        """Sends a message using the managed webhook."""
        async with self._init_lock:
             webhook_to_use = self.webhook

        if not webhook_to_use:
            logger.warning("WebhookManager: Webhook is not available. Attempting fallback.")
            await self._send_fallback(username, content)
            return

        webhook_username = username[:80]
        content = content[:2000]

        try:
            logger.debug(f"WebhookManager: Sending via webhook '{webhook_to_use.name}': User='{webhook_username}', Avatar='{avatar_url}', Content='{content[:50]}...'")
            await webhook_to_use.send(
                content=content,
                username=webhook_username,
                avatar_url=avatar_url if avatar_url else discord.utils.MISSING,
                allowed_mentions=discord.AllowedMentions.none()
            )
        except discord.NotFound:
             logger.error(f"WebhookManager: Webhook {webhook_to_use.id} not found during send (deleted?). Resetting.")
             async with self._init_lock:
                 self.webhook = None
             await self._send_fallback(username, content)
        except discord.Forbidden as e:
             logger.error(f"WebhookManager: Permission error sending webhook message: {e.text}")
             await self._send_fallback(username, content)
        except discord.HTTPException as e:
            logger.error(f"WebhookManager: Webhook send failed (HTTP {e.status}): {e.text}")
            if e.status == 404:
                 logger.error("WebhookManager: Got 404 on send, resetting.")
                 async with self._init_lock: self.webhook = None
                 await self._send_fallback(username, content)
        except Exception as e:
            logger.exception(f"WebhookManager: Unexpected error sending webhook message: {e}")
            await self._send_fallback(username, content)


    async def _send_fallback(self, username: str, content: str):
        """Sends a fallback message using the bot account if webhook fails."""
        if not self._target_channel:
            logger.error("WebhookManager Fallback: Cannot send, target channel unknown.")
            return

        logger.warning(f"WebhookManager: Sending fallback message for '{username}'...")
        try:
            if not self._target_channel.permissions_for(self._target_channel.guild.me).send_messages:
                 logger.error("WebhookManager Fallback: Missing 'Send Messages' permission.")
                 return

            fallback_msg = f"**[TW Fallback]** `{discord.utils.escape_markdown(username)}`: {discord.utils.escape_markdown(content)}"
            await self._target_channel.send(fallback_msg[:2000])
        except Exception as fb_e:
            logger.error(f"WebhookManager Fallback: Send failed: {fb_e}")


    def is_ready(self) -> bool:
        """Checks if the webhook is configured and likely ready."""
        return self.webhook is not None and self._target_channel is not None
