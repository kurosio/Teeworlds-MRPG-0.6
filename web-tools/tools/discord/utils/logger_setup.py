# utils/logger_setup.py
import logging
import sys

from config import LoggingSettings

ROOT_LOGGER_NAME = "TeeworldsBridge"
logger = logging.getLogger(ROOT_LOGGER_NAME)


def setup_logging(settings: LoggingSettings) -> logging.Logger:
    """Configures logging to console and (optionally) to a file."""
    log_format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    log_datefmt = '%Y-%m-%d %H:%M:%S'

    handlers = [logging.StreamHandler(sys.stdout)]
    if settings.file:
        handlers.append(logging.FileHandler(settings.file, encoding='utf-8', mode='a'))

    logging.basicConfig(
        level=settings.level,
        format=log_format,
        datefmt=log_datefmt,
        handlers=handlers,
        force=True
    )

    logging.getLogger("discord").setLevel(logging.WARNING)
    logging.getLogger("websockets").setLevel(logging.WARNING)
    logging.getLogger("aiohttp").setLevel(logging.WARNING)

    logger.info("=" * 20 + " Logger Initialized (Level: %s) " + "=" * 20, logging.getLevelName(settings.level))
    return logger
