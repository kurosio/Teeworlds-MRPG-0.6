# utils/logger_setup.py
import logging
import sys
from config import LOG_FILE, LOG_LEVEL

def setup_logging():
    """Configures logging to console and file."""
    log_format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    log_datefmt = '%Y-%m-%d %H:%M:%S'

    handlers = [
        logging.StreamHandler(sys.stdout),
        logging.FileHandler(LOG_FILE, encoding='utf-8', mode='a')
    ]

    logging.basicConfig(
        level=LOG_LEVEL,
        format=log_format,
        datefmt=log_datefmt,
        handlers=handlers,
        force=True
    )

    logging.getLogger("discord").setLevel(logging.WARNING)
    logging.getLogger("websockets").setLevel(logging.WARNING)
    logging.getLogger("aiohttp").setLevel(logging.WARNING)

    logger = logging.getLogger("TeeworldsBridge")
    logger.info("="*20 + " Logger Initialized (Level: %s) " + "="*20, logging.getLevelName(LOG_LEVEL))
    return logger

logger = setup_logging()