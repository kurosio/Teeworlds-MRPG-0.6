#ifndef GAME_SERVER_CORE_SCENARIOS_BASE_SCENARIO_BASE_WORLD_H
#define GAME_SERVER_CORE_SCENARIOS_BASE_SCENARIO_BASE_WORLD_H

#include "scenario_base_group.h"

class WorldScenarioBase : public GroupScenarioBase
{
	friend class CScenarioWorldManager;

protected:
	int m_WorldID {};

public:
	explicit WorldScenarioBase(int Flags = FLAG_NONE) : GroupScenarioBase(Flags) { }

	int GetWorldID() const { return m_WorldID; }
	bool AddParticipant(int ClientID) override;
};

#endif
