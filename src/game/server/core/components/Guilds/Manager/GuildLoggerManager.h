/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_LOGGER_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_LOGGER_MANAGER_H

// Define the structure for storing guild history log data
struct GuildLogData
{
	std::string m_Text {}; // The log message
	std::string m_Time {}; // The time when the log was added
};

// Enum representing different flags for guild activity logging
enum GuildActivityLogFlags
{
	LOGFLAG_MEMBERS_CHANGES = 1 << 0,               // Flag for logging member changes
	LOGFLAG_HOUSE_MAIN_CHANGES = 1 << 1,            // Flag for logging house main changes
	LOGFLAG_HOUSE_DOORS_CHANGES = 1 << 2,           // Flag for logging house doors changes
	LOGFLAG_HOUSE_DECORATIONS_CHANGES = 1 << 3,     // Flag for logging house decorations changes
	LOGFLAG_UPGRADES_CHANGES = 1 << 4,              // Flag for logging upgrades changes
	LOGFLAG_RANKS_CHANGES = 1 << 5,                 // Flag for logging ranks changes
	LOGFLAG_BANK_CHANGES = 1 << 6,                  // Flag for logging bank changes
	LOGFLAG_GUILD_MAIN_CHANGES = 1 << 7,            // Flag for logging main guild information

	// Flag for logging all guild activities
	LOGFLAG_GUILD_FULL = LOGFLAG_MEMBERS_CHANGES | LOGFLAG_HOUSE_MAIN_CHANGES | LOGFLAG_HOUSE_DOORS_CHANGES | LOGFLAG_HOUSE_DECORATIONS_CHANGES
																		| LOGFLAG_UPGRADES_CHANGES | LOGFLAG_RANKS_CHANGES | LOGFLAG_BANK_CHANGES | LOGFLAG_GUILD_MAIN_CHANGES,
};

// Forward declaration and alias
class CGuildData;
using GuildLogContainer = std::deque<GuildLogData>;

// Define the CGuildLoggerManager class
class CGuildLoggerManager
{
	CGuildData* m_pGuild {};
	int64_t m_Logflag {};
	GuildLogContainer m_aLogs{};

public:
	// Constructor's
	CGuildLoggerManager() = delete;
	CGuildLoggerManager(CGuildData* pGuild, int64_t Logflag);

	void SetActivityFlag(int64_t Flag);
	bool IsActivityFlagSet(int64_t Flag) const;

	// Get the guild history logs
	const GuildLogContainer& GetContainer() const { return m_aLogs; };

	// Add a log message to the guild history
	void Add(int64_t CheckFlag, const char* pBuffer, ...);

private:
	void InitLogs();
};

#endif
