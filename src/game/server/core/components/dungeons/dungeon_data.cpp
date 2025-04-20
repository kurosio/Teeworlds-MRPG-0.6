#include "dungeon_data.h"

#include <game/server/gamecontext.h>

CGS* CDungeonData::GS() const
{
	return (CGS*)Instance::GameServer(m_WorldID);
}