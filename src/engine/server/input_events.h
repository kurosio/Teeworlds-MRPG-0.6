#ifndef ENGINE_SERVER_INPUT_EVENTS_H
#define ENGINE_SERVER_INPUT_EVENTS_H

#include <engine/input_events.h>

class CInputEvents : public IInputEvents
{
    int64_t m_aActionEventKeys[MAX_CLIENTS]{};
    int64_t m_aBlockedInputKeys[MAX_CLIENTS]{};

public:
    void ParseInputClickedKeys(int ClientID, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput) override;
    void ProcessKeyPress(int ClientID, int LastInput, int NewInput, int EventKey, int ActiveWeaponKey = -1) override;
    void ProcessCharacterInput(int ClientID, int ActiveWeapon, const CNetObj_PlayerInput* pNewInput, const CNetObj_PlayerInput* pLastInput) override;

    void AppendEventKeyClick(int ClientID, int KeyID) override;
    bool IsKeyClicked(int ClientID, int KeyID) override;
    void BlockInputGroup(int ClientID, int64_t FlagBlockedGroup) override;
    bool IsBlockedInputGroup(int ClientID, int64_t FlagBlockedGroup) override;

    void ResetInputKeys()
    {
        mem_zero(m_aActionEventKeys, sizeof(m_aActionEventKeys));
    }
    void ResetClientBlockKeys(int ClientID)
    {
        if(ClientID >= 0 && ClientID < MAX_CLIENTS)
            m_aBlockedInputKeys[ClientID] = 0;
    }
};

extern CInputEvents* CreateInputKeys();

#endif // ENGINE_SERVER_INPUT_EVENTS_H