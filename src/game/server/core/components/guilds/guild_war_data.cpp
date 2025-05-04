/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_war_data.h"

#include <game/server/core/components/guilds/guild_data.h>

CGuildWarData::CGuildWarData(CGuild* pGuild, CGuild* pTargetGuild, int Score)
{
	m_pGuild = pGuild;
	m_pTargetGuild = pTargetGuild;
	m_Score = Score;
}

void CGuildWarData::AddScore(int Score)
{
	m_Score += Score;

	if(m_pWarHandler)
		m_pWarHandler->Save();
}

CGuildWarHandler::~CGuildWarHandler()
{
	if(m_pWarData.first->m_pGuild)
		m_pWarData.first->m_pGuild->m_pWar = nullptr;
	if(m_pWarData.second->m_pGuild)
		m_pWarData.second->m_pGuild->m_pWar = nullptr;

	delete m_pWarData.first;
	delete m_pWarData.second;

	dbg_msg("test", "deleting war handler");
}

void CGuildWarHandler::FormatTimeLeft(char* pBuf, int Size) const
{
	time_t TimeLeft = m_TimeUntilEnd - time(nullptr);
	str_format(pBuf, Size, "%02dh %02dm", (int)TimeLeft / 3600, (int)(TimeLeft / 60) % 60);
}

void CGuildWarHandler::Init(const CGuildWarData& WarData1, const CGuildWarData& WarData2, time_t TimeUntilEnd)
{
	m_pWarData = { new CGuildWarData(WarData1), new CGuildWarData(WarData2) };
	m_pWarData.first->m_pWarHandler = this;
	m_pWarData.second->m_pWarHandler = this;
	m_pWarData.first->m_pGuild->m_pWar = m_pWarData.first;
	m_pWarData.second->m_pGuild->m_pWar = m_pWarData.second;
	m_TimeUntilEnd = TimeUntilEnd;

	Database->Execute<DB::INSERT>(TW_GUILDS_WARS_TABLE, "(TimeUntilEnd, GuildID1, GuildID2) VALUES ('{}', '{}', '{}')",
		m_TimeUntilEnd, m_pWarData.first->m_pGuild->GetID(), m_pWarData.second->m_pGuild->GetID());
	dbg_msg("test", "creating war handler");
}

void CGuildWarHandler::Handle()
{
	// check end state
	if(time(nullptr) >= m_TimeUntilEnd)
	{
		End();
		return;
	}

	//dbg_msg("test", "%ld - %ld", time(nullptr), m_TimeUntilEnd);
}

void CGuildWarHandler::End()
{
	auto iter = std::find(m_pData.begin(), m_pData.end(), this);
	if(iter != m_pData.end())
	{
		m_pData.erase(iter);
		Database->Execute<DB::REMOVE>(TW_GUILDS_WARS_TABLE, "WHERE GuildID1 = '{}' AND GuildID2 = '{}'", m_pWarData.first->GetGuild()->GetID(), m_pWarData.second->GetGuild()->GetID());
	}

	// here reward count
	dbg_msg("test", "ending war handler");
	delete this;
	return;
}

void CGuildWarHandler::Save() const
{
	Database->Execute<DB::UPDATE>(TW_GUILDS_WARS_TABLE, "TimeUntilEnd = '{}', Score1 = '{}', Score2 = '{}' WHERE GuildID1 = '{}' AND GuildID2 = '{}'",
		m_TimeUntilEnd, m_pWarData.first->GetScore(), m_pWarData.second->GetScore(), m_pWarData.first->GetGuild()->GetID(), m_pWarData.second->GetGuild()->GetID());
}
