# main.py
import asyncio
import signal
import os
import logging
import discord
from typing import Optional

# Project imports
from config import DISCORD_BOT_TOKEN
from utils.logger_setup import logger
from discord_bot.bot import DiscordBridgeBot

shutdown_task: Optional[asyncio.Task] = None

async def close_bot_task(bot: DiscordBridgeBot, sig: Optional[signal.Signals] = None):
    """Async task for closing bot."""
    global shutdown_task
    if sig:
        logger.info(f"Received signal {sig.name}. Initiating shutdown...")
    else:
        logger.info("Initiating shutdown...")

    # closing bot
    try:
        await bot.close()
        logger.info("Bot close sequence completed.")
    except Exception as e:
        logger.exception(f"Error during bot close sequence: {e}")
    finally:
        shutdown_task = None

def signal_handler(sig: signal.Signals, bot: DiscordBridgeBot):
    """Handler signals."""
    global shutdown_task
    if shutdown_task and not shutdown_task.done():
        logger.warning(f"Shutdown already in progress. Signal {sig.name} ignored.")
        return
    logger.info(f"Signal {sig.name} caught. Scheduling bot shutdown.")
    loop = asyncio.get_running_loop()
    shutdown_task = loop.create_task(close_bot_task(bot, sig))


async def main():
    """Initializes and runs the Discord bot."""
    logger.info("Starting Teeworlds Discord Bridge Bot...")

    # check default token
    if not DISCORD_BOT_TOKEN or DISCORD_BOT_TOKEN == "YOUR_DISCORD_BOT_TOKEN":
        logger.critical("DISCORD_BOT_TOKEN is not set or is default in config.py! Please configure it.")
        return

    bot = DiscordBridgeBot()

    # setting signal handlers (Unix)
    if os.name != 'nt':
         loop = asyncio.get_running_loop()
         for sig in (signal.SIGINT, signal.SIGTERM):
              # Передаем бота в обработчик
              loop.add_signal_handler(sig, signal_handler, sig, bot)
         logger.info(f"Signal handlers for SIGINT/SIGTERM installed.")
    else:
         logger.info("Running on Windows. Use Ctrl+C to trigger shutdown (may not be caught by signal handler).")

    try:
        logger.info("Starting bot login and connection...")
        await bot.start(DISCORD_BOT_TOKEN)
    except discord.LoginFailure:
        logger.critical("Login Failed: Invalid Discord Bot Token.")
    except discord.PrivilegedIntentsRequired as e:
        logger.critical(f"Login Failed: Privileged Gateway Intent(s) required but not enabled. Shard ID: {e.shard_id}")
        logger.critical("Please enable 'Message Content Intent' (and potentially others) in the Discord Developer Portal.")
    except Exception as e:
        logger.exception(f"An unexpected error occurred during bot execution: {e}")
    finally:
        logger.info("Bot execution loop has finished or been interrupted.")
        if not bot.is_closed() and (not shutdown_task or shutdown_task.done()):
             logger.info("Bot is not closed, attempting final cleanup task...")
             await close_bot_task(bot)
        logger.info("Exiting main async function.")


if __name__ == "__main__":
    exit_code = 0
    try:
        # run async
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("KeyboardInterrupt received in main. Forcing exit.")
        exit_code = 1
    except Exception as e:
        logger.critical(f"Fatal error during asyncio.run(main): {e}", exc_info=True)
        exit_code = 1
    finally:
        logging.info("="*20 + f" Application Terminated (Exit Code: {exit_code}) " + "="*20)
        logging.shutdown()