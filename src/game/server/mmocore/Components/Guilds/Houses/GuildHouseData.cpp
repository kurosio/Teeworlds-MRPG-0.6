/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseData.h"

#include "engine/server.h"
#include "game/server/mmocore/Components/Guilds/GuildData.h"

CGS* CGuildHouseData::GS() const { return (CGS*)Instance::GetServer()->GameServer(m_WorldID); }

CGuildHouseData::~CGuildHouseData()
{
	delete m_pDecorations;
	delete m_pDoors;
}

void CGuildHouseData::SetGuild(CGuildData* pGuild)
{
	m_pGuild = pGuild;
	m_pGuild->m_pHouse = this;
}
