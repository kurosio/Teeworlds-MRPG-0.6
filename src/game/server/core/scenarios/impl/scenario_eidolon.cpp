#include "scenario_eidolon.h"

#include <game/server/gamecontext.h>

CPlayer* CEidolonScenario::GetOwner() const
{
	auto* pBot = dynamic_cast<CPlayerBot*>(GetPlayer());
	return pBot ? pBot->GetEidolonOwner() : nullptr;
}

bool CEidolonScenario::OnStopConditions()
{
	const auto* pBot = dynamic_cast<CPlayerBot*>(GetPlayer());
	return !pBot || !pBot->GetCharacter() || !pBot->GetEidolonOwner() || !pBot->GetEidolonOwner()->GetCharacter();
}

void CEidolonScenario::OnSetupScenario()
{
	/*const auto* pBot = dynamic_cast<CPlayerBot*>(GetPlayer());
	const auto* pOwner = GetOwner();
	CCharacter* pBotChar = pBot->GetCharacter();
	CCharacter* pOwnerChar = pOwner->GetCharacter();

	if(pOwner->GetHealth() < pOwner->GetMaxHealth() * 0.1)
	{

	}*/
}

void CEidolonScenario::SendRandomChatMessage(const std::vector<const char*>& messages) const
{
	if(messages.empty())
		return;

	//const int randIndex = rand() % messages.size();
	//GS()->SendChat(GetClientID(), CHAT_ALL, messages[randIndex]);
}
