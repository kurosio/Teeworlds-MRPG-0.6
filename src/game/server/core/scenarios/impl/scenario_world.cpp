#include "scenario_world.h"
#include "components/default.h"
#include <game/server/core/scenarios/base/component_registry.h>
#include <game/server/player.h>

class WorldGroupLivesComponent final : public Component<GroupScenarioBase, WorldGroupLivesComponent>
{
	enum class EAction { Set, Add, Subtract };
	EAction m_Action { EAction::Set };
	int m_Value {};

public:
	explicit WorldGroupLivesComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		const auto action = j.value("action", "set");
		m_Value = j.value("value", 0);
		if(action == "add")
			m_Action = EAction::Add;
		else if(action == "sub" || action == "subtract")
			m_Action = EAction::Subtract;
	}

	DECLARE_COMPONENT_NAME("world_group_lives")

private:
	void OnStartImpl() override
	{
		if(m_Action == EAction::Set)
			Scenario()->SetGroupLives(m_Value);
		else if(m_Action == EAction::Add)
			Scenario()->AddGroupLives(m_Value);
		else
			Scenario()->AddGroupLives(-m_Value);

		Finish();
	}
};
template struct ComponentRegistrar<WorldGroupLivesComponent>;

class WorldCompleteComponent final : public Component<CWorldScenario, WorldCompleteComponent>
{
	bool m_Successful {};

public:
	explicit WorldCompleteComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Successful = j.value("successful", true);
	}

	DECLARE_COMPONENT_NAME("world_complete")

private:
	void OnStartImpl() override
	{
		if(m_Successful)
		{
			for(const int ClientID : Scenario()->GetParticipants())
			{
				auto* pPlayer = GS()->GetPlayer(ClientID);
				if(!pPlayer)
					continue;

				for(const auto& Reward : Scenario()->GetContextRewards())
				{
					auto* pItem = pPlayer->GetItem(Reward.m_ItemID);
					if(pItem && random_float(100.0f) < Reward.m_Chance)
					{
						pItem->Add(Reward.m_Value, 0, 0, false);
						GS()->Chat(ClientID, "World scenario reward: {} x{}.", pItem->Info()->GetName(), Reward.m_Value);
					}
				}
			}
		}

		m_NextStepId = END_SCENARIO_STEP_ID;
		Finish();
	}
};
template struct ComponentRegistrar<WorldCompleteComponent>;

CWorldScenario::CWorldScenario(const nlohmann::json& jsonData)
	: WorldScenarioBase()
{
	m_JsonData = jsonData;
}

void CWorldScenario::OnSetupScenario()
{
	if(!m_JsonData.is_object() || !m_JsonData.contains("steps") || !m_JsonData["steps"].is_array())
		return;

	const auto& steps = m_JsonData["steps"];
	if(!steps.empty())
		m_StartStepId = steps[0].value("id", "");

	for(const auto& step : steps)
		ProcessStep(step);
}

void CWorldScenario::OnScenarioStart()
{
	m_EventListener.Init(this, IEventListener::CharacterDeath);
	m_EventListener.Register();
}

void CWorldScenario::OnScenarioEnd()
{
	m_EventListener.Unregister();
	WorldScenarioBase::OnScenarioEnd();
}

void CWorldScenario::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	if(!pVictim || pVictim->IsBot() || !HasPlayer(pVictim))
		return;

	if(ConsumeGroupLife())
		GS()->ChatWorld(GetWorldID(), "World scenario", "Group lives left: {}.", GetGroupLives());
	else
		RemoveParticipant(pVictim->GetCID());
}

void CWorldScenario::ProcessStep(const nlohmann::json& stepJson)
{
	if(!stepJson.is_object())
		return;

	StepId id = stepJson.value("id", "");
	if(id.empty() || !stepJson.contains("components") || !stepJson["components"].is_array())
		return;

	auto& newStep = AddStep(id, stepJson.value("msg_info", ""), stepJson.value("delay", -1));
	SetupStep(newStep, stepJson);
}
