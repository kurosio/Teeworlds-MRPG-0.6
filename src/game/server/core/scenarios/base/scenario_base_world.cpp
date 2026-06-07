#include "scenario_base_world.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

std::vector<CPlayer*> WorldScenarioBase::GetPlayers() const
{
	auto view = m_vParticipantIDs
		| std::views::transform([this](int CID) { return GS()->GetPlayer(CID); })
		| std::views::filter([this](CPlayer* pPtr) { return pPtr && pPtr->GetCurrentWorldID() == m_WorldID; });
	return { view.begin(), view.end() };
}

bool WorldScenarioBase::AddParticipant(int ClientID)
{
	auto* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || pPlayer->GetCurrentWorldID() != m_WorldID)
		return false;

	return GroupScenarioBase::AddParticipant(ClientID);
}
