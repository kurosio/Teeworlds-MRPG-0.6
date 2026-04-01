/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_war_data.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/guilds/guild_data.h>
#include <game/server/core/components/houses/guild_house_data.h>
#include <components/houses/entities/house_door.h>

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
	const time_t CurrentTime = time(nullptr);
	time_t TimeLeft = 0;
	const char* pState = "Finished";

	switch(m_Stage)
	{
		case Stage::Preparation:
			TimeLeft = m_TimeUntilPreparationEnd - CurrentTime;
			pState = "Preparation";
			break;
		case Stage::Siege:
			TimeLeft = m_TimeUntilSiegeEnd - CurrentTime;
			pState = "Siege";
			break;
		case Stage::Cooldown:
			TimeLeft = m_TimeUntilCooldownEnd - CurrentTime;
			pState = "Cooldown";
			break;
		default:
			break;
	}

	TimeLeft = maximum<time_t>(0, TimeLeft);
	str_format(pBuf, Size, "%s: %02dh %02dm", pState, (int)TimeLeft / 3600, (int)(TimeLeft / 60) % 60);
}

void CGuildWarHandler::Init(const CGuildWarData& WarData1, const CGuildWarData& WarData2, time_t TimeUntilPreparationEnd, time_t TimeUntilSiegeEnd, time_t TimeUntilCooldownEnd)
{
	m_pWarData = { new CGuildWarData(WarData1), new CGuildWarData(WarData2) };
	m_pWarData.first->m_pWarHandler = this;
	m_pWarData.second->m_pWarHandler = this;
	m_pWarData.first->m_pGuild->m_pWar = m_pWarData.first;
	m_pWarData.second->m_pGuild->m_pWar = m_pWarData.second;
	m_TimeUntilPreparationEnd = TimeUntilPreparationEnd;
	m_TimeUntilSiegeEnd = TimeUntilSiegeEnd;
	m_TimeUntilCooldownEnd = TimeUntilCooldownEnd;
	m_Stage = Stage::Preparation;
	m_CoreUnlocked = false;
	m_DefenseLost = false;

	Database->Execute<DB::INSERT>(TW_GUILDS_WARS_TABLE, "(TimeUntilEnd, GuildID1, GuildID2) VALUES ('{}', '{}', '{}')",
		m_TimeUntilCooldownEnd, m_pWarData.first->m_pGuild->GetID(), m_pWarData.second->m_pGuild->GetID());
	dbg_msg("test", "creating war handler");
}

bool CGuildWarHandler::CanDamageDoors(const CGuild* pAttacker, const CGuild* pDefender) const
{
	if(!pAttacker || !pDefender || !IsSiegeActive() || m_DefenseLost)
		return false;

	return (m_pWarData.first->GetGuild() == pAttacker && m_pWarData.second->GetGuild() == pDefender) ||
		(m_pWarData.second->GetGuild() == pAttacker && m_pWarData.first->GetGuild() == pDefender);
}

void CGuildWarHandler::OnRequiredDoorDestroyed(const CGuild* pDefender)
{
	if(!IsSiegeActive() || m_CoreUnlocked || !pDefender || !pDefender->GetHouse())
		return;

	const auto& Doors = pDefender->GetHouse()->GetDoorManager()->GetContainer();
	const bool AllDestroyed = std::all_of(Doors.begin(), Doors.end(), [](const auto& pDoor)
	{
		return pDoor.second->IsDestroyed();
	});

	if(AllDestroyed)
	{
		auto* pGS = (CGS*)Instance::GameServerPlayer(INITIALIZER_WORLD_ID);
		m_CoreUnlocked = true;
		pGS->ChatGuild(m_pWarData.first->GetGuild()->GetID(), "All required doors are destroyed. Core/relic access is now open.");
		pGS->ChatGuild(m_pWarData.second->GetGuild()->GetID(), "All required doors are destroyed. Core/relic access is now open.");
		Save();
	}
}

void CGuildWarHandler::MarkDefenseLost(const CGuild* pDefender, const char* pReason)
{
	if(!IsSiegeActive() || m_DefenseLost || !pDefender)
		return;

	m_DefenseLost = true;
	m_Stage = Stage::Cooldown;
	auto* pGS = (CGS*)Instance::GameServerPlayer(INITIALIZER_WORLD_ID);
	const auto pAttackerGuild = m_pWarData.first->GetGuild() == pDefender ? m_pWarData.second->GetGuild() : m_pWarData.first->GetGuild();
	pGS->ChatGuild(pAttackerGuild->GetID(), "Defender lost siege: {}.", pReason);
	pGS->ChatGuild(pDefender->GetID(), "Your defense is lost: {}.", pReason);
	Save();
}

void CGuildWarHandler::Handle()
{
	const time_t CurrentTime = time(nullptr);
	if(m_Stage == Stage::Preparation && CurrentTime >= m_TimeUntilPreparationEnd)
	{
		auto* pGS = (CGS*)Instance::GameServerPlayer(INITIALIZER_WORLD_ID);
		m_Stage = Stage::Siege;
		pGS->ChatGuild(m_pWarData.first->GetGuild()->GetID(), "Siege window is now open. Attackers can damage doors.");
		pGS->ChatGuild(m_pWarData.second->GetGuild()->GetID(), "Siege window is now open. Attackers can damage doors.");
		Save();
	}

	if(m_Stage == Stage::Siege && (CurrentTime >= m_TimeUntilSiegeEnd))
	{
		auto* pGS = (CGS*)Instance::GameServerPlayer(INITIALIZER_WORLD_ID);
		m_Stage = Stage::Cooldown;
		pGS->ChatGuild(m_pWarData.first->GetGuild()->GetID(), "Siege is over. House entered protection cooldown.");
		pGS->ChatGuild(m_pWarData.second->GetGuild()->GetID(), "Siege is over. House entered protection cooldown.");
		Save();
	}

	if(m_Stage == Stage::Cooldown && CurrentTime >= m_TimeUntilCooldownEnd)
	{
		End();
		return;
	}
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
		m_TimeUntilCooldownEnd, m_pWarData.first->GetScore(), m_pWarData.second->GetScore(), m_pWarData.first->GetGuild()->GetID(), m_pWarData.second->GetGuild()->GetID());
}
