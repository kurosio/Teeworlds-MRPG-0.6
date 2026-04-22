#include "scenario_base_world.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

bool WorldScenarioBase::AddParticipant(int ClientID)
{
	auto* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || pPlayer->GetCurrentWorldID() != m_WorldID)
		return false;

	return GroupScenarioBase::AddParticipant(ClientID);
}
