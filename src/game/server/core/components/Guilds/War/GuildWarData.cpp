/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildWarData.h"

#include <game/server/core/components/Guilds/GuildData.h>

CGuildWarData::CGuildWarData(CGuildData* pGuild, CGuildData* pTargetGuild, int Score)
{
	m_pGuild = pGuild;
	m_pGuild->m_pWar = this;
	m_pTargetGuild = pTargetGuild;
	m_Score = Score;
}

CGuildWarData::~CGuildWarData()
{
	m_pGuild->m_pWar = nullptr;
}

CGuildWarHandler::~CGuildWarHandler()
{
	delete m_pWarData.first;
	delete m_pWarData.second;
}

void CGuildWarData::AddScore(int Score)
{
	m_Score += Score;
	m_pWarHandler->Save();
}

void CGuildWarHandler::Tick()
{
	// check end state
	if(time(nullptr) >= m_TimeUntilEnd)
	{
		End();
		return;
	}

	dbg_msg("test", "%llu - %llu", time(nullptr), m_TimeUntilEnd);

}

void CGuildWarHandler::End()
{
	auto iter = std::find(m_pData.begin(), m_pData.end(), this);
	if(iter != m_pData.end())
	{
		m_pData.erase(iter);
		Database->Execute<DB::REMOVE>(TW_GUILDS_WARS_TABLE, "WHERE GuildID1 = '%d' AND GuildID2 = '%d'", m_pWarData.first->GetGuild()->GetID(), m_pWarData.second->GetGuild()->GetID());
	}

	// here reward count
	delete this;
	return;
}

void CGuildWarHandler::Save() const
{
	Database->Execute<DB::UPDATE>(TW_GUILDS_WARS_TABLE, "%llu = '%d', Score1 = '%d', Score2 = '%d' WHERE ID = '%d'", 
		m_TimeUntilEnd, m_pWarData.first->GetScore(), m_pWarData.second->GetScore());
}
