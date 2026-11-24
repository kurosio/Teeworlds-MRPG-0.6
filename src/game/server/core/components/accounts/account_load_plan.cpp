#include "account_load_plan.h"

#include <base/system.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

struct CAccountLoadPlan::SState
{
	struct SStage
	{
		std::string m_Name;
		std::vector<std::string> m_Dependencies;
		CStage m_Start;
		bool m_Running {};
		bool m_Done {};
	};

	std::vector<SStage> m_vStages;
	CCompletion m_OnComplete;
	bool m_Started {};
	bool m_Finished {};
	bool m_Pumping {};
	bool m_Repump {};
};

void CAccountLoadPlan::Add(std::string Name, std::vector<std::string> Dependencies, CStage Start)
{
	if(!m_pState)
		m_pState = std::make_shared<SState>();
	auto& State = *m_pState;
	if(State.m_Started)
	{
		dbg_msg("Account", "cannot register load stage '%s' after starting the plan", Name.c_str());
		Complete(m_pState, State.m_vStages.size(), false);
		return;
	}
	State.m_vStages.push_back({ std::move(Name), std::move(Dependencies), std::move(Start) });
}

void CAccountLoadPlan::Start(CCompletion OnComplete)
{
	if(!m_pState)
		m_pState = std::make_shared<SState>();
	auto& State = *m_pState;
	if(State.m_Started)
	{
		dbg_msg("Account", "account load plan started more than once");
		return;
	}
	State.m_Started = true;
	State.m_OnComplete = std::move(OnComplete);

	std::unordered_set<std::string> Names;
	for(const auto& Stage : State.m_vStages)
	{
		if(Stage.m_Name.empty() || !Stage.m_Start || !Names.insert(Stage.m_Name).second)
		{
			dbg_msg("Account", "account load plan has an invalid or duplicate stage");
			Complete(m_pState, State.m_vStages.size(), false);
			return;
		}
	}
	for(const auto& Stage : State.m_vStages)
	{
		for(const auto& Dependency : Stage.m_Dependencies)
		{
			if(!Names.contains(Dependency))
			{
				dbg_msg("Account", "account load stage '%s' has an unknown dependency '%s'", Stage.m_Name.c_str(), Dependency.c_str());
				Complete(m_pState, State.m_vStages.size(), false);
				return;
			}
		}
	}
	Pump(m_pState);
}

void CAccountLoadPlan::Cancel()
{
	if(!m_pState || m_pState->m_Finished)
		return;
	m_pState->m_Finished = true;
	m_pState->m_OnComplete = {};
	for(auto& Stage : m_pState->m_vStages)
		Stage.m_Start = {};
}

void CAccountLoadPlan::Complete(const std::shared_ptr<SState>& pState, size_t Index, bool Success)
{
	if(!pState || pState->m_Finished)
		return;
	if(Index < pState->m_vStages.size())
	{
		auto& Stage = pState->m_vStages[Index];
		if(!Stage.m_Running || Stage.m_Done)
			return;
		Stage.m_Running = false;
		Stage.m_Done = Success;
		if(!Success)
			dbg_msg("Account", "account load stage '%s' failed", Stage.m_Name.c_str());
	}
	pState->m_Finished = !Success || std::all_of(pState->m_vStages.begin(), pState->m_vStages.end(), [](const SState::SStage& Stage) { return Stage.m_Done; });
	if(pState->m_Finished)
	{
		auto OnComplete = std::move(pState->m_OnComplete);
		if(OnComplete)
			OnComplete(Success);
		return;
	}
	Pump(pState);
}

void CAccountLoadPlan::Pump(const std::shared_ptr<SState>& pState)
{
	if(!pState || pState->m_Finished)
		return;
	if(pState->m_Pumping)
	{
		pState->m_Repump = true;
		return;
	}

	pState->m_Pumping = true;
	do
	{
		pState->m_Repump = false;
		bool Running = false;
		bool Waiting = false;
		for(size_t Index = 0; Index < pState->m_vStages.size(); ++Index)
		{
			auto& Stage = pState->m_vStages[Index];
			if(Stage.m_Done)
				continue;
			if(Stage.m_Running)
			{
				Running = true;
				continue;
			}
			Waiting = true;
			const bool Ready = std::all_of(Stage.m_Dependencies.begin(), Stage.m_Dependencies.end(), [&pState](const std::string& Dependency)
			{
				const auto It = std::find_if(pState->m_vStages.begin(), pState->m_vStages.end(), [&Dependency](const SState::SStage& Candidate) { return Candidate.m_Name == Dependency; });
				return It != pState->m_vStages.end() && It->m_Done;
			});
			if(!Ready)
				continue;

			Stage.m_Running = true;
			Running = true;
			auto pCalled = std::make_shared<bool>(false);
			try
			{
				Stage.m_Start([pState, Index, pCalled](bool Success)
				{
					if(std::exchange(*pCalled, true))
						return;
					Complete(pState, Index, Success);
				});
			}
			catch(...)
			{
				if(!std::exchange(*pCalled, true))
					Complete(pState, Index, false);
			}
			if(pState->m_Finished)
				break;
		}

		if(pState->m_Finished)
			break;
		if(std::all_of(pState->m_vStages.begin(), pState->m_vStages.end(), [](const SState::SStage& Stage) { return Stage.m_Done; }))
		{
			Complete(pState, pState->m_vStages.size(), true);
			break;
		}
		if(!Running && Waiting)
		{
			dbg_msg("Account", "account load plan contains a dependency cycle");
			Complete(pState, pState->m_vStages.size(), false);
		}
	} while(pState->m_Repump);
	pState->m_Pumping = false;
}
