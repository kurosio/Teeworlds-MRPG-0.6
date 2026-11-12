# models/econ.py
"""Events parsed from the Econ output of a game server."""
from dataclasses import dataclass
from typing import Union

SYSTEM_NICKNAME = "***"


@dataclass(frozen=True)
class EconChatMessage:
    """A public chat line: a player message or a server ('***') message."""
    nickname: str
    message: str
    is_system: bool = False


@dataclass(frozen=True)
class EconBridgeToggle:
    """A player switched the relay of his own messages on/off (/bridge_enable, /bridge_disable)."""
    nickname: str
    enabled: bool


EconEvent = Union[EconChatMessage, EconBridgeToggle]
