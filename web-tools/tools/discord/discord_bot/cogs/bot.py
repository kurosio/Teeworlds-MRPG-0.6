# discord_bot/bot.py
import asyncio
import discord
from discord.ext import commands
import logging
import aiohttp
from typing import Optional

# Project imports
from config import COMMAND_PREFIX, DISCORD_CHANNEL_ID, STATUS_CHANNEL_ID
from services.teeworlds_client import TeeworldsEconClient
from services.server_status import ServerStatusService
from discord_bot.webhook_manager import WebhookManager

logger = logging.getLogger("TeeworldsBridge.Bot")

class DiscordBridgeBot(commands.Bot):
    """
    Main Discord Bot class orchestrating Teeworlds bridge and status updates.
    Initializes services and loads cogs.
    """

    def __init__(self):
        intents = discord.Intents.default()
        intents.message_content = True
        intents.guilds = True
        intents.messages = True
        allowed_mentions = discord.AllowedMentions.none()

        super().__init__(
            command_prefix=COMMAND_PREFIX,
            intents=intents,
            allowed_mentions=allowed_mentions,
            help_command=None
        )

        self.http_session: Optional[aiohttp.ClientSession] = None
        self.teeworlds_client = TeeworldsEconClient()
        self.status_service: Optional[ServerStatusService] = None
        self.webhook_manager: Optional[WebhookManager] = None

        self.initial_extensions = [
             'discord_bot.cogs.bridge',
             'discord_bot.cogs.status',
        ]

    async def setup_hook(self):
        """Asynchronous setup run before login but after __init__."""
        logger.info("Running setup_hook...")

        # create shared aiohttp session
        self.http_session = aiohttp.ClientSession(
             timeout=aiohttp.ClientTimeout(total=20)
        )
        logger.info("aiohttp ClientSession created.")

        # initialize services that depend on the session or bot instance
        if not self.http_session:
             raise RuntimeError("Failed to create aiohttp ClientSession!")

        self.status_service = ServerStatusService(session=self.http_session)
        self.webhook_manager = WebhookManager(bot=self, http_session=self.http_session)
        logger.info("Dependent services (StatusService, WebhookManager) initialized.")

        # load extensions (Cogs)
        logger.info(f"Loading {len(self.initial_extensions)} extensions...")
        for extension in self.initial_extensions:
            try:
                await self.load_extension(extension)
                logger.info(f"Successfully loaded extension: {extension}")
            except commands.ExtensionNotFound:
                logger.error(f"Extension not found: {extension}")
            except commands.ExtensionAlreadyLoaded:
                logger.warning(f"Extension already loaded: {extension}")
            except commands.NoEntryPointError:
                logger.error(f"Extension has no setup function: {extension}")
            except commands.ExtensionFailed as e:
                logger.error(f"Extension failed to load: {extension}", exc_info=e.original)
            except Exception as e:
                 logger.exception(f"Unexpected error loading extension {extension}: {e}")

        logger.info("setup_hook finished.")

    async def on_ready(self):
        """Called when the bot is fully connected and ready."""
        logger.info("--- Bot Ready ---")
        logger.info(f"Logged in as: {self.user.name} ({self.user.id})")
        logger.info(f"discord.py Version: {discord.__version__}")
        guild_str = ", ".join([f"'{g.name}' ({g.id})" for g in self.guilds])
        logger.info(f"Connected to {len(self.guilds)} guild(s): {guild_str}")

        # log configured channel IDs for verification
        logger.info(f"Bridge Channel ID configured: {DISCORD_CHANNEL_ID}")
        logger.info(f"Status Channel ID configured: {STATUS_CHANNEL_ID}")

        # set bot presence
        try:
             activity = discord.Activity(type=discord.ActivityType.watching, name="Teeworlds")
             await self.change_presence(activity=activity, status=discord.Status.online)
             logger.info("Bot presence set.")
        except Exception as e:
             logger.warning(f"Could not set presence: {e}")

    async def on_command_error(self, ctx: commands.Context, error: commands.CommandError):
        """Handles errors for commands invoked via the command prefix."""
        if isinstance(error, commands.CommandNotFound):
            return

        # handle specific common errors
        if isinstance(error, commands.MissingRequiredArgument):
            await ctx.send(f"Missing argument: `{error.param.name}`.", delete_after=10)
        elif isinstance(error, commands.CommandOnCooldown):
             await ctx.send(f"Slow down! Try again in {error.retry_after:.1f}s.", delete_after=5)
        elif isinstance(error, commands.CheckFailure):
             await ctx.send("You don't have the necessary permissions to use this command.", delete_after=10)
        elif isinstance(error, commands.BadArgument):
            await ctx.send(f"Invalid argument provided. {error}", delete_after=10)
        elif isinstance(error, commands.CommandInvokeError):
            original = error.original
            logger.error(f"Error executing command '{ctx.command.qualified_name}': {original}", exc_info=original)
            await ctx.send(f"An internal error occurred: `{type(original).__name__}`", delete_after=10)
        else:
            logger.error(f"Unhandled command error for '{ctx.command.qualified_name if ctx.command else 'unknown'}': {error}", exc_info=True)
            try:
                await ctx.send("An unexpected error occurred.", delete_after=10)
            except discord.HTTPException:
                 pass


    async def close(self):
        """Gracefully shuts down the bot and associated resources."""
        logger.info("--- Initiating Bot Shutdown Sequence ---")

        # unload extensions (cogs), allowing them to clean up tasks
        logger.info("Unloading extensions...")
        loaded_extensions = list(self.extensions.keys())
        for extension in loaded_extensions:
             try:
                 await self.unload_extension(extension)
                 logger.info(f"Unloaded extension: {extension}")
             except Exception as e:
                 logger.error(f"Error unloading extension {extension}: {e}")

        # close external connections (Econ)
        logger.info("Closing Teeworlds Econ connection...")
        await self.teeworlds_client.close()

        # close aiohttp session
        if self.http_session and not self.http_session.closed:
            logger.info("Closing aiohttp ClientSession...")
            await self.http_session.close()
            await asyncio.sleep(0.25)

        # close discord connection via super().close()
        logger.info("Closing Discord connection...")
        await super().close()
        logger.info("--- Bot Shutdown Complete ---")