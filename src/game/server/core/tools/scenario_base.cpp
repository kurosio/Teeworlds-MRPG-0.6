#include "scenario_base.h"

#include <game/server/gamecontext.h>

ScenarioBase::~ScenarioBase()
{
	if(m_Running)
		Stop();

	ScenarioBase::OnScenarioEnd();
}

void ScenarioBase::Start()
{
	if(m_Running)
		return;

	OnSetupScenario();

	m_Running = true;
	m_CurrentStepIndex = 0;
	m_LastStepTimeTick = 0;

	OnScenarioStart();

	if(!m_vSteps.empty())
		ExecuteStepStartActions();
	else
		Stop();
}

void ScenarioBase::Stop()
{
	if(!m_Running)
		return;

	if(!IsFinished())
	{
		ExecuteStepEndActions();
	}

	m_Running = false;
	m_CurrentStepIndex = m_vSteps.size();
	OnScenarioEnd();
}

void ScenarioBase::Tick()
{
	if(!m_Running || IsFinished())
		return;

	// check stop condition
	if(OnStopConditions())
	{
		Stop();
		return;
	}

	// check pause condition
	if(OnPauseConditions())
		return;

	// execute active actions for current step
	ExecuteStepActiveActions();

	// check if the current step can be concluded
	if(CanConcludeCurrentStep())
		AdvanceStep();
}

bool ScenarioBase::CanConcludeCurrentStep() const
{
	if(IsFinished())
		return false;

	const Step& currentStep = m_vSteps[m_CurrentStepIndex];
	bool conditionMet = true;
	bool timerElapsed = false;

	// check condition if provided
	if(currentStep.FuncCheckCondition)
		conditionMet = currentStep.FuncCheckCondition();

	// check timer if provided
	if(currentStep.DelayTick >= 0)
	{
		long elapsedTick = Server()->Tick() - m_LastStepTimeTick;
		timerElapsed = elapsedTick >= currentStep.DelayTick;
	}
	else
	{
		timerElapsed = (currentStep.Priority == ConditionPriority::CONDITION_OR_TIMER);
	}

	// evaluate based on priority
	switch(currentStep.Priority)
	{
		case ConditionPriority::CONDITION_AND_TIMER:
			return conditionMet && (currentStep.DelayTick < 0 ? true : timerElapsed);
		case ConditionPriority::CONDITION_OR_TIMER:
			return conditionMet || timerElapsed;
		default:
			return false;
	}
}

void ScenarioBase::ExecuteStepStartActions()
{
	if(IsFinished())
		return;

	m_LastStepTimeTick = Server()->Tick();
	const Step& currentStep = m_vSteps[m_CurrentStepIndex];
	OnStepStart(m_CurrentStepIndex);

	if(currentStep.FuncOnStart)
		currentStep.FuncOnStart();
}

void ScenarioBase::ExecuteStepActiveActions()
{
	if(IsFinished())
		return;

	const Step& currentStep = m_vSteps[m_CurrentStepIndex];
	OnStepTick(m_CurrentStepIndex);

	if(currentStep.FuncActive)
		currentStep.FuncActive();
}

void ScenarioBase::ExecuteStepEndActions()
{
	if(IsFinished())
		return;

	const Step& currentStep = m_vSteps[m_CurrentStepIndex];
	OnStepEnd(m_CurrentStepIndex);

	if(currentStep.FuncOnEnd)
		currentStep.FuncOnEnd();
}

void ScenarioBase::AdvanceStep()
{
	if(IsFinished())
		return;

	ExecuteStepEndActions();
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
	else
	{
		ExecuteStepStartActions();
	}
}

CGS* ScenarioBase::GS() const
{
	return m_pGS;
}

IServer* ScenarioBase::Server() const
{
	return GS()->Server();
}

// --- Step Builder Methods Implementation ---
ScenarioBase& ScenarioBase::AddStep(int delayTick)
{
	m_vSteps.emplace_back(delayTick);
	OnStepAdded(m_vSteps.back());
	return *this;
}

ScenarioBase& ScenarioBase::WhenStarted(const ScenarioAction& pfnOnStart)
{
	if(!m_vSteps.empty())
	{
		m_vSteps.back().FuncOnStart = pfnOnStart;
	}
	return *this;
}

ScenarioBase& ScenarioBase::WhenActive(const ScenarioAction& pfnActive)
{
	if(!m_vSteps.empty())
	{
		m_vSteps.back().FuncActive = pfnActive;
	}
	return *this;
}

ScenarioBase& ScenarioBase::WhenFinished(const ScenarioAction& pfnOnEnd)
{
	if(!m_vSteps.empty())
	{
		m_vSteps.back().FuncOnEnd = pfnOnEnd;
	}
	return *this;
}

ScenarioBase& ScenarioBase::CheckCondition(ConditionPriority priority, const ScenarioCondition& pfnCheckCondition)
{
	if(!m_vSteps.empty())
	{
		m_vSteps.back().Priority = priority;
		m_vSteps.back().FuncCheckCondition = pfnCheckCondition;
	}
	return *this;
}


// --- PlayerScenarioBase Implementation ---
bool PlayerScenarioBase::OnPauseConditions()
{
	return !GetCharacter();
}

bool PlayerScenarioBase::OnStopConditions()
{
	return !GetPlayer();
}

CPlayer* PlayerScenarioBase::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CCharacter* PlayerScenarioBase::GetCharacter() const
{
	return GS()->GetPlayerChar(m_ClientID);
}

std::vector<CPlayer*> GroupScenarioBase::GetPlayers() const
{
	std::vector<CPlayer*> vResult {};

	for(auto& clientId : m_vParticipantIDs)
		vResult.push_back(GS()->GetPlayer(clientId));

	return vResult;
}

std::vector<CCharacter*> GroupScenarioBase::GetCharacters() const
{
	std::vector<CCharacter*> vResult {};

	for(auto& clientId : m_vParticipantIDs)
		vResult.push_back(GS()->GetPlayerChar(clientId));

	return vResult;
}

// --- GroupScenarioBase Implementation ---
bool GroupScenarioBase::OnPauseConditions()
{
	return std::ranges::all_of(m_vParticipantIDs, [this](int ClientID)
	{
		return GS()->GetPlayerChar(ClientID) == nullptr;
	});
}

bool GroupScenarioBase::OnStopConditions()
{
	return m_vParticipantIDs.empty() || std::ranges::all_of(m_vParticipantIDs, [this](int ClientID)
	{
		return GS()->GetPlayer(ClientID) == nullptr;
	});
}

bool GroupScenarioBase::AddParticipant(int ClientID)
{
	auto [it, inserted] = m_vParticipantIDs.insert(ClientID);
	if(inserted)
	{
		OnPlayerJoin(ClientID);
	}

	return inserted;
}

bool GroupScenarioBase::RemoveParticipant(int ClientID)
{
	if(m_vParticipantIDs.erase(ClientID) > 0)
	{
		bool isStopping = !m_Running;
		OnPlayerLeave(ClientID, isStopping);
		return true;
	}

	return false;
}

bool GroupScenarioBase::IsParticipant(int ClientID) const
{
	return m_vParticipantIDs.contains(ClientID);
}

int GroupScenarioBase::GetParticipantCount() const
{
	return static_cast<int>(m_vParticipantIDs.size());
}

void GroupScenarioBase::OnScenarioEnd()
{
	std::vector<int> participantsToNotify(m_vParticipantIDs.begin(), m_vParticipantIDs.end());
	for(int clientID : participantsToNotify)
		OnPlayerLeave(clientID, true);
}