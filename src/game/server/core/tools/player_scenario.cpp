#include <game/server/gamecontext.h>
#include "player_scenario.h"

bool PlayerScenario::CanExecuteStep(const Step& step, long elapsedMs) const
{
	switch(step.Priority)
	{
		case ConditionPriority::CONDITION_AND_TIMER:
			return (step.DelayMs <= 0 || (elapsedMs >= step.DelayMs)) &&
				(!step.FuncCheckCondition || step.FuncCheckCondition(this));

		case ConditionPriority::CONDITION_OR_TIMER:
			return (step.DelayMs >= 0 && elapsedMs >= step.DelayMs) ||
				(step.FuncCheckCondition && step.FuncCheckCondition(this));
	}

	return false;
}

void PlayerScenario::ExecuteCurrentStep()
{
	if(auto& step = m_vSteps[m_CurrentStepIndex]; step.FuncOnEnd)
	{
		step.FuncOnEnd(this);
		step.FuncOnEnd = nullptr;
	}

	m_LastStepTime = std::chrono::steady_clock::now();
	m_CurrentStepIndex++;

	if(IsFinished())
		Stop();
}

CGS* PlayerScenario::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

void PlayerScenario::Start(int ClientID)
{
	if(m_vSteps.empty())
		return;

	m_ClientID = ClientID;
	m_Running = true;
	m_CurrentStepIndex = 0;
	m_LastStepTime = std::chrono::steady_clock::now();
	m_pData[m_ClientID].push_back(*this);
}

void PlayerScenario::Stop()
{
	m_Running = false;
}

void PlayerScenario::Tick()
{
	if(!m_Running || IsFinished())
		return;

	if(!GetPlayer() || !GetCharacter())
	{
		Stop();
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastStepTime).count();
	auto& step = m_vSteps[m_CurrentStepIndex];

	if(step.FuncOnStart)
	{
		step.FuncOnStart(this);
		step.FuncOnStart = nullptr;
	}

	if(step.FuncActive)
	{
		step.FuncActive(this);
	}

	if(CanExecuteStep(step, elapsedMs))
	{
		ExecuteCurrentStep();
	}
}
