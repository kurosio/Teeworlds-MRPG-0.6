#include "scenario_base_player.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

bool PlayerScenarioBase::OnPauseConditions()
{
	const auto* p = GetPlayer();
	return !p || !p->GetCharacter();
}

bool PlayerScenarioBase::OnStopConditions()
{
	return !GetPlayer();
}

CPlayer* PlayerScenarioBase::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}
