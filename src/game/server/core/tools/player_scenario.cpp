#include <game/server/gamecontext.h>
#include "player_scenario.h"

bool PlayerScenario::CanExecuteStep(const Step& step, long elapsedMs) const
{
	if(step.FuncCondition && step.FuncCondition(this))
		return true;
	if(step.DelayMs >= 0 && elapsedMs >= step.DelayMs)
		return true;
	return false;
}

void PlayerScenario::ExecuteCurrentStep()
{
	const auto& step = m_vSteps[m_CurrentStepIndex];
	step.FuncAction(this);
	m_LastStepTime = std::chrono::steady_clock::now();
	m_CurrentStepIndex++;

	if(IsFinished())
	{
		Stop();
	}
}

CGS* PlayerScenario::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

void PlayerScenario::Start()
{
	if(m_vSteps.empty())
		return;

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
		m_vSteps.clear();
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastStepTime).count();
	const auto& step = m_vSteps[m_CurrentStepIndex];

	if(CanExecuteStep(step, elapsedMs))
		ExecuteCurrentStep();
}
