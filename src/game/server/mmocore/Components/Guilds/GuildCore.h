/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_CORE_H
#define GAME_SERVER_COMPONENT_GUILD_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "GuildData.h"

class GuildDoor;
class CDecorationHouses;
class GuildCore : public MmoComponent
{
	~GuildCore() override
	{
		CGuildData::ms_aGuild.clear();
		CGuildHouseData::ms_aHouseGuild.clear();
		CGuildRankData::ms_aRankGuild.clear();
	};

	std::map < int, CDecorationHouses* > m_DecorationHouse;

	void OnInit() override;
	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

private:
	void LoadGuildRank(int GuildID);
	void TickHousingText();

public:
	int SearchGuildByName(const char* pGuildName) const;

	const char *GuildName(int GuildID) const;
	int GetMemberAccess(CPlayer *pPlayer) const;
	bool CheckMemberAccess(CPlayer *pPlayer, int Access = GuildAccess::ACCESS_LEADER) const;
	int GetMemberChairBonus(int GuildID, int Field) const;

	void CreateGuild(CPlayer *pPlayer, const char *pGuildName);
	void DisbandGuild(int GuildID);
	bool JoinGuild(int AccountID, int GuildID);
	void ExitGuild(int AccountID);

private:
	void ShowMenuGuild(CPlayer *pPlayer) const;
	void ShowGuildPlayers(CPlayer *pPlayer, int GuildID);

public:
	void AddExperience(int GuildID);
	bool AddMoneyBank(int GuildID, int Money);
	bool RemoveMoneyBank(int GuildID, int Money);
	bool UpgradeGuild(int GuildID, int Field);
	bool AddDecorationHouse(int DecoID, int GuildID, vec2 Position);

private:
	bool DeleteDecorationHouse(int ID);
	void ShowDecorationList(CPlayer* pPlayer);

public:
	const char *AccessNames(int Access);
	const char *GetGuildRank(int GuildID, int RankID);
	int FindGuildRank(int GuildID, const char *Rank) const;

private:
	void AddRank(int GuildID, const char *Rank);
	void DeleteRank(int RankID, int GuildID);
	void ChangeRank(int RankID, int GuildID, const char *NewRank);
	void ChangeRankAccess(int RankID);
	void ChangePlayerRank(int AccountID, int RankID);
	void ShowMenuRank(CPlayer *pPlayer);

public:
	static int GetGuildPlayerValue(int GuildID);

private:
	void ShowInvitesGuilds(int ClientID, int GuildID);
	void ShowFinderGuilds(int ClientID);
	void SendInviteGuild(int GuildID, CPlayer* pPlayer);

	void ShowHistoryGuild(int ClientID, int GuildID);
	static void AddHistoryGuild(int GuildID, const char *Buffer, ...);

public:
	int GetHouseGuildID(int HouseID) const;
	int GetHouseWorldID(int HouseID) const;
	int GetPosHouseID(vec2 Pos) const;

	bool GetGuildDoor(int GuildID) const;
	vec2 GetPositionHouse(int GuildID) const;
	int GetGuildHouseID(int GuildID) const;

	void BuyGuildHouse(int GuildID, int HouseID);
	void SellGuildHouse(int GuildID);
	void ShowBuyHouse(CPlayer *pPlayer, int HouseID);
	bool ChangeStateDoor(int GuildID);
};

#endif
