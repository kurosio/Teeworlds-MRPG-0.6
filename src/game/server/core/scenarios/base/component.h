#ifndef GAME_SERVER_CORE_SCENARIOS_BASE_COMPONENT_H
#define GAME_SERVER_CORE_SCENARIOS_BASE_COMPONENT_H

#include <base/types.h>
#include <optional>
#include <string>

class ScenarioBase;
class CGS;
class IServer;

#define DECLARE_COMPONENT_NAME(str) \
    static constexpr std::string_view Name = str;

class IStepComponent
{
public:
	virtual ~IStepComponent() = default;
	virtual void Init(ScenarioBase* pScenario) { }
	virtual void OnStart() { }
	virtual void OnActive() { }
	virtual void OnEnd() { }
	virtual bool IsFinished() const noexcept { return true; }
	virtual std::optional<StepId> GetNextStepId() const { return std::nullopt; }
};

template<typename TBase, typename T>
class Component : public IStepComponent
{
	int m_StartDelayTick {};

protected:
	TBase* m_pScenario {};
	bool m_bIsFinished {};
	std::optional<StepId> m_NextStepId {};
	int m_DelayTick {};

	CGS* GS() const { return m_pScenario ? m_pScenario->GS() : nullptr; }
	IServer* Server() const { return m_pScenario ? m_pScenario->Server() : nullptr; }
	TBase* Scenario() const { return m_pScenario; }

	virtual void OnStartImpl() {}
	virtual void OnActiveImpl() {}
	virtual void OnEndImpl() {}
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

	void Init(ScenarioBase* pScenario) override
	{
		m_pScenario = dynamic_cast<TBase*>(pScenario);
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

	bool IsFinished() const noexcept override
	{
		return m_DelayTick <= 0 && m_bIsFinished;
	}

	std::optional<StepId> GetNextStepId() const override
	{
		return IsFinished() ? m_NextStepId : std::nullopt;
	}
};

#endif