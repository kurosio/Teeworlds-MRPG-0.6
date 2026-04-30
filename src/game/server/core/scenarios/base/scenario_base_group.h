#ifndef GAME_SERVER_CORE_SCENARIOS_BASE_SCENARIO_BASE_GROUP_H
#define GAME_SERVER_CORE_SCENARIOS_BASE_SCENARIO_BASE_GROUP_H

#include "scenario_base.h"

class GroupScenarioBase : public ScenarioBase
{
	friend class CScenarioGroupManager;

protected:
	std::set<int> m_vParticipantIDs {};
	int m_GroupLives { 0 };

	bool OnPauseConditions() override;
	bool OnStopConditions() override;
	void OnScenarioEnd() override;
	virtual void OnPlayerJoin(int ClientID) { }
	virtual void OnPlayerLeave(int ClientID, bool scenarioEnding) { }

public:
	explicit GroupScenarioBase(int Flags = FLAG_NONE) : ScenarioBase(Flags) { }

	bool HasPlayer(CPlayer* pPlayer) const;
	std::vector<CPlayer*> GetPlayers() const;

	virtual bool AddParticipant(int ClientID);
	virtual bool RemoveParticipant(int ClientID);
	std::set<int> GetParticipants() const { return m_vParticipantIDs; }

	int GetGroupLives() const { return m_GroupLives; }
	void SetGroupLives(int Lives) { m_GroupLives = Lives > 0 ? Lives : 0; }
	void AddGroupLives(int Lives) { m_GroupLives = (m_GroupLives + Lives) > 0 ? (m_GroupLives + Lives) : 0; }
	bool ConsumeGroupLife();
};

#endif
