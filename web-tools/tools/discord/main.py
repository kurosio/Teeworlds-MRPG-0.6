# main.py
"""
Teeworlds <-> Discord bridge for any number of game servers.

    python main.py [bridge.cfg]            run the bridge
    python main.py [bridge.cfg] --check-config   validate the config and exit
"""
import argparse
import asyncio
import logging
import os
import signal
import sys
from typing import Optional

# Project imports (discord is imported after the config is validated: a broken config fails fast)
from config import BridgeConfig, ConfigError, CONFIG_ENV_VAR, load_config, resolve_config_path

logger = logging.getLogger("TeeworldsBridge")
shutdown_task: Optional[asyncio.Task] = None


async def close_bot_task(bot, sig: Optional[signal.Signals] = None):
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


def signal_handler(sig: signal.Signals, bot):
    """Handler signals."""
    global shutdown_task
    if shutdown_task and not shutdown_task.done():
        logger.warning(f"Shutdown already in progress. Signal {sig.name} ignored.")
        return
    logger.info(f"Signal {sig.name} caught. Scheduling bot shutdown.")
    loop = asyncio.get_running_loop()
    shutdown_task = loop.create_task(close_bot_task(bot, sig))


async def main(config: BridgeConfig) -> int:
    """Initializes and runs the Discord bot. Returns the process exit code."""
    import discord
    from discord_bot.bot import DiscordBridgeBot

    logger.info("Starting Teeworlds Discord Bridge Bot...")
    logger.info(f"Config: {config.path}; servers: {', '.join(server.label for server in config.servers)}")

    bot = DiscordBridgeBot(config)
    exit_code = 0

    # setting signal handlers (Unix)
    if os.name != 'nt':
        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(sig, signal_handler, sig, bot)
        logger.info("Signal handlers for SIGINT/SIGTERM installed.")
    else:
        logger.info("Running on Windows. Use Ctrl+C to trigger shutdown (may not be caught by signal handler).")

    try:
        logger.info("Starting bot login and connection...")
        await bot.start(config.discord.token)
    except discord.LoginFailure:
        logger.critical("Login Failed: Invalid Discord Bot Token.")
        exit_code = 1
    except discord.PrivilegedIntentsRequired as e:
        logger.critical(f"Login Failed: Privileged Gateway Intent(s) required but not enabled. Shard ID: {e.shard_id}")
        logger.critical("Please enable 'Message Content Intent' (and potentially others) in the Discord Developer Portal.")
        exit_code = 1
    except Exception as e:
        logger.exception(f"An unexpected error occurred during bot execution: {e}")
        exit_code = 1
    finally:
        logger.info("Bot execution loop has finished or been interrupted.")
        if not bot.is_closed() and (not shutdown_task or shutdown_task.done()):
            logger.info("Bot is not closed, attempting final cleanup task...")
            await close_bot_task(bot)
        logger.info("Exiting main async function.")
    return exit_code


def parse_args(argv=None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Teeworlds <-> Discord bridge for any number of game servers.")
    parser.add_argument("config", nargs="?", default=None,
                        help=f"path to the config file (default: ${CONFIG_ENV_VAR} or bridge.cfg next to main.py)")
    parser.add_argument("--check-config", action="store_true", help="validate the config, print the servers and exit")
    return parser.parse_args(argv)


def run(argv=None) -> int:
    args = parse_args(argv)

    try:
        config = load_config(args.config)
    except ConfigError as e:
        print(f"Invalid configuration ({resolve_config_path(args.config)}):", file=sys.stderr)
        for problem in e.problems:
            print(f"  - {problem}", file=sys.stderr)
        return 2

    if args.check_config:
        print(f"Config OK: {config.path}")
        print(f"  bridge channel: {config.discord.channel_id}, status channel: {config.status.channel_id or 'disabled'}")
        for server in config.servers:
            print(f"  {server.label} {server.name}: econ {server.econ_host}:{server.econ_port}, "
                  f"game address {server.game_address or 'not set'}")
        return 0

    from utils.logger_setup import setup_logging
    setup_logging(config.logging)

    exit_code = 0
    try:
        # run async
        exit_code = asyncio.run(main(config))
    except KeyboardInterrupt:
        logger.info("KeyboardInterrupt received in main. Forcing exit.")
        exit_code = 1
    except Exception as e:
        logger.critical(f"Fatal error during asyncio.run(main): {e}", exc_info=True)
        exit_code = 1
    finally:
        logging.info("=" * 20 + f" Application Terminated (Exit Code: {exit_code}) " + "=" * 20)
        logging.shutdown()
    return exit_code


if __name__ == "__main__":
    sys.exit(run())
