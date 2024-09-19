#include <game/server/gamecontext.h>
#include "scenario_manager.h"

ScenarioBase::~ScenarioBase()
{
	ScenarioBase::OnUnregisterEventListener(GS()->EventListener());
}

CGS* ScenarioBase::GS() const
{
	return dynamic_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
}

IServer* ScenarioBase::Server() const
{
	return Instance::Server();
}

CPlayer* ScenarioBase::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CCharacter* ScenarioBase::GetCharacter() const
{
	return GetPlayer()->GetCharacter();
}

bool ScenarioBase::CanExecuteStep(const Step& step, long elapsedTick) const
{
	switch(step.Priority)
	{
		case ConditionPriority::CONDITION_AND_TIMER:
			return (step.DelayTick <= 0 || elapsedTick >= step.DelayTick) &&
				(!step.FuncCheckCondition || step.FuncCheckCondition(this));

		case ConditionPriority::CONDITION_OR_TIMER:
			return (step.DelayTick >= 0 && elapsedTick >= step.DelayTick) ||
				(step.FuncCheckCondition && step.FuncCheckCondition(this));
	}
	return false;
}

void ScenarioBase::ExecuteCurrentStep()
{
	auto& step = m_vSteps[m_CurrentStepIndex];
	if(step.FuncOnEnd)
	{
		OnUnregisterEventListener(GS()->EventListener());
		step.FuncOnEnd(this);
		step.FuncOnEnd = nullptr;
	}

	m_LastStepTimeTick = Server()->Tick();
	m_CurrentStepIndex++;

	if(IsFinished())
	{
		if(IsRepeatable())
		{
			m_CurrentStepIndex = 0;
			m_LastStepTimeTick = 0;
			m_vSteps.clear();
			Start();
		}
		else
		{
			Stop();
		}
	}
}

void ScenarioBase::Start()
{
	if(!GetPlayer() || OnStopConditions())
		return;

	OnSetupScenario();

	if(m_vSteps.empty())
		return;

	m_Running = true;
	m_CurrentStepIndex = 0;
	m_LastStepTimeTick = Server()->Tick();
}

void ScenarioBase::Stop()
{
	m_Running = false;
	m_vSteps.clear();
	OnUnregisterEventListener(GS()->EventListener());
}

void ScenarioBase::Tick()
{
	if(!m_Running || IsFinished())
		return;

	if(!GetPlayer() || OnStopConditions())
	{
		Stop();
		return;
	}

	const auto elapsedTick = Server()->Tick() - m_LastStepTimeTick;
	auto& step = m_vSteps[m_CurrentStepIndex];

	if(step.FuncOnStart)
	{
		OnRegisterEventListener(GS()->EventListener());
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