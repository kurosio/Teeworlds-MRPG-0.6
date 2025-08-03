#ifndef MMOTEEWORLDS_COMPONENT_H
#define MMOTEEWORLDS_COMPONENT_H

#include "step_component.h"

class CGS;

// universal component
template<typename TBase, typename T>
class Component : public IStepComponent
{
    TBase* m_pScenario {};
    int m_StartDelayTick {};

protected:
    bool m_bIsFinished {};
    std::optional<StepId> m_NextStepId {};
    int m_DelayTick {};

    CGS* GS() const { return m_pScenario->GS(); }
    IServer* Server() const { return m_pScenario->Server(); }
    TBase* Scenario() const { return m_pScenario; }

    virtual void OnStartImpl() { }
    virtual void OnActiveImpl() { }
    virtual void OnEndImpl() { }
    void Finish() { m_bIsFinished = true; }

public:
    Component() = default;
    explicit Component(const nlohmann::json& j) { }

    void InitBaseJsonField(const nlohmann::json& j)
    {
        if(j.contains("delay"))
        {
            m_StartDelayTick = j.value("delay", 0);
            m_DelayTick = m_StartDelayTick;
        }
        if(j.contains("next_step_id"))
            m_NextStepId = j.value("next_step_id", "");
    }

    void Init(TBase* pScenario)
    {
        m_pScenario = pScenario;
    }

    void OnStart() final
    {
        m_DelayTick = m_StartDelayTick;
        m_bIsFinished = false;
        OnStartImpl();
    }

    void OnActive() final
    {
        if(m_DelayTick)
            m_DelayTick--;
        if(!IsFinished())
            OnActiveImpl();
    }

    void OnEnd() final
    {
        OnEndImpl();
    }

    bool IsFinished() const noexcept override { return m_DelayTick <= 0 && m_bIsFinished; }
    std::optional<StepId> GetNextStepId() const override { return IsFinished() ? m_NextStepId : std::nullopt; }
};

#endif // MMOTEEWORLDS_COMPONENT_H
