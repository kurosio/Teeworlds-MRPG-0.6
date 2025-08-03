#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_BASE_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_BASE_H

#include <base/types.h>
#include "component.h"

#include <vector>
#include <memory>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <map>
#include <optional>

class CGS;
class IServer;
class CPlayer;
class CCharacter;

class ScenarioBase
{
	friend class CScenarioPlayerManager;
	friend class CScenarioGroupManager;
	CGS* m_pGS {};

protected:
	struct Step
	{
		StepId m_ID {};
		std::vector<std::unique_ptr<IStepComponent>> m_vComponents {};
		ConditionPriority m_Priority { ConditionPriority::CONDITION_AND_TIMER };
		StepCompletionLogic m_CompletionLogic { StepCompletionLogic::ALL_OF };
		std::string m_MsgInfo {};
		int m_DelayTick {};
		size_t m_CurrentComponentIndex { 0 };

		explicit Step(StepId ID, std::string msgInfo, int delayTick) noexcept
		{
			m_ID = std::move(ID);
			m_DelayTick = delayTick;
			m_MsgInfo = msgInfo;
		}

		void AddComponent(std::unique_ptr<IStepComponent> pComponent)
		{
			if(pComponent)
				m_vComponents.push_back(std::move(pComponent));
		}
	};

	using ComponentsList = std::map<std::string, std::function<std::unique_ptr<IStepComponent>(const nlohmann::json&)>, std::less<>>;
	ComponentsList m_ComponentsRegistered {};
	int m_ScenarioID {};
	std::map<StepId, Step, std::less<>> m_mSteps {};
	std::vector<StepId> m_vStepOrder {};
	StepId m_StartStepId {};
	StepId m_CurrentStepId {};
	int m_LastStepTimeTick {};
	bool m_Running {};
	int m_Flags {};

	virtual void OnSetupScenario() { }
	virtual void OnScenarioStart() { }
	virtual void OnScenarioEnd() { }
	virtual bool OnPauseConditions() { return false; }
	virtual bool OnStopConditions() { return false; }

	void AdvanceStep();
	bool CanConcludeCurrentStep() const;
	void ExecuteStepStartActions();
	void ExecuteStepActiveActions();
	void ExecuteStepEndActions();
	void TryAdvanceSequentialComponent();

	[[nodiscard]] Step& AddStep(StepId id, std::string MsgInfo = "", int delayTick = -1);

private:
	void Start();
	void Tick();
	void Stop();
	bool IsFinished() const noexcept { return m_CurrentStepId == END_SCENARIO_STEP_ID; }

public:
	CGS* GS() const;
	IServer* Server() const;

	enum Flags { FLAG_NONE = 0, FLAG_REPEATABLE = 1 << 0 };
	explicit ScenarioBase(int Flags = FLAG_NONE) noexcept : m_Flags(Flags) { }
	virtual ~ScenarioBase();

	template<typename TBase, typename T>
	void RegisterComponent(std::string_view name)
	{
		static_assert(std::is_base_of_v<ScenarioBase, TBase>, "TBase must derive from ScenarioBase");
		static_assert(std::is_base_of_v<IStepComponent, T>, "T must derive from IStepComponent");

		m_ComponentsRegistered.emplace(name, [this](const nlohmann::json& j) -> std::unique_ptr<IStepComponent>
		{
			auto pPtr = std::make_unique<T>(j);
			if(auto* pScenario = dynamic_cast<TBase*>(this))
				pPtr->Init(pScenario);

			return pPtr;
		});
	}

	ComponentsList& GetComponents() { return m_ComponentsRegistered; }
	bool IsRunning() const noexcept { return m_Running; }
};

#endif