#ifndef GAME_SERVER_MESSAGES_IMPL_H
#define GAME_SERVER_MESSAGES_IMPL_H

#include "gamecontext.h"

template<typename... Ts>
inline void Chat(int ClientID, const char* pText, const Ts&... args)
{
    const int Start = std::max(ClientID, 0);
    const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

    for(int i = Start; i < End; i++)
    {
        if(m_apPlayers[i])
            SendChatTarget(i, fmt_localize(i, pText, args...).c_str());
    }
}

template<typename... Ts>
inline bool ChatAccount(int AccountID, const char* pText, const Ts&... args)
{
    if(CPlayer* pPlayer = GetPlayerByUserID(AccountID))
    {
        SendChatTarget(pPlayer->GetCID(), fmt_localize(pPlayer->GetCID(), pText, args...).c_str());
        return true;
    }
    return false;
}

template<typename... Ts>
inline void ChatGuild(int GuildID, const char* pText, const Ts&... args)
{
    const std::string GuildPrefix = "Guild | ";
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && pPlayer->Account()->IsSameGuild(GuildID))
        {
            SendChatTarget(i, (GuildPrefix + fmt_localize(i, pText, args...)).c_str());
        }
    }
}

template<typename... Ts>
inline void ChatWorld(int WorldID, const char* pSuffix, const char* pText, const Ts&... args)
{
    const std::string Prefix = pSuffix[0] != '\0' ? std::string(pSuffix) + " " : "";
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && IsPlayerInWorld(i, WorldID))
        {
            SendChatTarget(i, (Prefix + fmt_localize(i, pText, args...)).c_str());
        }
    }
}

template<typename... Ts>
inline void Motd(int ClientID, const char* pText, const Ts&... args)
{
    const int Start = std::max(ClientID, 0);
    const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

    for(int i = Start; i < End; i++)
    {
        if(m_apPlayers[i])
            SendMotd(i, fmt_localize(i, pText, args...).c_str());
    }
}

template<typename... Ts>
inline void Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args)
{
    const int Start = std::max(ClientID, 0);
    const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

    for(int i = Start; i < End; i++)
    {
        if(m_apPlayers[i])
            AddBroadcast(i, fmt_localize(i, pText, args...).c_str(), Priority, LifeSpan);
    }
}

template<typename... Ts>
inline void BroadcastWorld(int WorldID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args)
{
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        if(m_apPlayers[i] && IsPlayerInWorld(i, WorldID))
            AddBroadcast(i, fmt_localize(i, pText, args...).c_str(), Priority, LifeSpan);
    }
}

#endif // GAME_SERVER_MESSAGES_IMPL_H