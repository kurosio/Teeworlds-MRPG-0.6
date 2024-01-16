/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_RANKS_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_RANKS_MANAGER_H

#include "GuildRankData.h"

// Forward declaration and alias
class CGS;
class CGuildData;
using GuildRankContainer = std::deque<class CGuildRankData*>;

// This class is a controller for managing guild ranks
class CGuildRanksManager
{
	// Pointer to the game server
	CGS* GS() const;

	CGuildRankData* m_pDefaultRank{};
	GuildRankContainer m_aRanks{};
	CGuildData* m_pGuild{};

public:

	// Constructor 
	CGuildRanksManager() = delete;
	CGuildRanksManager(CGuildData* pGuild, GuildRankIdentifier DefaultID);

	// Destructor
	~CGuildRanksManager();

	// Function to get the container of guild ranks
	GuildRankContainer& GetContainer() { return m_aRanks; }

	// Function to add a new guild rank
	[[nodiscard]] GUILD_RANK_RESULT Add(const std::string& Rank);

	// Function to remove an existing guild rank
	[[nodiscard]] GUILD_RANK_RESULT Remove(const std::string& Rank);

	// Function to get a guild rank by its name
	CGuildRankData* Get(const std::string& Rank) const;
	
	// Function to get a guild rank by its id
	CGuildRankData* Get(GuildRankIdentifier ID) const;

	// Function get default rank
	CGuildRankData* GetDefaultRank() const { return m_pDefaultRank; };

	// Function to initialize the default guild rank
	void UpdateDefaultRank();

private:

	// Function to initialize the guild ranks
	void Init(GuildRankIdentifier DefaultID);
};


#endif
