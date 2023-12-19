/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_RANKS_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_RANKS_MANAGER_H

#include "GuildRankData.h"

class CGS;
class CGuildData;
using GuildRankContainer = std::deque<class CGuildRankData*>;

class CGuildRanksController
{
	CGS* GS() const;

	GuildRankContainer m_aRanks;
	CGuildData* m_pGuild;

public:
	CGuildRanksController() = delete;
	CGuildRanksController(CGuildData* pGuild);
	~CGuildRanksController();

	GuildRankContainer& GetContainer() { return m_aRanks; }

	bool Add(std::string Rank);
	bool Remove(std::string Rank);
	CGuildRankData* Get(std::string Rank) const;

private:
	void Init();
};


#endif
