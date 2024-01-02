/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_MEMBER_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_MEMBER_DATA_H

#include "../Ranks/GuildRankData.h"

class CGuildData;
class CGuildRankData;

enum class GUILD_MEMBER_RESULT : int
{
	JOIN_ALREADY_IN_GUILD,
	KICK_DOES_NOT_EXIST,

	SUCCESSFUL
};

class CGuildMemberData
{
	CGS* GS() const;

	CGuildData* m_pGuild {};
	CGuildRankData* m_pRank {};
	int m_AccountID {};
	int m_Deposit {};

public:
	CGuildMemberData(CGuildData* pGuild, int AccountID, CGuildRankData* pRank, int Deposit = 0);
	~CGuildMemberData();

	int GetAccountID() const { return m_AccountID; }
	int GetDeposit() const { return m_Deposit; }

	CGuildRankData* GetRank() const;
	bool SetRank(GuildRankIdentifier RankID);
	bool SetRank(CGuildRankData* pRank);

	bool DepositInBank(int Golds);
	bool WithdrawFromBank(int Golds);
};

#endif
