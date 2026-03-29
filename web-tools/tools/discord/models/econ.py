# models/econ.py
from typing import Union, TypedDict, Literal

class EconChatMessage(TypedDict):
    type: Literal["chat"]
    nickname: str
    message: str

class EconJoinMessage(TypedDict): # Placeholder, currently unused
    type: Literal["join"]
    nickname: str

class EconLeaveMessage(TypedDict): # Placeholder, currently unused
    type: Literal["leave"]
    nickname: str
    reason: str

class EconOtherMessage(TypedDict):
    type: Literal["other"]
    raw: str

# Union type for easier type checking in handlers
EconMessage = Union[EconChatMessage, EconJoinMessage, EconLeaveMessage, EconOtherMessage, None]