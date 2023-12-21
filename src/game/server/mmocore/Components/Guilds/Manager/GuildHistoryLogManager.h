/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HISTORY_LOG_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_HISTORY_LOG_MANAGER_H

// Define the structure for storing guild history log data
struct GuildHistoryLogData
{
	std::string m_Log {}; // The log message
	std::string m_Time {}; // The time when the log was added
};

// Forward declaration and alias
class CGuildData;
using GuildHistoryContainer = std::vector<GuildHistoryLogData>;

// Define the CGuildHistoryController class
class CGuildHistoryController
{
	CGuildData* m_pGuild {};

public:
	// Constructor's
	CGuildHistoryController() = delete;
	CGuildHistoryController(CGuildData* pGuild) : m_pGuild(pGuild) {}

	// Get the guild history logs
	GuildHistoryContainer&& GetLogs() const;

	// Add a log message to the guild history
	void Add(const char* pBuffer, ...) const;
};

#endif
