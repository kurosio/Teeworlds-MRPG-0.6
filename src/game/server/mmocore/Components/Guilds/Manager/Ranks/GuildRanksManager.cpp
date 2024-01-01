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

CGuildRanksController::CGuildRanksController(CGuildData* pGuild, GuildRankIdentifier DefaultID)
	: m_pGuild(pGuild)
{
	Init(DefaultID);
}

CGuildRanksController::~CGuildRanksController()
{
	m_pDefaultRank = nullptr;
	for(auto& p : m_aRanks)
		delete p;
	m_aRanks.clear();
}

void CGuildRanksController::Init(GuildRankIdentifier DefaultID)
{
	// rank loading
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		int Access = pRes->getInt("Access");
		if(DefaultID == RID)
		{
			m_pDefaultRank = m_aRanks.emplace_back(new CGuildRankData(RID, std::forward<std::string>(Rank), Access, m_pGuild));
		}
		else
		{
			m_aRanks.emplace_back(new CGuildRankData(RID, std::forward<std::string>(Rank), Access, m_pGuild));
		}
	}

	InitDefaultRank();
}

void CGuildRanksController::InitDefaultRank()
{
	// initilize default rank
	if(!m_pDefaultRank)
	{
		if(!m_aRanks.empty())
		{
			m_pDefaultRank = m_aRanks.back();
		}
		else
		{
			Add("Member");
			m_pDefaultRank = Get("Member");
		}
	}
}

GUILD_RANK_RESULT CGuildRanksController::Add(std::string Rank)
{
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank) { return std::string(pRank->GetName()) == Rank; }))
	{
		return GUILD_RANK_RESULT::ADD_ALREADY_EXISTS;
	}

	if(m_aRanks.size() >= MAX_GUILD_RANK_NUM)
	{
		return GUILD_RANK_RESULT::ADD_LIMIT_HAS_REACHED;
	}

	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // thread save ? hm need for table all time auto increment = 1; NEED FIX IT

	GuildIdentifier GuildID = m_pGuild->GetID();
	CSqlString<64> cGuildRank = CSqlString<64>(Rank.c_str());
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, GuildID, Name) VALUES ('%d', '%d', '%s')", InitID, GuildID, cGuildRank.cstr());
	GS()->ChatGuild(GuildID, "Creates new rank [{STR}]!", Rank);

	m_aRanks.emplace_back(new CGuildRankData(InitID, std::move(Rank), RIGHTS_DEFAULT, m_pGuild));
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

GUILD_RANK_RESULT CGuildRanksController::Remove(std::string Rank)
{
	CGuildRankData* pRank = Get(Rank);
	if(pRank == m_pDefaultRank)
	{
		return GUILD_RANK_RESULT::REMOVE_RANK_IS_DEFAULT;
	}

	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank){ return pRank->GetName() == Rank; });
	if(Iter == m_aRanks.end())
	{
		return GUILD_RANK_RESULT::REMOVE_RANK_DOES_NOT_EXIST;
	}

	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildRank = NULL WHERE GuildRank = '%d' AND GuildID = '%d'", (*Iter)->GetID(), GuildID);
	Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '%d' AND GuildID = '%d'", (*Iter)->GetID(), GuildID);

	GS()->ChatGuild(GuildID, "Rank [{STR}] succesful delete", Rank.c_str());
	m_aRanks.erase(Iter);
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

CGuildRankData* CGuildRanksController::Get(std::string Rank) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank){ return pRank->GetName() == Rank; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}

CGuildRankData* CGuildRanksController::Get(GuildRankIdentifier ID) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&ID](const CGuildRankData* pRank){ return pRank->GetID() == ID; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}
