#ifndef MMOTEEWORLDS_SCENARIO_BASE_GROUP_H
#define MMOTEEWORLDS_SCENARIO_BASE_GROUP_H

#include "scenario_base.h"

class GroupScenarioBase : public ScenarioBase
{
    friend class CScenarioGroupManager;

protected:
    std::set<int> m_vParticipantIDs {};

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
    std::set<int>& GetParticipants() { return m_vParticipantIDs; }
};

#endif // MMOTEEWORLDS_SCENARIO_BASE_GROUP_H
