/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_CORE_H
#define GAME_SERVER_COMPONENT_GUILD_CORE_H
#include <game/server/core/mmo_component.h>

#include "GuildData.h"

class CGuildManager : public MmoComponent
{
	~CGuildManager() override
	{
		for(const auto pHouse : CGuildHouseData::Data())
			delete pHouse;
		for(const auto pGuild : CGuildData::Data())
			delete pGuild;

		CGuildData::Data().clear();
		CGuildHouseData::Data().clear();
		CGuildData::Data().shrink_to_fit();
		CGuildHouseData::Data().shrink_to_fit();
	};

	void OnInit() override;
	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	void OnHandleTimePeriod(TIME_PERIOD Period) override;

public:
	void Create(CPlayer *pPlayer, const char *pGuildName) const;
	void Disband(GuildIdentifier GuildID) const;

private:
	void ShowMenu(int ClientID) const;
	void ShowRanksSettings(int ClientID) const;
	void ShowFinder(int ClientID) const;
	void ShowLogs(int ClientID) const;
	void ShowMembershipList(int ClientID) const;
	void ShowMembershipList(int ClientID, GuildIdentifier ID) const;
	void ShowRequests(int ClientID) const;
	void ShowPlantZone(int ClientID, int PlantzoneID) const;
	void ShowBuyHouse(int ClientID, CGuildHouseData* pHouse) const;

public:
	CGuildHouseData* GetGuildHouseByID(const GuildHouseIdentifier& ID) const;
	CGuildHouseData* GetGuildHouseByPos(vec2 Pos) const;
	CGuildData* GetGuildByID(GuildIdentifier ID) const;
	CGuildData* GetGuildByName(const char* pGuildname) const;
	CGuildHousePlantzoneData* GetGuildHousePlantzoneByPos(vec2 Pos) const;
	bool IsAccountMemberGuild(int AccountID) const;

};

#endif
