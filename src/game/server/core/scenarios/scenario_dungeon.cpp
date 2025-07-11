#include "scenario_dungeon.h"
#include <game/server/gamecontext.h>

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

class FollowCameraComponent final : public Component<CDungeonScenario, FollowCameraComponent>
{
	vec2 m_Pos {};
	bool m_Smooth {};

public:
	explicit FollowCameraComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Pos = j.value("position", vec2());
		m_Smooth = j.value("smooth", true);
	}

	void OnActiveImpl() override
	{
		if(!m_DelayTick)
		{
			Finish();
			return;
		}

		for(auto* pPlayer : Scenario()->GetPlayers())
			pPlayer->LockedView().ViewLock(m_Pos, m_Smooth);
	}
};

class MessageComponent final : public Component<GroupScenarioBase, MessageComponent>, public IEventListener
{
	std::string m_Broadcast {};
	std::string m_Chat {};

public:
	explicit MessageComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Broadcast = j.value("broadcast", "");
		m_Chat = j.value("chat", "");
		if(j.contains("full"))
			m_Broadcast = m_Chat = j.value("full", "");
	}

private:
	void OnStartImpl() override
	{
		for(auto* pPlayer : Scenario()->GetPlayers())
		{
			if(!m_Chat.empty())
				GS()->Chat(pPlayer->GetCID(), m_Chat.c_str());
			if(!m_Broadcast.empty())
				GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::TitleInformation, m_DelayTick ? m_DelayTick : 100, m_Broadcast.c_str());
		}

		Finish();
	}
};

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


struct WaitComponent final : public Component<GroupScenarioBase, WaitComponent>
{
	int m_Duration;
	int m_TargetTick = 0;

	explicit WaitComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Duration = j.value("duration", 0);
	}

	void OnStartImpl() override
	{
		m_TargetTick = Server()->Tick() + Server()->TickSpeed() * m_Duration;
	}

	void OnActiveImpl() override
	{
		if(m_Duration <= 0 || Server()->Tick() >= m_TargetTick)
		{
			Finish();
		}
	}
};

struct MovementCondition final : public Component<GroupScenarioBase, MovementCondition>
{
	vec2 m_Position;
	bool m_EntireGroup;

	explicit MovementCondition(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Position = j.value("position", vec2());
		m_EntireGroup = j.value("entire_group", false);
	}

private:
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

		const bool ConditionMet = m_EntireGroup ?
			(!vpPlayers.empty() && std::all_of(vpPlayers.begin(), vpPlayers.end(), IsInsideFunc)) :
			std::any_of(vpPlayers.begin(), vpPlayers.end(), IsInsideFunc);
		if(ConditionMet)
		{
			Finish();
		}
	}
};


struct ActivatePointComponent final : public Component<GroupScenarioBase, ActivatePointComponent>
{
	vec2 m_Position {};
	bool m_EntireGroup {};
	int m_Duration {};
	int m_TargetTick {};
	int m_ActivationTick {};
	std::string m_Action {};

	explicit ActivatePointComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Position = j.value("position", vec2());
		m_EntireGroup = j.value("entire_group", false);
		m_Duration = j.value("duration", 0);
		m_Action = j.value("action_text", "Activating Point");
	}

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

CDungeonScenario::CDungeonScenario(const nlohmann::json& jsonData) : GroupScenarioBase(), m_JsonData(jsonData)
{
	// register components
	RegisterComponent<CDungeonScenario, DefeatMobsComponent>("defeat_mobs");
	RegisterComponent<CDungeonScenario, FollowCameraComponent>("follow_camera");
	RegisterComponent<CDungeonScenario, DoorControlComponent>("door_control");
	RegisterComponent<CDungeonScenario, UseChatComponent>("use_chat_code");
	RegisterComponent<CDungeonScenario, MessageComponent>("message");
	RegisterComponent<CDungeonScenario, MessageComponent>("broadcast");
	RegisterComponent<CDungeonScenario, WaitComponent>("wait");
	RegisterComponent<CDungeonScenario, ActivatePointComponent>("activate_point");
	RegisterComponent<CDungeonScenario, MovementCondition>("condition_movement");
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

	// add new step
	auto& newStep = AddStep(id, stepJson.value("msg_info", ""), stepJson.value("delay", -1));
	if(const auto logic = stepJson.value("completion_logic", "all_of"); logic == "any_of")
		newStep.m_CompletionLogic = StepCompletionLogic::ANY_OF;
	else if(logic == "sequential")
		newStep.m_CompletionLogic = StepCompletionLogic::SEQUENTIAL;

	// add components
	for(const auto& compJson : stepJson["components"])
	{
		const auto type = compJson.value("type", "");
		if(type.empty())
			continue;

		auto& componentsList = GetComponents();
		if(auto it = componentsList.find(type); it != componentsList.end())
			newStep.AddComponent(it->second(compJson));
	}
}