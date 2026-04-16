#include "scenario_dungeon.h"
#include <game/server/gamecontext.h>
#include <game/server/worldmodes/dungeon/dungeon.h>

#include "components/default.h"
#include <game/server/core/scenarios/base/component_registry.h>

class DoorControlComponent final : public Component<CDungeonScenario, DoorControlComponent>
{
	enum class DoorAction { Create, Remove };
	std::string m_Key {};
	vec2 m_Pos {};
	DoorAction m_Action {};

public:
	explicit DoorControlComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Key = j.value("key", "");
		m_Pos = j.value("position", vec2());
		auto actionStr = j.value("action", "create");
		m_Action = actionStr == "remove" ? DoorAction::Remove : DoorAction::Create;
	}

    DECLARE_COMPONENT_NAME("dungeon_door_control")

private:
	void OnStartImpl() override
	{
		if(m_Action == DoorAction::Create)
		{
			Scenario()->m_vpDoors[m_Key].m_Pos = m_Pos;
			Scenario()->m_vpDoors[m_Key].m_EntPtr = std::make_unique<CEntityBaseDoor>(&GS()->m_World, CGameWorld::ENTTYPE_DEFAULT_DOOR, m_Pos, vec2(0, -1));
		}
		else if(m_Action == DoorAction::Remove)
		{
			if(Scenario()->m_vpDoors.contains(m_Key))
				Scenario()->m_vpDoors.erase(m_Key);
		}

		Finish();
	}
};
template struct ComponentRegistrar<DoorControlComponent>;

class UseChatComponent final : public Component<CDungeonScenario, UseChatComponent>, public IEventListener
{
	ScopedEventListener m_ListenerScope {};
	std::string m_ChatCode {};
	bool m_Hidden {};

public:
	explicit UseChatComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_ChatCode = j.value("code", "");
		m_Hidden = j.value("hidden", true);
		m_ListenerScope.Init(this, IEventListener::PlayerChat);
	}

    DECLARE_COMPONENT_NAME("dungeon_use_chat_code")

private:
	void OnStartImpl() override
	{
		m_ListenerScope.Register();
	}

	void OnActiveImpl() override
	{
		if(m_Hidden || Server()->Tick() % Server()->TickSpeed() != 0)
			return;

		for(auto& CID : Scenario()->GetParticipants())
			GS()->Broadcast(CID, BroadcastPriority::VeryImportant, Server()->TickSpeed(), "Objective: Write in the chat '{}'", m_ChatCode);
	}

	void OnPlayerChat(CPlayer* pFrom, const char* pMessage) override
	{
		if(Scenario()->HasPlayer(pFrom) && std::string_view(pMessage) == m_ChatCode)
			Finish();
	}

	void OnEndImpl() override
	{
		m_ListenerScope.Unregister();
	}
};
template struct ComponentRegistrar<UseChatComponent>;

struct ActivatePointComponent final : public Component<GroupScenarioBase, ActivatePointComponent>
{
	vec2 m_Position {};
	bool m_EntireGroup {};
	int m_Duration {};
	int m_TargetTick {};
	int m_ActivationTick {};
	std::string m_Action {};

public:
	explicit ActivatePointComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Position = j.value("position", vec2());
		m_EntireGroup = j.value("entire_group", false);
		m_Duration = j.value("duration", 0);
		m_Action = j.value("action_text", "Activating Point");
	}

    DECLARE_COMPONENT_NAME("dungeon_activate_point")

private:
	template<typename... Args>
	void Broadcast(const char* pFormat, Args&&... args) const
	{
		for(auto* pPlayer : Scenario()->GetPlayers())
			GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::TitleInformation, Server()->TickSpeed(), pFormat, std::forward<Args>(args)...);
	}

	void BroadcastCooldownProgress() const
	{
		// initialize variables
		const int TicksLeft = m_TargetTick - Server()->Tick();
		const int SecLeft = std::max(0, TicksLeft / Server()->TickSpeed());
		const int CentiSecLeft = std::max(0, (TicksLeft % Server()->TickSpeed()) * 100 / Server()->TickSpeed());
		const float currentProgress = translate_to_percent(m_TargetTick, Server()->Tick());

		// send information
		char aTimeFormat[32] {};
		str_format(aTimeFormat, sizeof(aTimeFormat), "%d.%.2ds", SecLeft, CentiSecLeft);
		std::string progressBar = mystd::string::progressBar(100, static_cast<int>(currentProgress), 10, "\u25B0", "\u25B1");
		Broadcast("{}\n< {} > {} - Action", m_Action, aTimeFormat, progressBar);
	}

	void ResetProgress()
	{
		m_ActivationTick = 0;
	}

	void OnStartImpl() override
	{
		ResetProgress();
	}

	void OnActiveImpl() override
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->CreateHammerHit(m_Position, Scenario()->GetClientsMask());
		}

		const auto& vpPlayers = Scenario()->GetPlayers();
		auto IsInsideFunc = [&](const CPlayer* pPlayer)
		{
			const auto* pChr = pPlayer->GetCharacter();
			return pChr && distance(pChr->GetPos(), m_Position) < 128.f;
		};

		// check condition
		const bool ConditionMet = m_EntireGroup ?
			(!vpPlayers.empty() && std::all_of(vpPlayers.begin(), vpPlayers.end(), IsInsideFunc)) :
			std::any_of(vpPlayers.begin(), vpPlayers.end(), IsInsideFunc);

		// nonactive condition
		if(!ConditionMet)
		{
			if(m_ActivationTick != 0)
			{
				ResetProgress();
				Broadcast("< Capture interrupted! >");
			}

			return;
		}

		// active condition
		if(m_Duration <= 0)
		{
			Finish();
			return;
		}

		if(m_ActivationTick == 0)
		{
			m_ActivationTick = Server()->Tick();
			m_TargetTick = m_ActivationTick + m_Duration * Server()->TickSpeed();
		}

		if(Server()->Tick() >= m_TargetTick)
		{
			Finish();
			return;
		}
		else if(Server()->Tick() % (Server()->TickSpeed() / 5) == 0)
			BroadcastCooldownProgress();
	}
};
template struct ComponentRegistrar<ActivatePointComponent>;

class CompleteDungeonComponent final : public Component<CDungeonScenario, CompleteDungeonComponent>
{
public:
	explicit CompleteDungeonComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
	}

	DECLARE_COMPONENT_NAME("dungeon_complete")

private:
	void OnStartImpl() override
	{
		if(auto* pController = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController))
			pController->FinishDungeon();

		Finish();
	}
};
template struct ComponentRegistrar<CompleteDungeonComponent>;

CDungeonScenario::CDungeonScenario(const nlohmann::json& jsonData) : GroupScenarioBase(), m_JsonData(jsonData)
{
}

void CDungeonScenario::OnSetupScenario()
{
	if(!m_JsonData.is_object() || !m_JsonData.contains("steps") || !m_JsonData["steps"].is_array())
		return;

	// start step by first
	const auto& steps = m_JsonData["steps"];
	if(!steps.empty())
		m_StartStepId = steps[0].value("id", "");

	// setup all steps
	for(const auto& step : steps)
		ProcessStep(step);
}

void CDungeonScenario::ProcessStep(const nlohmann::json& stepJson)
{
	if(!stepJson.is_object())
		return;

	StepId id = stepJson.value("id", "");
	if(id.empty() || !stepJson.contains("components") || !stepJson["components"].is_array())
		return;

	auto& newStep = AddStep(id,
		stepJson.value("msg_info", ""),
		stepJson.value("delay", -1));
	SetupStep(newStep, stepJson);
}
