﻿#include "component_registry.h"
#include "scenario_base.h"

#include <scenarios/base/scenario_base_player.h>
#include <scenarios/base/scenario_base_group.h>
#include <game/server/gamecontext.h>

#include <ranges>

ScenarioBase::~ScenarioBase()
{
	if(m_Running)
		Stop();

	OnScenarioEnd();
}

void ScenarioBase::Start()
{
	if(m_Running)
		return;

	OnSetupScenario();

	if(m_StartStepId.empty() && !m_mSteps.empty())
		m_StartStepId = m_vStepOrder.front();

	if(m_StartStepId.empty() || !m_mSteps.contains(m_StartStepId))
		return;

	m_Running = true;
	m_CurrentStepId = m_StartStepId;
	m_LastStepTimeTick = 0;
	OnScenarioStart();
	ExecuteStepStartActions();
}

void ScenarioBase::Stop()
{
	if(!m_Running)
		return;

	if(!IsFinished())
		ExecuteStepEndActions();

	m_Running = false;
	m_CurrentStepId = END_SCENARIO_STEP_ID;
	OnScenarioEnd();
}

void ScenarioBase::Tick()
{
	if(!m_Running || IsFinished())
		return;

	if(OnStopConditions())
	{
		Stop();
		return;
	}

	if(OnPauseConditions())
		return;

	ExecuteStepActiveActions();

	if(m_mSteps.contains(m_CurrentStepId) && m_mSteps.at(m_CurrentStepId).m_CompletionLogic == StepCompletionLogic::SEQUENTIAL)
	{
		TryAdvanceSequentialComponent();
	}

	if(CanConcludeCurrentStep())
		AdvanceStep();
}

void ScenarioBase::ExecuteStepStartActions()
{
	if(IsFinished() || !m_mSteps.contains(m_CurrentStepId))
		return;

	m_LastStepTimeTick = Server()->Tick();
	auto& step = m_mSteps.at(m_CurrentStepId);

	if(step.m_CompletionLogic == StepCompletionLogic::SEQUENTIAL)
	{
		step.m_CurrentComponentIndex = 0;
		if(!step.m_vComponents.empty())
			step.m_vComponents.front()->OnStart();
	}
	else
	{
		for(const auto& pComponent : step.m_vComponents)
			pComponent->OnStart();
	}
}

void ScenarioBase::ExecuteStepActiveActions()
{
	if(IsFinished() || !m_mSteps.contains(m_CurrentStepId))
		return;

	auto& currentStep = m_mSteps.at(m_CurrentStepId);
	if(Server()->Tick() % Server()->TickSpeed() == 0 && !currentStep.m_MsgInfo.empty())
	{
		if(const auto* pPlayerScenario = dynamic_cast<PlayerScenarioBase*>(this))
		{
			if(const auto* pPlayer = pPlayerScenario->GetPlayer())
				GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::VeryImportant, Server()->TickSpeed(), currentStep.m_MsgInfo.c_str());
		}
		else if(auto* pGroupScenario = dynamic_cast<GroupScenarioBase*>(this))
		{
			for(const auto& CID : pGroupScenario->GetParticipants())
				GS()->Broadcast(CID, BroadcastPriority::VeryImportant, Server()->TickSpeed(), currentStep.m_MsgInfo.c_str());
		}
	}

	if(currentStep.m_CompletionLogic == StepCompletionLogic::SEQUENTIAL)
	{
		if(currentStep.m_CurrentComponentIndex < currentStep.m_vComponents.size())
			currentStep.m_vComponents[currentStep.m_CurrentComponentIndex]->OnActive();
	}
	else
	{
		for(const auto& pComponent : currentStep.m_vComponents)
			pComponent->OnActive();
	}
}

void ScenarioBase::ExecuteStepEndActions()
{
	if(IsFinished() || !m_mSteps.contains(m_CurrentStepId))
		return;

	auto& currentStep = m_mSteps.at(m_CurrentStepId);
	if(currentStep.m_CompletionLogic == StepCompletionLogic::SEQUENTIAL)
	{
		if(currentStep.m_CurrentComponentIndex < currentStep.m_vComponents.size())
			currentStep.m_vComponents[currentStep.m_CurrentComponentIndex]->OnEnd();
	}
	else
	{
		for(const auto& pComponent : currentStep.m_vComponents)
			pComponent->OnEnd();
	}
}

bool ScenarioBase::CanConcludeCurrentStep() const
{
	if(IsFinished())
		return false;

	const Step& currentStep = m_mSteps.at(m_CurrentStepId);
	bool conditionMet;

	if(currentStep.m_CompletionLogic == StepCompletionLogic::SEQUENTIAL)
	{
		conditionMet = currentStep.m_CurrentComponentIndex >= currentStep.m_vComponents.size();
	}
	else
	{
		auto isComponentFinished = [&](const auto& p) { return p->IsFinished(); };
		conditionMet = (currentStep.m_CompletionLogic == StepCompletionLogic::ANY_OF) ?
			std::ranges::any_of(currentStep.m_vComponents, isComponentFinished) :
			std::ranges::all_of(currentStep.m_vComponents, isComponentFinished);
	}


	bool timerElapsed = false;
	if(currentStep.m_DelayTick >= 0)
	{
		timerElapsed = (Server()->Tick() - m_LastStepTimeTick) >= currentStep.m_DelayTick;
	}
	else
	{
		timerElapsed = (currentStep.m_Priority == ConditionPriority::CONDITION_OR_TIMER);
	}

	return (currentStep.m_Priority == ConditionPriority::CONDITION_AND_TIMER) ?
		(conditionMet && (currentStep.m_DelayTick < 0 || timerElapsed)) :
		(conditionMet || timerElapsed);
}

void ScenarioBase::TryAdvanceSequentialComponent()
{
	auto& currentStep = m_mSteps.at(m_CurrentStepId);
	if(currentStep.m_CurrentComponentIndex >= currentStep.m_vComponents.size())
		return;

	auto& pCurrentComp = currentStep.m_vComponents[currentStep.m_CurrentComponentIndex];
	if(pCurrentComp->IsFinished())
	{
		pCurrentComp->OnEnd();
		currentStep.m_CurrentComponentIndex++;

		if(currentStep.m_CurrentComponentIndex < currentStep.m_vComponents.size())
			currentStep.m_vComponents[currentStep.m_CurrentComponentIndex]->OnStart();
	}
}

void ScenarioBase::AdvanceStep()
{
	if(IsFinished())
		return;

	ExecuteStepEndActions();

	std::optional<StepId> nextStepIdOpt;
	const auto& currentStep = m_mSteps.at(m_CurrentStepId);
	for(const auto& pComponent : currentStep.m_vComponents)
	{
		auto componentNextId = pComponent->GetNextStepId();
		if(componentNextId.has_value())
		{
			nextStepIdOpt = componentNextId;
			break;
		}
	}

	if(!nextStepIdOpt.has_value())
	{
		auto it = std::ranges::find(m_vStepOrder, m_CurrentStepId);
		if(it != m_vStepOrder.end() && std::next(it) != m_vStepOrder.end())
			nextStepIdOpt = *std::next(it);
	}

	const StepId nextStepId = nextStepIdOpt.value_or(END_SCENARIO_STEP_ID);
	if(nextStepId == END_SCENARIO_STEP_ID || !m_mSteps.contains(nextStepId))
	{
		Stop();
		return;
	}

	m_CurrentStepId = nextStepId;
	ExecuteStepStartActions();
}

CGS* ScenarioBase::GS() const { return m_pGS; }
IServer* ScenarioBase::Server() const { return GS()->Server(); }

void ScenarioBase::SetupStep(Step& NewStep, const nlohmann::json& StepJson)
{
	if(const auto logic = StepJson.value("completion_logic", "all_of"); logic == "any_of")
		NewStep.m_CompletionLogic = StepCompletionLogic::ANY_OF;
	else if(logic == "sequential")
		NewStep.m_CompletionLogic = StepCompletionLogic::SEQUENTIAL;

	if(StepJson.contains("components"))
	{
		for(const auto& CompJson : StepJson["components"])
		{
			const auto Type = CompJson.value("type", "");
			if(Type.empty())
				continue;

			if(auto pComponent = ComponentRegistry::GetInstance().Create(Type, CompJson, this))
			{
				NewStep.AddComponent(std::move(pComponent));
			}
		}
	}
}

[[nodiscard]] ScenarioBase::Step& ScenarioBase::AddStep(StepId id, std::string MsgInfo, int delayTick)
{
	m_vStepOrder.push_back(id);
	auto [it, inserted] = m_mSteps.try_emplace(id, std::move(id), MsgInfo, delayTick);
	return it->second;
}