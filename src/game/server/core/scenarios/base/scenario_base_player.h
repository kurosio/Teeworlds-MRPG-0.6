#ifndef MMOTEEWORLDS_SCENARIO_BASE_PLAYER_H
#define MMOTEEWORLDS_SCENARIO_BASE_PLAYER_H

#include "scenario_base.h"

class PlayerScenarioBase : public ScenarioBase
{
    friend class CScenarioPlayerManager;

protected:
    int m_ClientID {};
    bool OnPauseConditions() override;
    bool OnStopConditions() override;

public:
    explicit PlayerScenarioBase(int Flags = FLAG_NONE) : ScenarioBase(Flags) { }
    CPlayer* GetPlayer() const;
};

#endif // MMOTEEWORLDS_SCENARIO_BASE_PLAYER_H
