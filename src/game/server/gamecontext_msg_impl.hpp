#ifndef GAME_SERVER_GAMECONTEXT_MSG_IMPL_HPP
#define GAME_SERVER_GAMECONTEXT_MSG_IMPL_HPP

#include "gamecontext.h"

template<typename... Ts>
void CGS::Chat(int ClientID, const char* pText, const Ts&... args)
{
    const int Start = std::max(ClientID, 0);
    const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

    for(int i = Start; i < End; i++)
    {
        if(m_apPlayers[i])
            SendChatTarget(i, fmt_localize(i, pText, args...).c_str());
    }

    if(Start == 0 && End == MAX_PLAYERS)
        Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "chat", "*** %s", fmt_default(pText, args...).c_str());
}

template<typename... Ts>
bool CGS::ChatAccount(int AccountID, const char* pText, const Ts&... args)
{
    if(CPlayer* pPlayer = GetPlayerByUserID(AccountID))
    {
        SendChatTarget(pPlayer->GetCID(), fmt_localize(pPlayer->GetCID(), pText, args...).c_str());
        return true;
    }
    return false;
}

template<typename... Ts>
void CGS::ChatGuild(int GuildID, const char* pText, const Ts&... args)
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
void CGS::ChatWorld(int WorldID, const char* pSuffix, const char* pText, const Ts&... args)
{
    const std::string Prefix = pSuffix ? std::string(pSuffix) + " " : "";
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && IsPlayerInWorld(i, WorldID))
        {
            SendChatTarget(i, (Prefix + fmt_localize(i, pText, args...)).c_str());
        }
    }
}

template<typename... Ts>
void CGS::Motd(int ClientID, const char* pText, const Ts&... args)
{
    const int Start = std::max(ClientID, 0);
    const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

    for(int i = Start; i < End; i++)
    {
        if(m_apPlayers[i])
            Server()->SendMotd(i, fmt_localize(i, pText, args...).c_str());
    }
}

template<typename... Ts>
void CGS::Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args)
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
void CGS::BroadcastWorld(int WorldID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args)
{
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        if(m_apPlayers[i] && IsPlayerInWorld(i, WorldID))
            AddBroadcast(i, fmt_localize(i, pText, args...).c_str(), Priority, LifeSpan);
    }
}

#endif // GAME_SERVER_GAMECONTEXT_MSG_IMPL_HPP
