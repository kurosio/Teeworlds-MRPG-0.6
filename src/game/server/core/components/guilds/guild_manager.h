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
		mrpgstd::free_container(CGuildHouse::Data(), CGuild::Data(), CGuildWarHandler::Data());
	};

	void OnInit() override;
	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnTick() override;
	bool OnCharacterTile(CCharacter* pChr) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnPlayerMenulist(CPlayer* pPlayer, int Menulist) override;
	void OnTimePeriod(ETimePeriod Period) override;

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
	void ShowLogs(CPlayer* pPlayer) const;
	void ShowMembershipList(CPlayer* pPlayer) const;
	void ShowMembershipEdit(CPlayer* pPlayer, int AccountID) const;
	void ShowRequests(CPlayer* pPlayer) const;
	void ShowBuyHouse(CPlayer* pPlayer, CGuildHouse* pHouse) const;
	void ShowDeclareWar(int ClientID) const;
	void ShowDoorsControl(CPlayer* pPlayer) const;
	void ShowFarmzonesControl(CPlayer* pPlayer) const;
	void ShowFarmzoneEdit(CPlayer* pPlayer, int FarmzoneID) const;

public:
	CGuildHouse* GetHouseByID(const GuildHouseIdentifier& ID) const;
	CGuildHouse* GetHouseByPos(vec2 Pos) const;
	CGuild* GetGuildByID(GuildIdentifier ID) const;
	CGuild* GetGuildByName(const char* pGuildname) const;
	CGuildHouse::CFarmzone* GetHouseFarmzoneByPos(vec2 Pos) const;
};

#endif
