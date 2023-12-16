/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

std::map < int, CGuildHouseData > CGuildHouseData::ms_aHouseGuild;
std::map < int, CGuildRankData > CGuildRankData::ms_aRankGuild;

CGuildData::~CGuildData()
{
	delete m_pBank;
}

void CGuildData::InitRanks()
{
	// rank loading
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_ID);
	while(pRes->next())
	{
		int ID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		int Access = pRes->getInt("Access");

		m_aRanks.push_back({ Rank, Access, this });
	}
}

CGuildRankData::CGuildRankData(std::string Rank, int Access, CGuildData* pGuild) : m_Rank(Rank), m_Access(Access)
{
	m_pGuild = pGuild;
}
