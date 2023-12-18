/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

#include <game/server/gamecontext.h>

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
	delete m_pHistory;
	delete m_pRanks;
	delete m_pBank;
}

void CGuildData::AddExperience(int Experience)
{
}
