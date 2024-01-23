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

public:
	void Create(CPlayer *pPlayer, const char *pGuildName) const;
	void Disband(GuildIdentifier GuildID) const;

private:
	void ShowMenu(CPlayer* pPlayer) const;
	void ShowRanksSettings(CPlayer *pPlayer) const;
	void ShowFinder(int ClientID) const;
	void ShowLogs(int ClientID) const;
	void ShowPlayerlist(CPlayer* pPlayer) const;
	void ShowPlayerlist(CPlayer* pPlayer, GuildIdentifier ID) const;
	void ShowRequests(int ClientID) const;

public:
	CGuildHouseData* GetGuildHouseByID(const GuildHouseIdentifier& ID) const;
	CGuildHouseData* GetGuildHouseByPos(vec2 Pos) const;
	CGuildData* GetGuildByID(GuildIdentifier ID) const;
	CGuildData* GetGuildByName(const char* pGuildname) const;
	bool IsAccountMemberGuild(int AccountID) const;

	void ShowBuyHouse(CPlayer *pPlayer, CGuildHouseData* pHouse) const;
};

#endif
