#include "scenario_dungeon.h"
#include <game/server/gamecontext.h>

#include "components/default.h"
#include <game/server/core/scenarios/base/component_registry.h>

class DefeatMobsComponent final : public Component<CDungeonScenario, DefeatMobsComponent>, public IEventListener
{
	enum class EMode { Annihilation, Wave, Survival };
	inline static EMode GetMode(std::string_view modeStr)
	{
		if(modeStr == "wave") return EMode::Wave;
		else if(modeStr == "survival") return EMode::Survival;
		else return EMode::Annihilation;
	}

	ScopedEventListener m_ListenerScope {};
	struct Properties
	{
		int TargetKills {};
		int Duration {};
	};

	Properties m_Prop {};
	std::set<int> m_SpawnedBotIds {};
	EMode m_Mode {};
	vec2 m_Position {};
	float m_Radius {};
	int m_KillsMade {};
	int m_TargetTick {};
	nlohmann::json m_MobsData {};

public:
	explicit DefeatMobsComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Mode = GetMode(j.value("mode", ""));
		m_Radius = j.value("radius", 180.f);
		m_Position = j.value("position", vec2 {});
		m_MobsData = j.value("mobs", nlohmann::json::array());
		m_Prop.TargetKills = j.value("kill_target", (int)NOPE);
		m_Prop.Duration = j.value("duration", 0);
		m_ListenerScope.Init(this, IEventListener::CharacterDeath);
	}

    DECLARE_COMPONENT_NAME("defeat_mobs")

private:
	void Reset()
	{
		m_KillsMade = 0;
		m_TargetTick = Server()->Tick() + m_Prop.Duration * Server()->TickSpeed();
	}

	void OnStartImpl() override
	{
		m_ListenerScope.Register();
		Reset();

		for(const auto& mobData : m_MobsData)
		{
			const int mobID = mobData.value("mob_id", (int)NOPE);
			if(!MobBotInfo::ms_aMobBot.contains(mobID))
				continue;

			// initialize and create new mobs
			MobBotInfo mobInfo = MobBotInfo::ms_aMobBot[mobID];
			mobInfo.m_Level = mobData.value("level", 1);
			mobInfo.m_Power = mobData.value("power", 1);
			mobInfo.m_Position = m_Position;
			mobInfo.m_Radius = m_Radius;
			mobInfo.m_WorldID = GS()->GetWorldID();
			for(int i = 0; i < mobData.value("count", 1); ++i)
			{
				if(auto* pPlayerBot = GS()->CreateBot(TYPE_BOT_MOB, mobInfo.m_BotID, mobID))
				{
					pPlayerBot->InitBotMobInfo(mobInfo);
					pPlayerBot->SetAllowedSpawn(true);
					m_SpawnedBotIds.insert(pPlayerBot->GetCID());
				}
			}
		}
	}

	void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) override
	{
		if(!pVictim || !m_SpawnedBotIds.contains(pVictim->GetCID()))
			return;

		bool ShouldDestroy = false;
		auto* pPlayerBot = static_cast<CPlayerBot*>(pVictim);
		switch(m_Mode)
		{
			default:
				ShouldDestroy = true;
				break;

			case EMode::Survival:
				ShouldDestroy = Server()->Tick() >= m_TargetTick;
				break;

			case EMode::Wave:
				ShouldDestroy = ++m_KillsMade >= m_Prop.TargetKills;
				break;
		}

		if(ShouldDestroy)
		{
			m_SpawnedBotIds.erase(pVictim->GetCID());
			pPlayerBot->MarkForDestroy();
		}
	}

	void OnActiveImpl() override
	{
		bool ShouldFinish = false;
		switch(m_Mode)
		{
			default:
				if(Server()->Tick() % Server()->TickSpeed() == 0)
				{
					for(auto& CID : Scenario()->GetParticipants())
						GS()->Broadcast(CID, BroadcastPriority::TitleInformation, Server()->TickSpeed(),
							"Objective: Defeat all the mobs. Remaining: {} mobs.", m_SpawnedBotIds.size());
				}

				ShouldFinish = m_SpawnedBotIds.empty();
				break;

			case EMode::Wave:
				if(Server()->Tick() % Server()->TickSpeed() == 0)
				{
					for(auto& CID : Scenario()->GetParticipants())
						GS()->Broadcast(CID, BroadcastPriority::TitleInformation, Server()->TickSpeed(),
							"Objective: Defeat wave mobs '{} of {}'", m_KillsMade, m_Prop.TargetKills);
				}

				ShouldFinish = m_KillsMade >= m_Prop.TargetKills;
				break;

			case EMode::Survival:
			{
				const int timeLeft = (m_TargetTick - Server()->Tick()) / Server()->TickSpeed();
				if(Server()->Tick() % Server()->TickSpeed() == 0 && timeLeft >= 0)
				{
					for(auto& CID : Scenario()->GetParticipants())
						GS()->Broadcast(CID, BroadcastPriority::TitleInformation, Server()->TickSpeed(),
							"Objective: Survive! Time left: {}s", timeLeft);
				}

				ShouldFinish = Server()->Tick() >= m_TargetTick;
				break;
			}
		}

		if(ShouldFinish)
			Finish();
	}

	void OnEndImpl() override
	{
		for(int CID : m_SpawnedBotIds)
		{
			if(auto* pPlayer = GS()->GetPlayer(CID))
				pPlayer->MarkForDestroy();
		}
		m_SpawnedBotIds.clear();
		m_ListenerScope.Unregister();
	}
};
template struct ComponentRegistrar<DefeatMobsComponent>;

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

    DECLARE_COMPONENT_NAME("door_control")

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

    DECLARE_COMPONENT_NAME("use_chat_code")

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

    DECLARE_COMPONENT_NAME("activate_point")

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
			GS()->CreateHammerHit(m_Position);

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
	StepId id = stepJson.value("id", "");
	if(id.empty() || !stepJson.contains("components"))
		return;

	auto& newStep = AddStep(id,
		stepJson.value("msg_info", ""),
		stepJson.value("delay", -1));

	if(const auto logic = stepJson.value("completion_logic", "all_of"); logic == "any_of")
		newStep.m_CompletionLogic = StepCompletionLogic::ANY_OF;
	else if(logic == "sequential")
		newStep.m_CompletionLogic = StepCompletionLogic::SEQUENTIAL;

	for(const auto& compJson : stepJson["components"])
	{
		const auto type = compJson.value("type", "");
		if(type.empty())
			continue;

		if(auto pComponent = ComponentRegistry::GetInstance().Create(type, compJson, this))
		{
			newStep.AddComponent(std::move(pComponent));
		}
	}
}