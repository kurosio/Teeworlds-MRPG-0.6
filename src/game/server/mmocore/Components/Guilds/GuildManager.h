/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_CORE_H
#define GAME_SERVER_COMPONENT_GUILD_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "GuildData.h"

class CEntityGuildDoor;
class CDecorationHouses;
class CGuildManager : public MmoComponent
{
	~CGuildManager() override
	{
		CGuildData::Data().clear();
		CGuildData::Data().shrink_to_fit();
		CGuildHouseData::Data().clear();
		CGuildHouseData::Data().shrink_to_fit();
	};

	std::map < int, CDecorationHouses* > m_DecorationHouse;

	void OnInit() override;
	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

public:
	void CreateGuild(CPlayer *pPlayer, const char *pGuildName);

private:
	void ShowMenuRank(CPlayer *pPlayer);

	void ShowInvitesGuilds(int ClientID, int GuildID);
	void ShowFinderGuilds(int ClientID);
	void SendInviteGuild(int GuildID, CPlayer* pPlayer);

	void ShowHistoryGuild(int ClientID);
public:
	CGuildHouseData* GetGuildHouseByPos(vec2 Pos) const;
	CGuildData* GetGuildByID(GuildIdentifier ID) const;

	void ShowBuyHouse(CPlayer *pPlayer, CGuildHouseData* pHouse);
};

#endif
