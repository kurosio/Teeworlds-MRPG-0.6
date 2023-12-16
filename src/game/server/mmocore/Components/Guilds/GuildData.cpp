/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

#include "engine/server.h"

std::map < int, CGuildHouseData > CGuildHouseData::ms_aHouseGuild;

CGS* CGuildData::GS() const
{
	if(/*does not house*/ true)
	{
		return (CGS*)Instance::GetServer()->GameServer();
	}

	return nullptr;
}

CGuildData::~CGuildData()
{
	for(auto& pRank : m_aRanks)
		delete pRank;

	delete m_pBank;
}

void CGuildData::InitRanks()
{
	// rank loading
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_ID);
	while(pRes->next())
	{
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		int Access = pRes->getInt("Access");

		m_aRanks.emplace_back(new CGuildRankData(RID, std::move(Rank), Access, this));
	}
}