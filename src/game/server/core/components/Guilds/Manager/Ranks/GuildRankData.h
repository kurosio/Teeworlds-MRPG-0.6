/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H

// Forward declaration and alias
class CGS;
class CPlayer;
class CGuildData;
using GuildRankIdentifier = int;

// Define an enum class for the different results of guild rank operations
enum class GUILD_RANK_RESULT : int
{
	ADD_LIMIT_HAS_REACHED,        // Cannot add more ranks, limit has been reached
	ADD_ALREADY_EXISTS,           // Rank with the same name already exists
	REMOVE_RANK_IS_DEFAULT,       // Cannot remove default rank
	REMOVE_RANK_DOES_NOT_EXIST,   // Rank to be removed does not exist
	RENAME_ALREADY_NAME_EXISTS,   // Cannot rename rank, name already exists
	WRONG_NUMBER_OF_CHAR_IN_NAME, // Wrong number of characters in the name
	SUCCESSFUL                    // Operation was successful
};

class CGuildRankData
{
	CGS* GS() const;

	GuildRankIdentifier m_ID {};
	std::string m_Rank {};
	GuildRankAccess m_Access {};
	CGuildData* m_pGuild {};

public:
	CGuildRankData() = delete;
	CGuildRankData(GuildRankIdentifier RID, std::string&& Rank, GuildRankAccess Access, CGuildData* pGuild);

	// Get the unique identifier of the guild rank
	GuildRankIdentifier GetID() const { return m_ID; }

	// Get the name of the guild rank
	const char* GetName() const { return m_Rank.c_str(); }

	// Get the name of the access level of the guild rank
	const char* GetAccessName() const;

	// Change the name of the guild rank
	[[nodiscard]] GUILD_RANK_RESULT Rename(std::string NewRank);

	// Change the access level of the guild rank
	void ChangeAccess();

	// Set the access level of the guild rank
	void SetAccess(GuildRankAccess Access);

	// Get the rank access
	const GuildRankAccess& GetAccess() const { return m_Access; }
};

#endif
