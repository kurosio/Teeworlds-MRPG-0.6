/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H

// Forward declaration and alias
class CGS;
class CPlayer;
class CGuildData;
using GuildRankIdentifier = int;

// access guild
enum GuildRankAccess
{
	RIGHTS_LEADER = -1,
	RIGHTS_DEFAULT = 0,
	RIGHTS_INVITE_KICK,
	RIGHTS_UPGRADES_HOUSE,
	RIGHTS_FULL,
	RIGHTS_NUM,
};

enum class GUILD_RANK_RESULT : int
{
	ADD_LIMIT_HAS_REACHED,
	ADD_ALREADY_EXISTS,

	REMOVE_RANK_IS_DEFAULT,
	REMOVE_RANK_DOES_NOT_EXIST,

	RENAME_ALREADY_NAME_EXISTS,

	SUCCESSFUL
};

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
	const char* GetAccessName() const;

	// Method to change the name of the guild rank
	GUILD_RANK_RESULT ChangeName(std::string NewRank);

	// Method to change the access level of the guild rank
	void ChangeAccess();
	bool CheckAccess(CPlayer* pPlayer, GuildRankAccess Access) const;
};

#endif
