# discord_bot/bot.py
import asyncio
import discord
from discord.ext import commands
import logging
import aiohttp
from typing import Optional # Добавили Optional

# Project imports
from config import COMMAND_PREFIX, DISCORD_CHANNEL_ID, STATUS_CHANNEL_ID
from services.teeworlds_client import TeeworldsEconClient
from services.server_status import ServerStatusService
from discord_bot.webhook_manager import WebhookManager
# Импортируем только для type hints в __init__
# from discord_bot.cogs.bridge import BridgeCog
# from discord_bot.cogs.status import StatusCog

logger = logging.getLogger("TeeworldsBridge.Bot")

class DiscordBridgeBot(commands.Bot):
    """
    Main Discord Bot class orchestrating Teeworlds bridge and status updates.
    Initializes services and loads cogs.
    """

    def __init__(self):
        intents = discord.Intents.default()
        intents.message_content = True # Нужно для чтения контента сообщений
        intents.guilds = True          # Нужно для поиска каналов/гильдий при старте
        intents.messages = True        # Нужно для on_message

        allowed_mentions = discord.AllowedMentions.none() # Запрещаем боту кого-либо пинговать

        super().__init__(
            command_prefix=COMMAND_PREFIX,
            intents=intents,
            allowed_mentions=allowed_mentions,
            help_command=None # Отключаем стандартную help команду, если не нужна
        )

        # --- Инициализация сервисов (некоторые требуют await в setup_hook) ---
        self.http_session: Optional[aiohttp.ClientSession] = None
        self.teeworlds_client = TeeworldsEconClient()
        self.status_service: Optional[ServerStatusService] = None
        self.webhook_manager: Optional[WebhookManager] = None

        # Список когов для загрузки
        self.initial_extensions = [
             'discord_bot.cogs.bridge',
             'discord_bot.cogs.status',
        ]


    async def setup_hook(self):
        """
        Asynchronous setup that runs before the bot logs in.
        Initialize services requiring async context and load cogs.
        """
        logger.info("Running setup_hook...")

        # Создаем сессию aiohttp
        self.http_session = aiohttp.ClientSession(
             timeout=aiohttp.ClientTimeout(total=20) # Глобальный таймаут для сессии
        )
        logger.info("aiohttp ClientSession created.")

        # Инициализируем сервисы, зависящие от сессии или бота
        if self.http_session:
            self.status_service = ServerStatusService(session=self.http_session)
            self.webhook_manager = WebhookManager(bot=self, http_session=self.http_session)
            logger.info("ServerStatusService and WebhookManager initialized.")
        else:
             # Это не должно произойти, но на всякий случай
             logger.critical("Failed to create aiohttp session! Status updates and webhooks will fail.")
             # Можно остановить бота здесь: raise RuntimeError("Failed to create aiohttp session")

        # Загружаем коги
        logger.info(f"Loading {len(self.initial_extensions)} extensions...")
        for extension in self.initial_extensions:
            try:
                await self.load_extension(extension)
                logger.info(f"Successfully loaded extension: {extension}")
            except commands.ExtensionNotFound:
                logger.error(f"Extension not found: {extension}")
            except commands.ExtensionAlreadyLoaded:
                logger.warning(f"Extension already loaded: {extension}") # Не должно происходить при старте
            except commands.NoEntryPointError:
                 # Эта ошибка больше не должна появляться после добавления setup()
                logger.error(f"Extension has no setup function: {extension}")
            except commands.ExtensionFailed as e:
                # Логируем оригинальную ошибку из кога
                logger.error(f"Extension failed to load: {extension}", exc_info=e.original)
            except Exception as e:
                 logger.exception(f"Unexpected error loading extension {extension}: {e}")

        logger.info("setup_hook finished.")


    async def on_ready(self):
        """Called when the bot is fully connected and ready."""
        logger.info("--- Bot Ready ---")
        logger.info(f"Logged in as: {self.user.name} ({self.user.id})")
        logger.info(f"Discord.py Version: {discord.__version__}")
        # Логгируем ID гильдий для диагностики
        guild_str = ", ".join([f"'{g.name}' ({g.id})" for g in self.guilds])
        logger.info(f"Connected to {len(self.guilds)} guild(s): {guild_str}")

        # Логируем ID каналов (коги теперь сами их ищут, но для инфо полезно)
        logger.info(f"Bridge Channel ID configured: {DISCORD_CHANNEL_ID}")
        logger.info(f"Status Channel ID configured: {STATUS_CHANNEL_ID}")

        # Устанавливаем статус бота
        try:
             activity = discord.Activity(type=discord.ActivityType.watching, name="Teeworlds Chat")
             await self.change_presence(activity=activity, status=discord.Status.online)
             logger.info("Bot presence set.")
        except Exception as e:
             logger.warning(f"Could not set presence: {e}")


    async def on_command_error(self, ctx: commands.Context, error: commands.CommandError):
        """Basic command error handling."""
        if isinstance(error, commands.CommandNotFound):
            # Не отвечаем на неизвестные команды
            return
        elif isinstance(error, commands.MissingRequiredArgument):
            await ctx.send(f"Missing argument: `{error.param.name}`. Use `{self.command_prefix}help {ctx.command}` for details.", delete_after=10)
        elif isinstance(error, commands.CommandOnCooldown):
             await ctx.send(f"Command on cooldown. Try again in {error.retry_after:.1f} seconds.", delete_after=5)
        elif isinstance(error, commands.CheckFailure):
             # Сюда попадают ошибки прав из commands.has_permissions и т.п.
             await ctx.send("You don't have permission to use this command here.", delete_after=10)
        elif isinstance(error, commands.CommandInvokeError):
            # Ошибка внутри самой команды
            original_error = error.original
            logger.error(f"Error executing command '{ctx.command}': {original_error}", exc_info=original_error)
            await ctx.send(f"An error occurred while executing the command: `{original_error}`", delete_after=10)
        else:
            # Другие ошибки (например, парсинга аргументов)
            logger.error(f"Unhandled command error for '{ctx.command}': {error}", exc_info=True)
            try:
                await ctx.send("An unexpected error occurred while running the command.", delete_after=10)
            except discord.HTTPException:
                 pass # Игнорируем, если не можем отправить сообщение об ошибке


    async def close(self):
        """Gracefully shuts down the bot and associated resources."""
        logger.info("--- Initiating Bot Shutdown Sequence ---")

        # Отменяем задачи (коги должны делать это в cog_unload, но на всякий случай)
        # for task in asyncio.all_tasks(): ... (может быть слишком агрессивно)

        # Выгружаем коги
        logger.info("Unloading extensions...")
        # Копируем ключи перед итерацией, так как словарь может меняться
        loaded_extensions = list(self.extensions.keys())
        for extension in loaded_extensions:
             try:
                 await self.unload_extension(extension)
                 logger.info(f"Successfully unloaded extension: {extension}")
             except Exception as e:
                 logger.error(f"Error unloading extension {extension}: {e}")

        # Закрываем Econ
        logger.info("Closing Teeworlds Econ connection...")
        await self.teeworlds_client.close()

        # Закрываем aiohttp сессию
        if self.http_session and not self.http_session.closed:
            logger.info("Closing aiohttp ClientSession...")
            await self.http_session.close()
            await asyncio.sleep(0.25) # Даем время на закрытие коннекторов

        # Закрываем соединение с Discord
        logger.info("Closing Discord connection...")
        await super().close()
        logger.info("--- Bot Shutdown Complete ---")
