/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBER_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBER_DATA_H

#include "../Ranks/GuildRankData.h"

class CGuildData;

class CGuildMemberData
{
	CGuildData* m_pGuild {};
	GuildRankIdentifier m_RankID{};
	int m_AccountID {};
	int m_Deposit {};

public:
	CGuildMemberData(CGuildData* pGuild, int AccountID, GuildRankIdentifier = -1, int Deposit = 0);

	int GetAccountID() const { return m_AccountID; }
	int GetDeposit() const { return m_Deposit; }
};

#endif
