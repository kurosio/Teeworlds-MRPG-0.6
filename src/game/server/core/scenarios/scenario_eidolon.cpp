#include "scenario_eidolon.h"

#include <game/server/gamecontext.h>

bool CEidolonScenario::OnStopConditions()
{
	const auto* pBot = dynamic_cast<CPlayerBot*>(GetPlayer());
	return !pBot || !pBot->GetCharacter() || pBot->GetBotType() != TYPE_BOT_EIDOLON;
}

void CEidolonScenario::OnSetupScenario()
{
	auto& step = AddStep(100);
	step.WhenFinished([this](auto*)
	{
		//GS()->SendChat(GetClientID(), CHAT_ALL, "Hmmm...");
	});

	// Setup the scenario
}