/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_RANK_DATA_H

class CGuildData;
using GuildRankIdentifier = int;
using GuildRankContainer = std::deque<class CGuildRankData*>;

class CGuildRankData
{
	GuildRankIdentifier m_ID {};
	std::string m_Rank{};
	int m_Access{};
	CGuildData* m_pGuild{};

public:
	CGuildRankData() = delete;
	CGuildRankData(GuildRankIdentifier RID, std::string Rank, int Access, CGuildData* pGuild);

	/*void ChangeAccess(int Access)
	{
		m_Access = Access;
	}*/

	GuildRankIdentifier GetID() const { return m_ID; }
	const char* GetAccessName();

	void ChangeName(std::string NewRank);
	void ChangeAccess(int Access);


};

#endif
