/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H

// Forward declaration and alias
class CGS;
class CGuildData;
using GuildRankIdentifier = int;

// This class represents the data of a guild rank
class CGuildRankData
{
	// Pointer to the game server
	CGS* GS() const;

	GuildRankIdentifier m_ID {};
	std::string m_Rank {};
	int m_Access {};
	CGuildData* m_pGuild {};

public:
	// Constructor
	CGuildRankData() = delete;
	CGuildRankData(GuildRankIdentifier RID, std::string&& Rank, int Access, CGuildData* pGuild);

	// Getter for the guild rank identifier
	GuildRankIdentifier GetID() const { return m_ID; }

	// Getter for the guild rank name
	const char* GetName() const { return m_Rank.c_str(); }

	// Getter for the access level name
	const char* GetAccessName();

	// Method to change the name of the guild rank
	void ChangeName(std::string NewRank);

	// Method to change the access level of the guild rank
	void ChangeAccess(int Access);

	bool CheckAccess(int Access);
};

#endif
