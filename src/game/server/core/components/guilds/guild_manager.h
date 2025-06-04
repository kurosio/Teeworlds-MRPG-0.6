/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_GUILDS_GUILD_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_GUILDS_GUILD_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "guild_data.h"

class CGuildManager : public MmoComponent
{
	~CGuildManager() override
	{
		// free data
		mystd::freeContainer(CGuild::Data());
		mystd::freeContainer(CGuildHouse::Data());
		mystd::freeContainer(CGuildWarHandler::Data());
	};

	void OnPreInit() override;
	void OnInitWorld(const std::string& SqlQueryWhereWorld) override;
	void OnTick() override;
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;
	void OnGlobalTimePeriod(ETimePeriod Period) override;

	void InitWars() const;

public:
	void Create(CPlayer *pPlayer, const char *pGuildName) const;
	void Disband(GuildIdentifier GuildID) const;

private:
	void ShowMenu(int ClientID) const;
	void ShowUpgrades(CPlayer* pPlayer) const;
	void ShowDisband(CPlayer* pPlayer) const;
	void ShowHouseSell(CPlayer* pPlayer) const;
	void ShowRanksList(CPlayer* pPlayer) const;
	void ShowRankEdit(CPlayer* pPlayer, GuildRankIdentifier ID) const;
	void ShowFinder(CPlayer* pPlayer) const;
	void ShowFinderDetail(CPlayer* pPlayer, GuildIdentifier ID) const;
	void ShowLogsMenu(CPlayer* pPlayer) const;
	void ShowMembershipList(CPlayer* pPlayer) const;
	void ShowMembershipEdit(CPlayer* pPlayer, int AccountID) const;
	void ShowRequests(CPlayer* pPlayer) const;
	void ShowDetail(CPlayer* pPlayer, CGuildHouse* pHouse) const;
	void ShowDeclareWarMenu(int ClientID) const;
	void ShowDoorsControl(CPlayer* pPlayer) const;
	void ShowFarmzonesControl(CPlayer* pPlayer) const;
	void ShowFarmzoneEdit(CPlayer* pPlayer, int FarmzoneID) const;

public:
	CGuild* GetGuildByID(GuildIdentifier ID) const;
	CGuild* GetGuildByName(const char* pGuildname) const;

	CGuildHouse* GetHouseByID(const GuildHouseIdentifier& ID) const;
	CGuildHouse* GetHouseByPos(vec2 Pos) const;
	CFarmzone* GetHouseFarmzoneByPos(vec2 Pos) const;
};

#endif
