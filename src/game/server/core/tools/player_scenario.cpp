#include <game/server/gamecontext.h>
#include "player_scenario.h"

CGS* PlayerScenario::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

IServer* PlayerScenario::Server() const
{
	return Instance::Server();
}

CPlayer* PlayerScenario::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CCharacter* PlayerScenario::GetCharacter() const
{
	return GetPlayer()->GetCharacter();
}

bool PlayerScenario::CanExecuteStep(const Step& step, long elapsedTick) const
{
	switch(step.Priority)
	{
		case ConditionPriority::CONDITION_AND_TIMER:
			return (step.DelayTick <= 0 || (elapsedTick >= step.DelayTick)) &&
				(!step.FuncCheckCondition || step.FuncCheckCondition(this));

		case ConditionPriority::CONDITION_OR_TIMER:
			return (step.DelayTick >= 0 && elapsedTick >= step.DelayTick) ||
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

	m_LastStepTimeTick = Server()->Tick();
	m_CurrentStepIndex++;

	if(IsFinished())
		Stop();
}

void PlayerScenario::Start()
{
	if(m_vSteps.empty())
		return;

	m_Running = true;
	m_CurrentStepIndex = 0;
	m_LastStepTimeTick = Server()->Tick();
}

void PlayerScenario::Stop()
{
	m_Running = false;
}

void PlayerScenario::Tick()
{
	if(!m_Running || IsFinished())
		return;

	if(!GetPlayer())
	{
		Stop();
		return;
	}

	const auto elapsedTick = Server()->Tick() - m_LastStepTimeTick;
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

	if(CanExecuteStep(step, elapsedTick))
	{
		ExecuteCurrentStep();
	}
}
