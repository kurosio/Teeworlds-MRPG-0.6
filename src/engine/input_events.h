#ifndef ENGINE_INPUT_EVENTS_H
#define ENGINE_INPUT_EVENTS_H

#include <cstdint>

struct CNetObj_PlayerInput;

enum InputEvents
{
	// Key events for firing different weapons
	KEY_EVENT_FIRE = 1 << 0,
	KEY_EVENT_FIRE_HAMMER = 1 << 1,
	KEY_EVENT_FIRE_GUN = 1 << 2,
	KEY_EVENT_FIRE_SHOTGUN = 1 << 3,
	KEY_EVENT_FIRE_GRENADE = 1 << 4,
	KEY_EVENT_FIRE_LASER = 1 << 5,
	KEY_EVENT_FIRE_NINJA = 1 << 6,

	// Key events for voting
	KEY_EVENT_VOTE_YES = 1 << 7,
	KEY_EVENT_VOTE_NO = 1 << 8,

	// Key event for player states
	KEY_EVENT_SCOREBOARD = 1 << 9,
	KEY_EVENT_CHAT = 1 << 10,

	// Key events for player actions
	KEY_EVENT_JUMP = 1 << 11,
	KEY_EVENT_HOOK = 1 << 12,
	KEY_EVENT_SELF_KILL = 1 << 13,

	// Key events for changing weapons
	KEY_EVENT_NEXT_WEAPON = 1 << 14,
	KEY_EVENT_PREV_WEAPON = 1 << 15,
	KEY_EVENT_MENU = 1 << 16,
	KEY_EVENT_WANTED_WEAPON = 1 << 17,
	KEY_EVENT_WANTED_HAMMER = 1 << 18,
	KEY_EVENT_WANTED_GUN = 1 << 19,
	KEY_EVENT_WANTED_SHOTGUN = 1 << 20,
	KEY_EVENT_WANTED_GRENADE = 1 << 21,
	KEY_EVENT_WANTED_LASER = 1 << 22,

	// Blocking states for input events
	BLOCK_INPUT_FREEZE_WEAPON = 1 << 0,
	BLOCK_INPUT_FREEZE_HAMMER = 1 << 1,
	BLOCK_INPUT_FREEZE_GUN = 1 << 2,
	BLOCK_INPUT_FREEZE_SHOTGUN = 1 << 3,
	BLOCK_INPUT_FREEZE_GRENADE = 1 << 4,
	BLOCK_INPUT_FREEZE_LASER = 1 << 5,
	BLOCK_INPUT_FIRE = 1 << 6,
	BLOCK_INPUT_HOOK = 1 << 7,
	BLOCK_INPUT_FULL_WEAPON = BLOCK_INPUT_FREEZE_WEAPON | BLOCK_INPUT_FIRE,
};

class IInputEvents
{
public:
    virtual ~IInputEvents() = default;

	virtual void ParseInputClickedKeys(int ClientID, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput) = 0;
    virtual void ProcessKeyPress(int ClientID, int LastInput, int NewInput, int EventKey, int ActiveWeaponKey = -1) = 0;
    virtual void ProcessCharacterInput(int ClientID, int ActiveWeapon, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput) = 0;

    virtual void AppendEventKeyClick(int ClientID, int KeyID) = 0;
    virtual bool IsKeyClicked(int ClientID, int KeyID) = 0;
    virtual void BlockInputGroup(int ClientID, int64_t FlagBlockedGroup) = 0;
    virtual bool IsBlockedInputGroup(int ClientID, int64_t FlagBlockedGroup) = 0;
};

#endif // ENGINE_INPUT_EVENTS_H