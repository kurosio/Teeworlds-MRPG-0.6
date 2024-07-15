#include "event_key_manager.h"
#include <engine/server.h>
#include "player.h"

void CEventKeyManager::ParseInputClickedKeys(CPlayer* pPlayer, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput)
{
    auto AppendIfFlagChanged = [&](int flag, int event)
    {
        if((pNewInput->m_PlayerFlags & flag) && !(pLastInput->m_PlayerFlags & flag))
        {
            AppendEventKeyClick(pPlayer->GetCID(), event);
        }
    };

    AppendIfFlagChanged(PLAYERFLAG_IN_MENU, KEY_EVENT_MENU);
    AppendIfFlagChanged(PLAYERFLAG_SCOREBOARD, KEY_EVENT_SCOREBOARD);
    AppendIfFlagChanged(PLAYERFLAG_CHATTING, KEY_EVENT_CHAT);

    if(pNewInput->m_Jump && !pLastInput->m_Jump)
    {
        AppendEventKeyClick(pPlayer->GetCID(), KEY_EVENT_JUMP);
    }

    if(pNewInput->m_Hook && !pLastInput->m_Hook)
    {
        AppendEventKeyClick(pPlayer->GetCID(), KEY_EVENT_HOOK);
    }
}

void CEventKeyManager::ProcessKeyPress(int ClientID, int LastInput, int NewInput, int EventKey, int ActiveWeaponKey)
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

void CEventKeyManager::ProcessCharacterInput(CPlayer* pPlayer, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput)
{
    if(!pPlayer || !pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
    {
        return;
    }

    int ClientID = pPlayer->GetCID();
    ProcessKeyPress(ClientID, pLastInput->m_Fire, pNewInput->m_Fire, KEY_EVENT_FIRE, 1 << (KEY_EVENT_FIRE + pPlayer->GetCharacter()->m_Core.m_ActiveWeapon));
    ProcessKeyPress(ClientID, pLastInput->m_NextWeapon, pNewInput->m_NextWeapon, KEY_EVENT_NEXT_WEAPON);
    ProcessKeyPress(ClientID, pLastInput->m_PrevWeapon, pNewInput->m_PrevWeapon, KEY_EVENT_PREV_WEAPON);

    if(pLastInput->m_WantedWeapon != pNewInput->m_WantedWeapon)
    {
        AppendEventKeyClick(ClientID, KEY_EVENT_WANTED_WEAPON);
        AppendEventKeyClick(ClientID, KEY_EVENT_WANTED_WEAPON << (pNewInput->m_WantedWeapon));
    }
}

void CEventKeyManager::AppendEventKeyClick(int ClientID, int KeyID)
{
    if(ClientID >= 0 && ClientID < MAX_CLIENTS)
    {
        Instance::Server()->GetClientInputFlags(ClientID) |= KeyID;
    }
}

bool CEventKeyManager::IsKeyClicked(int ClientID, int KeyID)
{
    return (ClientID >= 0 && ClientID < MAX_CLIENTS) ? (Instance::Server()->GetClientInputFlags(ClientID) & KeyID) != 0 : false;
}

void CEventKeyManager::BlockInputGroup(int ClientID, int64_t FlagBlockedGroup)
{
    if(ClientID >= 0 && ClientID < MAX_CLIENTS)
    {
        Instance::Server()->GetClientInputBlockedFlags(ClientID) |= FlagBlockedGroup;
    }
}

bool CEventKeyManager::IsBlockedInputGroup(int ClientID, int64_t FlagBlockedGroup)
{
    return (ClientID >= 0 && ClientID < MAX_CLIENTS) ? (Instance::Server()->GetClientInputBlockedFlags(ClientID) & FlagBlockedGroup) != 0 : false;
}