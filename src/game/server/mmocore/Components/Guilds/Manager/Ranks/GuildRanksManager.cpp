/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRanksManager.h"

#include <game/server/gamecontext.h>
#include "GuildRankData.h"

#include <game/server/mmocore/Components/Guilds/GuildData.h>

CGS* CGuildRanksController::GS() const
{
	return m_pGuild->GS();
}

CGuildRanksController::CGuildRanksController(CGuildData* pGuild)
	: m_pGuild(pGuild)
{
	Init();
}

CGuildRanksController::~CGuildRanksController()
{
	for(auto& p : m_aRanks)
		delete p;
	m_aRanks.clear();
}

void CGuildRanksController::Init()
{
	// rank loading
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		int Access = pRes->getInt("Access");

		m_aRanks.emplace_back(new CGuildRankData(RID, std::move(Rank), Access, m_pGuild));
	}
}

bool CGuildRanksController::Add(std::string Rank)
{
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank) { return std::string(pRank->GetName()) == Rank; }))
		return false;

	GuildIdentifier GuildID = m_pGuild->GetID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "WHERE GuildID = '%d'", GuildID);
	if(pRes->rowsCount() < 5)
		return false;

	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // thread save ? hm need for table all time auto increment = 1; NEED FIX IT

	CSqlString<64> cGuildRank = CSqlString<64>(Rank.c_str());
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, GuildID, Name) VALUES ('%d', '%d', '%s')", InitID, GuildID, cGuildRank.cstr());
	GS()->ChatGuild(GuildID, "Creates new rank [{STR}]!", Rank);

	m_aRanks.emplace_back(new CGuildRankData(InitID, std::move(Rank), ACCESS_NO, m_pGuild));
	return true;
}

bool CGuildRanksController::Delete(std::string Rank)
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank){ return pRank->GetName() == Rank; });
	if(Iter != m_aRanks.end())
	{
		GuildIdentifier GuildID = m_pGuild->GetID();
		Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildRank = NULL WHERE GuildRank = '%d' AND GuildID = '%d'", (*Iter)->GetID(), GuildID);
		Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '%d' AND GuildID = '%d'", (*Iter)->GetID(), GuildID);

		GS()->ChatGuild(GuildID, "Rank [{STR}] succesful delete", Rank.c_str());
		GetContainer().erase(Iter);
		return true;
	}

	return false;
}

CGuildRankData* CGuildRanksController::Get(std::string Rank) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank){ return pRank->GetName() == Rank; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}
