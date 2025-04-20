#include "dungeon_data.h"

#include <game/server/gamecontext.h>

CGS* CDungeonData::GS() const
{
	return (CGS*)Instance::GameServer(m_WorldID);
}

const char* CDungeonData::GetName() const
{
	return Instance::Server()->GetWorldName(m_WorldID);
}
