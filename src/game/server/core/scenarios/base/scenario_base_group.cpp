#include "scenario_base_group.h"

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <ranges>

// GroupScenarioBase
bool GroupScenarioBase::HasPlayer(CPlayer* pPlayer) const
{
	return pPlayer && m_vParticipantIDs.contains(pPlayer->GetCID());
}

std::vector<CPlayer*> GroupScenarioBase::GetPlayers() const
{
	auto view = m_vParticipantIDs
				| std::views::transform([this](int CID) { return GS()->GetPlayer(CID); })
				| std::views::filter([](CPlayer* pPtr) { return pPtr != nullptr; });
	return { view.begin(), view.end() };
}

bool GroupScenarioBase::OnPauseConditions()
{
	return std::ranges::all_of(m_vParticipantIDs, [this](int id){ return !GS()->GetPlayerChar(id); });
}

bool GroupScenarioBase::OnStopConditions()
{
	return m_vParticipantIDs.empty();
}

void GroupScenarioBase::OnScenarioEnd()
{
	std::vector<int> participants(m_vParticipantIDs.begin(), m_vParticipantIDs.end());
	for(int id : participants)
		RemoveParticipant(id);
}
bool GroupScenarioBase::AddParticipant(int ClientID)
{
	if(m_vParticipantIDs.contains(ClientID))
		return false;

	auto [it, inserted] = m_vParticipantIDs.insert(ClientID);
	if(inserted)
		OnPlayerJoin(ClientID);

	return inserted;
}
bool GroupScenarioBase::RemoveParticipant(int ClientID)
{
	if(m_vParticipantIDs.erase(ClientID) > 0)
	{
		OnPlayerLeave(ClientID, !IsRunning());
		return true;
	}

	return false;
}
