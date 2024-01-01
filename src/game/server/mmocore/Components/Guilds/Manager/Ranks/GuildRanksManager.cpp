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
		GuildRankAccess Access = (GuildRankAccess)pRes->getInt("Access");

		if(DefaultID == RID)
		{
			m_pDefaultRank = m_aRanks.emplace_back(new CGuildRankData(RID, std::forward<std::string>(Rank), RIGHTS_DEFAULT, m_pGuild));
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
			m_pDefaultRank->SetAccess(RIGHTS_DEFAULT);
		}
		else
		{
			Add("Member");
			m_pDefaultRank = Get("Member");
		}

		Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "DefaultRankID = '%d' WHERE ID = '%d'", m_pDefaultRank->GetID(), m_pGuild->GetID());
	}
}

GUILD_RANK_RESULT CGuildRanksController::Add(std::string Rank)
{
	auto cstrRank = CSqlString<64>(Rank.c_str());

	// check if already exist
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CGuildRankData* pRank) { return std::string(pRank->GetName()) == cstrRank.cstr(); }))
	{
		return GUILD_RANK_RESULT::ADD_ALREADY_EXISTS;
	}

	// check limit rank count
	if((int)m_aRanks.size() >= MAX_GUILD_RANK_NUM)
	{
		return GUILD_RANK_RESULT::ADD_LIMIT_HAS_REACHED;
	}

	// insert new rank
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // thread save ? hm need for table all time auto increment = 1; NEED FIX IT

	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, Access, GuildID, Name) VALUES ('%d', '%d', '%d', '%s')", InitID, (int)RIGHTS_DEFAULT, GuildID, cstrRank.cstr());
	m_aRanks.emplace_back(new CGuildRankData(InitID, cstrRank.cstr(), RIGHTS_DEFAULT, m_pGuild));

	// information
	GS()->ChatGuild(GuildID, "New rank is created [{STR}]!", cstrRank.cstr());
	m_pGuild->GetHistory()->Add("added rank '%s'", cstrRank.cstr());
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

GUILD_RANK_RESULT CGuildRanksController::Remove(std::string Rank)
{
	auto cstrRank = CSqlString<64>(Rank.c_str());
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CGuildRankData* pRank){ return str_comp(pRank->GetName(), cstrRank.cstr()) == 0; });

	// can't remove default rank
	if(*Iter == m_pDefaultRank)
	{
		return GUILD_RANK_RESULT::REMOVE_RANK_IS_DEFAULT;
	}

	// rank does not exist
	if(Iter == m_aRanks.end())
	{
		return GUILD_RANK_RESULT::REMOVE_RANK_DOES_NOT_EXIST;
	}

	// update member rank's
	for(auto& pMember : m_pGuild->GetMembers()->GetContainer())
	{
		if((*Iter)->GetID() == pMember.second->GetRank()->GetID())
			pMember.second->SetRank(m_pDefaultRank->GetID());
	}

	// remove guild rank
	Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '%d'", (*Iter)->GetID());
	delete (*Iter);
	m_aRanks.erase(Iter);

	// information
	GS()->ChatGuild(m_pGuild->GetID(), "Rank [{STR}] succesful delete", cstrRank.cstr());
	m_pGuild->GetHistory()->Add("removed rank '%s'", cstrRank.cstr());
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
