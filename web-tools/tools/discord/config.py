# config.py
"""
Configuration settings for the Teeworlds-Discord bridge.
"""
import logging

# --- Logging ---
LOG_FILE = "bridge.log"
LOG_LEVEL = logging.INFO
STATUS_UPDATE_LOG_LEVEL = logging.INFO

# --- Teeworlds Econ ---
TEESERVER_IP = "localhost"
TEESERVER_ECON_PORT = 7310
ECON_PASSWORD = "mmorf1312fd"
MAX_MESSAGE_LENGTH = 128
ECON_CONNECT_TIMEOUT = 10.0
ECON_READ_TIMEOUT = 60.0
ECON_AUTH_TIMEOUT = 5.0
SERVER_TAG_TEEWORLDS = "[DS]"

# --- Discord Bot ---
DISCORD_BOT_TOKEN = "...."
DISCORD_CHANNEL_ID = 552211416841584656
STATUS_CHANNEL_ID = 536497172728774667
STATUS_MESSAGE_ID = None
COMMAND_PREFIX = "!"
WEBHOOK_NAME = "Testing Bridge Bot"
WEBHOOK_SERVER_SKIN = "stargirl"
WEBHOOK_UNKNOWN_SKIN = "default"

# --- Timing ---
POLL_INTERVAL = 0.5
RECONNECT_DELAY = 15.0
STATUS_UPDATE_INTERVAL_MINUTES = 2

# --- Server Status ---
MASTER_SERVER_API_URL = "https://master1.ddnet.org/ddnet/15/servers.json"
SKIN_API_URL = ""
GAME_SERVER_ADDRESS = "103.27.156.253:8310"
SHOW_JOIN_BUTTON = True