#include "dungeon_data.h"

#include <game/server/gamecontext.h>

CGS* CDungeonData::GS() const
{
	return (CGS*)Instance::GameServer(m_WorldID);
}

int CDungeonData::GetLevel() const
{
	return Instance::Server()->GetWorldDetail(m_WorldID)->GetRequiredLevel();
}

const char* CDungeonData::GetName() const
{
	return Instance::Server()->GetWorldName(m_WorldID);
}
