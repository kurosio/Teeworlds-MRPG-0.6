#include <engine/server.h>
#include "input_events.h"

void CInputEvents::ParseInputClickedKeys(int ClientID, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput)
{
    auto AppendIfFlagChanged = [&](int flag, int event)
    {
        if((pNewInput->m_PlayerFlags & flag) && !(pLastInput->m_PlayerFlags & flag))
        {
            AppendEventKeyClick(ClientID, event);
        }
    };

    AppendIfFlagChanged(PLAYERFLAG_IN_MENU, KEY_EVENT_MENU);
    AppendIfFlagChanged(PLAYERFLAG_SCOREBOARD, KEY_EVENT_SCOREBOARD);
    AppendIfFlagChanged(PLAYERFLAG_CHATTING, KEY_EVENT_CHAT);

    if(pNewInput->m_Jump && !pLastInput->m_Jump)
    {
        AppendEventKeyClick(ClientID, KEY_EVENT_JUMP);
    }

    if(pNewInput->m_Hook && !pLastInput->m_Hook)
    {
        AppendEventKeyClick(ClientID, KEY_EVENT_HOOK);
    }
}

void CInputEvents::ProcessKeyPress(int ClientID, int LastInput, int NewInput, int EventKey, int ActiveWeaponKey)
{
    if(CountInput(LastInput, NewInput).m_Presses)
    {
        AppendEventKeyClick(ClientID, EventKey);
        if(ActiveWeaponKey != -1)
        {
            AppendEventKeyClick(ClientID, ActiveWeaponKey);
        }
    }
}

void CInputEvents::ProcessCharacterInput(int ClientID, int ActiveWeapon, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput)
{
    ProcessKeyPress(ClientID, pLastInput->m_Fire, pNewInput->m_Fire, KEY_EVENT_FIRE, 1 << (KEY_EVENT_FIRE + ActiveWeapon));
    ProcessKeyPress(ClientID, pLastInput->m_NextWeapon, pNewInput->m_NextWeapon, KEY_EVENT_NEXT_WEAPON);
    ProcessKeyPress(ClientID, pLastInput->m_PrevWeapon, pNewInput->m_PrevWeapon, KEY_EVENT_PREV_WEAPON);

    if(pLastInput->m_WantedWeapon != pNewInput->m_WantedWeapon)
    {
        AppendEventKeyClick(ClientID, KEY_EVENT_WANTED_WEAPON);
        AppendEventKeyClick(ClientID, KEY_EVENT_WANTED_WEAPON << (pNewInput->m_WantedWeapon));
    }
}

void CInputEvents::AppendEventKeyClick(int ClientID, int KeyID)
{
    if(ClientID >= 0 && ClientID < MAX_CLIENTS)
    {
        m_aActionEventKeys[ClientID] |= KeyID;
    }
}

bool CInputEvents::IsKeyClicked(int ClientID, int KeyID)
{
    return (ClientID >= 0 && ClientID < MAX_CLIENTS) ? (m_aActionEventKeys[ClientID] & KeyID) != 0 : false;
}

void CInputEvents::BlockInputGroup(int ClientID, int64_t FlagBlockedGroup)
{
    if(ClientID >= 0 && ClientID < MAX_CLIENTS)
    {
        m_aBlockedInputKeys[ClientID] |= FlagBlockedGroup;
    }
}

bool CInputEvents::IsBlockedInputGroup(int ClientID, int64_t FlagBlockedGroup)
{
    return (ClientID >= 0 && ClientID < MAX_CLIENTS) ? (m_aBlockedInputKeys[ClientID] & FlagBlockedGroup) != 0 : false;
}

CInputEvents* CreateInputKeys() { return new CInputEvents; }