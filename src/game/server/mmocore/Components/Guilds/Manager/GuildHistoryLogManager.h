/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HISTORY_LOG_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_HISTORY_LOG_MANAGER_H

struct GuildHistoryLogData
{
	std::string m_Log {};
	std::string m_Time {};
};

class CGuildData;
using GuildHistoryContainer = std::vector< class GuildHistoryLogData >;

class CGuildHistoryController
{
	CGuildData* m_pGuild {};

public:
	CGuildHistoryController() = delete;
	CGuildHistoryController(CGuildData* pGuild) : m_pGuild(pGuild) {}

	GuildHistoryContainer&& GetLogs() const;
	void Add(const char* pBuffer, ...) const;
};

#endif
