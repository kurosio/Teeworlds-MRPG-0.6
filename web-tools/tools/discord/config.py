# config.py
"""
Configuration of the Teeworlds <-> Discord bridge.

Settings live in an INI file (bridge.cfg, see bridge.cfg.example). One bridge
process serves any number of game servers: every `[server:TAG]` section adds
one server, and TAG is what its messages are labelled with on Discord.
"""
import configparser
import logging
import os
import re
from dataclasses import dataclass, field
from typing import Dict, List, Mapping, Optional

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG_PATH = os.path.join(BASE_DIR, "bridge.cfg")
CONFIG_ENV_VAR = "DISCORD_BRIDGE_CONFIG"
TOKEN_ENV_VAR = "DISCORD_BOT_TOKEN"
ECON_PASSWORD_ENV_VAR = "ECON_PASSWORD"

SERVER_SECTION_PREFIX = "server:"
MAX_TAG_LENGTH = 16
TOKEN_PLACEHOLDERS = {"", "....", "YOUR_DISCORD_BOT_TOKEN"}
PASSWORD_PLACEHOLDER = "CHANGE_ME"

# 1..16 visible characters; no whitespace, brackets, quotes, colons, backslashes
_TAG_RE = re.compile(r"^[^\s\[\]\"'`:\\]{1,%d}$" % MAX_TAG_LENGTH)
_ADDRESS_RE = re.compile(r"^(?:\[[0-9A-Fa-f:.]+\]|[^\s:\[\]/]+):(\d{1,5})$")
_LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL")


class ConfigError(Exception):
    """Raised when the configuration file is missing or invalid."""

    def __init__(self, problems: List[str]):
        self.problems = list(problems)
        super().__init__("; ".join(self.problems))


@dataclass(frozen=True)
class ServerConfig:
    """One game server: its Econ endpoint and how it is shown on Discord."""
    tag: str
    name: str
    econ_host: str
    econ_port: int
    econ_password: str = field(repr=False)
    game_address: Optional[str] = None

    @property
    def label(self) -> str:
        """Tag as it is shown in front of nicknames, e.g. '[EU]'."""
        return f"[{self.tag}]"

    @property
    def key(self) -> str:
        """Case-insensitive identity of the server."""
        return self.tag.casefold()


@dataclass(frozen=True)
class EconSettings:
    connect_timeout: float = 10.0
    auth_timeout: float = 5.0
    read_timeout: float = 60.0
    send_timeout: float = 5.0
    reconnect_delay: float = 15.0
    reconnect_delay_max: float = 300.0


@dataclass(frozen=True)
class DiscordSettings:
    token: str = field(repr=False)
    channel_id: int = 0
    command_prefix: str = "!"
    admin_role_id: Optional[int] = None
    webhook_name: str = "Teeworlds Bridge"
    sync_commands: bool = True


@dataclass(frozen=True)
class BridgeSettings:
    discord_tag: str = "[DS]"
    max_message_length: int = 128
    max_discord_message_length: int = 300
    queue_size: int = 200
    webhook_send_timeout: float = 10.0
    player_cache_timeout: float = 3.0
    skin_api_url: str = "http://127.0.0.1:8000/render"
    server_skin: str = "bowlingball"
    unknown_skin: str = "default"


@dataclass(frozen=True)
class StatusSettings:
    channel_id: Optional[int] = None
    message_id: Optional[int] = None
    update_interval_minutes: float = 2.0
    master_url: str = "https://master1.ddnet.org/ddnet/15/servers.json"
    show_join_button: bool = True
    player_url: str = "https://ddnet.org/players/{name}"


@dataclass(frozen=True)
class LoggingSettings:
    file: Optional[str] = "bridge.log"
    level: int = logging.INFO
    status_level: int = logging.INFO


@dataclass(frozen=True)
class BridgeConfig:
    discord: DiscordSettings
    bridge: BridgeSettings
    status: StatusSettings
    econ: EconSettings
    logging: LoggingSettings
    servers: List[ServerConfig]
    path: Optional[str] = None

    def get_server(self, tag: str) -> Optional[ServerConfig]:
        key = tag.casefold()
        for server in self.servers:
            if server.key == key:
                return server
        return None


# option names accepted per section; anything else is reported as a typo
_KNOWN_OPTIONS: Dict[str, set] = {
    "discord": {"token", "channel_id", "command_prefix", "admin_role_id", "webhook_name", "sync_commands"},
    "bridge": {"discord_tag", "max_message_length", "max_discord_message_length", "queue_size",
               "webhook_send_timeout", "player_cache_timeout", "skin_api_url", "server_skin", "unknown_skin"},
    "status": {"channel_id", "message_id", "update_interval_minutes", "master_url", "show_join_button",
               "player_url"},
    "econ": {"host", "password", "connect_timeout", "auth_timeout", "read_timeout", "send_timeout",
             "reconnect_delay", "reconnect_delay_max"},
    "logging": {"file", "level", "status_level"},
    "server": {"name", "econ_host", "econ_port", "econ_password", "game_address", "enabled"},
}

_BOOL_VALUES = {"1": True, "yes": True, "true": True, "on": True,
                "0": False, "no": False, "false": False, "off": False}


class _Section:
    """Typed, error-collecting access to one INI section."""

    def __init__(self, parser: configparser.ConfigParser, name: str, problems: List[str],
                 known: Optional[set] = None):
        self.name = name
        self._problems = problems
        self._values: Dict[str, str] = dict(parser.items(name)) if parser.has_section(name) else {}
        if known is not None:
            for option in sorted(set(self._values) - known):
                self.problem(f"unknown option '{option}'")

    def problem(self, message: str):
        self._problems.append(f"[{self.name}] {message}")

    def raw(self, option: str) -> Optional[str]:
        """Returns the stripped value or None when it is missing/empty."""
        value = self._values.get(option)
        if value is None:
            return None
        value = value.strip()
        return value or None

    def text(self, option: str, default: str) -> str:
        value = self.raw(option)
        return default if value is None else value

    def integer(self, option: str, default: Optional[int], minimum: Optional[int] = None,
                maximum: Optional[int] = None) -> Optional[int]:
        value = self.raw(option)
        if value is None:
            return default
        try:
            number = int(value)
        except ValueError:
            self.problem(f"{option} must be an integer, got '{value}'")
            return default
        return self._in_range(option, number, default, minimum, maximum)

    def number(self, option: str, default: float, minimum: Optional[float] = None,
               maximum: Optional[float] = None) -> float:
        value = self.raw(option)
        if value is None:
            return default
        try:
            number = float(value)
        except ValueError:
            self.problem(f"{option} must be a number, got '{value}'")
            return default
        if number != number or number in (float("inf"), float("-inf")):
            self.problem(f"{option} must be a finite number, got '{value}'")
            return default
        return self._in_range(option, number, default, minimum, maximum)

    def boolean(self, option: str, default: bool) -> bool:
        value = self.raw(option)
        if value is None:
            return default
        parsed = _BOOL_VALUES.get(value.lower())
        if parsed is None:
            self.problem(f"{option} must be true/false, got '{value}'")
            return default
        return parsed

    def log_level(self, option: str, default: int) -> int:
        value = self.raw(option)
        if value is None:
            return default
        if value.upper() not in _LOG_LEVELS:
            self.problem(f"{option} must be one of {', '.join(_LOG_LEVELS)}, got '{value}'")
            return default
        return getattr(logging, value.upper())

    def _in_range(self, option, number, default, minimum, maximum):
        if minimum is not None and number < minimum:
            self.problem(f"{option} must be >= {minimum}, got {number}")
            return default
        if maximum is not None and number > maximum:
            self.problem(f"{option} must be <= {maximum}, got {number}")
            return default
        return number


def resolve_config_path(cli_path: Optional[str] = None, env: Optional[Mapping[str, str]] = None) -> str:
    """Command line argument > $DISCORD_BRIDGE_CONFIG > bridge.cfg next to main.py."""
    env = os.environ if env is None else env
    return os.path.abspath(cli_path or env.get(CONFIG_ENV_VAR) or DEFAULT_CONFIG_PATH)


def load_config(path: Optional[str] = None, env: Optional[Mapping[str, str]] = None) -> BridgeConfig:
    """Reads and validates the configuration file. Raises ConfigError with every problem found."""
    env = os.environ if env is None else env
    path = resolve_config_path(path, env)

    if not os.path.isfile(path):
        example = os.path.join(BASE_DIR, "bridge.cfg.example")
        raise ConfigError([f"config file not found: {path} (copy {example} to bridge.cfg and edit it)"])

    # no interpolation and no inline comments: passwords may contain %, # and ;
    parser = configparser.ConfigParser(interpolation=None)
    try:
        with open(path, "r", encoding="utf-8-sig") as config_file:
            parser.read_file(config_file)
    except (configparser.Error, OSError, UnicodeDecodeError) as e:
        raise ConfigError([f"cannot read {path}: {e}"])

    return parse_config(parser, env, path)


def parse_config(parser: configparser.ConfigParser, env: Optional[Mapping[str, str]] = None,
                 path: Optional[str] = None) -> BridgeConfig:
    """Builds a BridgeConfig from a parsed INI file."""
    env = os.environ if env is None else env
    problems: List[str] = []

    for section in parser.sections():
        if section not in _KNOWN_OPTIONS and not section.lower().startswith(SERVER_SECTION_PREFIX):
            problems.append(f"[{section}] unknown section (servers are declared as [server:TAG])")
        elif section == "server":
            problems.append("[server] the server tag is missing, use [server:TAG]")

    # --- [discord] ---
    section = _Section(parser, "discord", problems, _KNOWN_OPTIONS["discord"])
    token = (env.get(TOKEN_ENV_VAR) or "").strip() or section.text("token", "")
    if token in TOKEN_PLACEHOLDERS:
        section.problem(f"token is not set (or use the {TOKEN_ENV_VAR} environment variable)")
    channel_id = section.integer("channel_id", None, minimum=1)
    if channel_id is None:
        section.problem("channel_id is required (the channel the servers are bridged to)")
    command_prefix = section.text("command_prefix", "!")
    discord_settings = DiscordSettings(
        token=token,
        channel_id=channel_id or 0,
        command_prefix=command_prefix,
        admin_role_id=section.integer("admin_role_id", None, minimum=1),
        webhook_name=section.text("webhook_name", "Teeworlds Bridge")[:80],
        sync_commands=section.boolean("sync_commands", True),
    )

    # --- [bridge] ---
    section = _Section(parser, "bridge", problems, _KNOWN_OPTIONS["bridge"])
    discord_tag = section.text("discord_tag", "[DS]")
    if len(discord_tag) > 16:
        section.problem("discord_tag must be at most 16 characters")
    max_message_length = section.integer("max_message_length", 128, minimum=48, maximum=250)
    bridge_settings = BridgeSettings(
        discord_tag=discord_tag,
        max_message_length=max_message_length,
        max_discord_message_length=section.integer("max_discord_message_length", 300, minimum=16, maximum=2000),
        queue_size=section.integer("queue_size", 200, minimum=1, maximum=100000),
        webhook_send_timeout=section.number("webhook_send_timeout", 10.0, minimum=1.0),
        player_cache_timeout=section.number("player_cache_timeout", 3.0, minimum=0.1),
        skin_api_url=section.text("skin_api_url", "http://127.0.0.1:8000/render"),
        server_skin=section.text("server_skin", "bowlingball"),
        unknown_skin=section.text("unknown_skin", "default"),
    )

    # --- [status] ---
    section = _Section(parser, "status", problems, _KNOWN_OPTIONS["status"])
    status_settings = StatusSettings(
        channel_id=section.integer("channel_id", None, minimum=1),
        message_id=section.integer("message_id", None, minimum=1),
        update_interval_minutes=section.number("update_interval_minutes", 2.0, minimum=0.25),
        master_url=section.text("master_url", "https://master1.ddnet.org/ddnet/15/servers.json"),
        show_join_button=section.boolean("show_join_button", True),
        # an explicitly empty value disables the links
        player_url=section.raw("player_url") or ("" if "player_url" in section._values
                                                 else "https://ddnet.org/players/{name}"),
    )

    # --- [econ] (defaults shared by all servers) ---
    section = _Section(parser, "econ", problems, _KNOWN_OPTIONS["econ"])
    default_econ_host = section.text("host", "127.0.0.1")
    default_econ_password = (env.get(ECON_PASSWORD_ENV_VAR) or "").strip() or section.text("password", "")
    reconnect_delay = section.number("reconnect_delay", 15.0, minimum=1.0)
    reconnect_delay_max = section.number("reconnect_delay_max", 300.0, minimum=1.0)
    econ_settings = EconSettings(
        connect_timeout=section.number("connect_timeout", 10.0, minimum=0.5),
        auth_timeout=section.number("auth_timeout", 5.0, minimum=0.5),
        read_timeout=section.number("read_timeout", 60.0, minimum=1.0),
        send_timeout=section.number("send_timeout", 5.0, minimum=0.5),
        reconnect_delay=reconnect_delay,
        reconnect_delay_max=max(reconnect_delay, reconnect_delay_max),
    )

    # --- [logging] ---
    section = _Section(parser, "logging", problems, _KNOWN_OPTIONS["logging"])
    log_file = section.raw("file") or (None if "file" in section._values else "bridge.log")
    if log_file and path and not os.path.isabs(log_file):
        log_file = os.path.join(os.path.dirname(os.path.abspath(path)), log_file)
    logging_settings = LoggingSettings(
        file=log_file,
        level=section.log_level("level", logging.INFO),
        status_level=section.log_level("status_level", logging.INFO),
    )

    # --- [server:TAG] ---
    servers: List[ServerConfig] = []
    declared = 0
    seen_tags: Dict[str, str] = {}
    seen_econ: Dict[tuple, str] = {}
    for section_name in parser.sections():
        if not section_name.lower().startswith(SERVER_SECTION_PREFIX):
            continue
        declared += 1
        section = _Section(parser, section_name, problems, _KNOWN_OPTIONS["server"])
        tag = section_name[len(SERVER_SECTION_PREFIX):].strip()
        if not _TAG_RE.match(tag):
            section.problem(f"invalid tag '{tag}': 1-{MAX_TAG_LENGTH} characters without spaces, "
                            "brackets, quotes, colons or backslashes")
            continue
        if tag.casefold() in seen_tags:
            section.problem(f"duplicate tag (already used by [{SERVER_SECTION_PREFIX}{seen_tags[tag.casefold()]}])")
            continue
        seen_tags[tag.casefold()] = tag

        enabled = section.boolean("enabled", True)
        econ_host = section.text("econ_host", default_econ_host)
        econ_port = section.integer("econ_port", None, minimum=1, maximum=65535)
        econ_password = section.text("econ_password", default_econ_password)
        game_address = section.raw("game_address")

        if econ_port is None:
            section.problem("econ_port is required (ec_port of the game server)")
        if not econ_password or econ_password == PASSWORD_PLACEHOLDER:
            section.problem("econ_password is not set (and there is no default password in [econ])")
            econ_password = ""
        if game_address is not None:
            match = _ADDRESS_RE.match(game_address)
            if not match or not 0 < int(match.group(1)) <= 65535:
                section.problem(f"game_address must look like 1.2.3.4:8303, got '{game_address}'")
                game_address = None

        if not enabled or econ_port is None or not econ_password:
            continue

        econ_endpoint = (econ_host.lower(), econ_port)
        if econ_endpoint in seen_econ:
            section.problem(f"econ {econ_host}:{econ_port} is already used by "
                            f"[{SERVER_SECTION_PREFIX}{seen_econ[econ_endpoint]}]")
            continue
        seen_econ[econ_endpoint] = tag

        servers.append(ServerConfig(
            tag=tag,
            name=section.text("name", tag),
            econ_host=econ_host,
            econ_port=econ_port,
            econ_password=econ_password,
            game_address=game_address,
        ))

    if declared == 0:
        problems.append("no game servers configured: add at least one [server:TAG] section")
    elif not servers and not any(p.startswith(f"[{SERVER_SECTION_PREFIX}") for p in problems):
        problems.append("every [server:TAG] section is disabled: enable at least one server")

    if problems:
        raise ConfigError(problems)

    return BridgeConfig(
        discord=discord_settings,
        bridge=bridge_settings,
        status=status_settings,
        econ=econ_settings,
        logging=logging_settings,
        servers=servers,
        path=path,
    )
