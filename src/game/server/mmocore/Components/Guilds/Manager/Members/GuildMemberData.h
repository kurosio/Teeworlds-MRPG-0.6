/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBER_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBER_DATA_H

#include "../Ranks/GuildRanksManager.h"

class CGuildData;

class CGuildMember
{
	CGuildData* m_pGuild {};
	GuildRankIdentifier m_RankID{};
	int m_AccountID {};
	int m_Deposit {};

public:
	CGuildMember(CGuildData* pGuild, int AccountID, GuildRankIdentifier = -1, int Deposit = 0);
};

#endif