#ifndef GAME_SERVER_CORE_SCENARIOS_IMPL_COMPONENTS_DEFAULT_H
#define GAME_SERVER_CORE_SCENARIOS_IMPL_COMPONENTS_DEFAULT_H

#include <scenarios/base/scenario_base.h>
#include <scenarios/base/scenario_base_group.h>
#include <scenarios/base/scenario_base_player.h>
#include <game/server/core/tools/event_listener.h>
#include <game/server/core/balance/balance.h>
#include <game/server/gamecontext.h>

/**
 * @class PlayerAwareComponent
 * @brief A helper base class designed to encapsulate the logic for retrieving players.
 *
 * This class abstracts away the distinction between single-player (PlayerScenarioBase)
 * and group (GroupScenarioBase) scenarios. It provides a unified GetPlayers() method,
 * eliminating redundant code in components that need to interact with scenario participants.
 */
template<typename T>
class PlayerAwareComponent : public Component<ScenarioBase, T>
{
protected:
	std::vector<CPlayer*> GetPlayers() const
	{
		std::vector<CPlayer*> vpPlayers;
		ScenarioBase* pBaseScenario = this->Scenario();

		if(const auto* pGroupScenario = dynamic_cast<const GroupScenarioBase*>(pBaseScenario))
		{
			vpPlayers = pGroupScenario->GetPlayers();
		}
		else if(const auto* pPlayerScenario = dynamic_cast<const PlayerScenarioBase*>(pBaseScenario))
		{
			if(CPlayer* pPlayer = pPlayerScenario->GetPlayer())
				vpPlayers.push_back(pPlayer);
		}
		return vpPlayers;
	}

	void ForEachScenarioPlayer(const std::function<void(CPlayer*)>& Func) const
	{
		ScenarioBase* pBaseScenario = this->Scenario();
		if(const auto* pGroupScenario = dynamic_cast<const GroupScenarioBase*>(pBaseScenario))
		{
			for(auto* pPlayer : pGroupScenario->GetPlayers())
			{
				if(pPlayer)
					Func(pPlayer);
			}
		}
		else if(const auto* pPlayerScenario = dynamic_cast<const PlayerScenarioBase*>(pBaseScenario))
		{
			if(auto* pPlayer = pPlayerScenario->GetPlayer())
				Func(pPlayer);
		}
	}
};

/**
 * @class ScenarioMessageComponent
 * @brief A component responsible for sending messages to scenario participants.
 *
 * It can deliver both standard in-game chat messages and on-screen "broadcast" messages.
 * The component is configured via a JSON object with the following fields:
 * @param mode (string): Determines where the message is sent. Can be "chat", "broadcast", or "full".
 * @param text (string): The content of the message to be sent.
 *
 * The component executes its logic immediately upon starting and then finishes.
 */
class ScenarioMessageComponent final : public PlayerAwareComponent<ScenarioMessageComponent>
{
	std::string m_Broadcast {};
	std::string m_Chat {};

public:
	explicit ScenarioMessageComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);

		auto modeStr = j.value("mode", "");
		if(modeStr == "broadcast" || modeStr == "full")
			m_Broadcast = j.value("text", "");
		if(modeStr == "chat" || modeStr == "full")
			m_Chat = j.value("text", "");
	}

    DECLARE_COMPONENT_NAME("message")

private:
	void OnStartImpl() override
	{
		for(const auto* pPlayer : GetPlayers())
		{
			if(!m_Chat.empty())
			{
				GS()->Chat(pPlayer->GetCID(), m_Chat.c_str());
			}

			if(!m_Broadcast.empty())
			{
				int Time = std::max(GetExecutionTimeTick(), Server()->TickSpeed());
				GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::TitleInformation, Time, m_Broadcast.c_str());
			}
		}

		Finish();
	}
};

/**
 * @class ScenarioFollowCameraComponent
 * @brief A component that locks the camera of all participating players onto a specific world position.
 *
 * This is useful for creating cutscenes or directing player focus.
 * The component is configured via a JSON object with the following fields:
 * @param position (vec2): The world coordinate where the camera will be locked.
 * @param execution_time (int): The duration, in game ticks, for which the camera lock will be active.
 * @param smooth (bool): If true, the camera will move smoothly to the target position.
 *
 * During its active phase, it continuously applies the camera lock. Upon completion, it
 * automatically releases the lock, restoring normal camera control to the players.
 */
class ScenarioFollowCameraComponent final : public PlayerAwareComponent<ScenarioFollowCameraComponent>
{
	vec2 m_Pos {};
	bool m_Smooth {};

public:
	explicit ScenarioFollowCameraComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Pos = j.value("position", vec2());
		m_Smooth = j.value("smooth", true);
	}

    DECLARE_COMPONENT_NAME("follow_camera")

private:
	void OnActiveImpl() override
	{
		if(GetExecutionTimeTick() <= 0)
		{
			Finish();
			return;
		}

		for(auto* pPlayer : GetPlayers())
			pPlayer->LockedView().ViewLock(m_Pos, m_Smooth);
	}

	void OnEndImpl() override
	{
		for(auto* pPlayer : GetPlayers())
			pPlayer->LockedView().Reset();
	}
};

/**
 * @class ScenarioTeleportComponent
 * @brief A component that teleports all participating players to a new position.
 *
 * The component waits until all players have an active character before performing the teleport.
 * The component is configured via a JSON object with the following field:
 * @param position (vec2): The world coordinate to which players will be teleported.
 */
class ScenarioTeleportComponent final : public PlayerAwareComponent<ScenarioTeleportComponent>
{
	vec2 m_NewPos {};

public:
	explicit ScenarioTeleportComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_NewPos = j.value("position", vec2());
	}

    DECLARE_COMPONENT_NAME("teleport")

private:
	void OnActiveImpl() override
	{
		const auto vpPlayers = GetPlayers();
		for(const auto* pPlayer : vpPlayers)
		{
			if(!pPlayer || !pPlayer->GetCharacter())
				return;
		}

		// all players are ready, perform teleport
		for(auto* pPlayer : vpPlayers)
			pPlayer->GetCharacter()->ChangePosition(m_NewPos);

		Finish();
	}
};

/**
 * @class ScenarioWaitComponent
 * @brief A simple component that pauses a scenario's progression for a specified duration.
 *
 * Its sole purpose is to delay the completion of a step without performing any
 * actions on players or game objects.
 * The component is configured via a JSON object with the following field:
 * @param duration (int): The duration of the wait, in seconds.
 */
class ScenarioWaitComponent final : public PlayerAwareComponent<ScenarioWaitComponent>
{
	int m_Duration;
	int m_TargetTick = 0;

public:
	explicit ScenarioWaitComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Duration = j.value("duration", 0);
	}

    DECLARE_COMPONENT_NAME("wait")

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

/**
 * @class ScenarioBranchRandomComponent
 * @brief Random branch node (blueprint-style branch with chance).
 *
 * JSON fields:
 * @param chance_percent (int): chance [0..100] to take true_step_id.
 * @param true_step_id (string): step id when random check passes.
 * @param false_step_id (string): step id when random check fails.
 */
class ScenarioBranchRandomComponent final : public PlayerAwareComponent<ScenarioBranchRandomComponent>
{
	int m_ChancePercent {};
	StepId m_TrueStepId {};
	StepId m_FalseStepId {};

public:
	explicit ScenarioBranchRandomComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_ChancePercent = clamp(j.value("chance_percent", 50), 0, 100);
		m_TrueStepId = j.value("true_step_id", StepId {});
		m_FalseStepId = j.value("false_step_id", StepId {});
	}

	DECLARE_COMPONENT_NAME("branch_random")

private:
	void OnStartImpl() override
	{
		const int roll = rand() % 100;
		const bool passed = roll < m_ChancePercent;
		m_NextStepId = passed ? m_TrueStepId : m_FalseStepId;
		Finish();
	}
};



/**
 * @class ScenarioDynamicConditionComponent
 *
 * JSON fields:
 * @param condition_type (string): players_count, alive_players_count, item_count, quest_state, world_id.
 * @param compare (string): ==, !=, >=, <=, >, <.
 * @param value (int): right-hand value for numeric conditions.
 * @param item_id (int): item id for item_count.
 * @param item_scope (string): any_player, all_players, total for item_count.
 * @param quest_id (int), quest_state (string), step (int), entire_group (bool): quest condition fields.
 * @param invert (bool): invert final result.
 */
class ScenarioDynamicConditionComponent final : public PlayerAwareComponent<ScenarioDynamicConditionComponent>
{
	enum class EConditionType
	{
		PlayersCount,
		AlivePlayersCount,
		ItemCount,
		QuestState,
	};

	enum class ECompare
	{
		Equal,
		NotEqual,
		Greater,
		GreaterOrEqual,
		Less,
		LessOrEqual,
	};

	enum class EItemScope
	{
		AnyPlayer,
		AllPlayers,
		Total,
	};

	enum class EQuestState
	{
		Accepted,
		Finished,
		StepFinished,
	};

	EConditionType m_ConditionType { EConditionType::PlayersCount };
	ECompare m_Compare { ECompare::GreaterOrEqual };
	EItemScope m_ItemScope { EItemScope::AnyPlayer };
	EQuestState m_QuestState { EQuestState::Accepted };
	int m_Value { 1 };
	int m_ItemID { 0 };
	int m_QuestID { -1 };
	int m_Step { 0 };
	bool m_EntireGroup { false };
	bool m_Invert { false };
	bool m_ShowProgress { true };

	static EConditionType ParseConditionType(const std::string& Type)
	{
		if(Type == "alive_players_count")
			return EConditionType::AlivePlayersCount;
		if(Type == "item_count")
			return EConditionType::ItemCount;
		if(Type == "quest_state")
			return EConditionType::QuestState;
		return EConditionType::PlayersCount;
	}

	static ECompare ParseCompare(const std::string& Compare)
	{
		if(Compare == "!=")
			return ECompare::NotEqual;
		if(Compare == ">")
			return ECompare::Greater;
		if(Compare == "<")
			return ECompare::Less;
		if(Compare == "<=")
			return ECompare::LessOrEqual;
		if(Compare == "==" || Compare == "=")
			return ECompare::Equal;
		return ECompare::GreaterOrEqual;
	}

	static EItemScope ParseItemScope(const std::string& Scope)
	{
		if(Scope == "all_players")
			return EItemScope::AllPlayers;
		if(Scope == "total")
			return EItemScope::Total;
		return EItemScope::AnyPlayer;
	}

	static EQuestState ParseQuestState(const std::string& State)
	{
		if(State == "finished")
			return EQuestState::Finished;
		if(State == "step_finished")
			return EQuestState::StepFinished;
		return EQuestState::Accepted;
	}

	bool CompareInt(int Left, int Right) const
	{
		switch(m_Compare)
		{
			case ECompare::Equal: return Left == Right;
			case ECompare::NotEqual: return Left != Right;
			case ECompare::Greater: return Left > Right;
			case ECompare::GreaterOrEqual: return Left >= Right;
			case ECompare::Less: return Left < Right;
			case ECompare::LessOrEqual: return Left <= Right;
		}
		return false;
	}

	int GetPlayerItemCount(CPlayer* pPlayer) const
	{
		if(!pPlayer || m_ItemID <= 0)
			return 0;

		const auto* pItem = pPlayer->GetItem(m_ItemID);
		return pItem ? pItem->GetValue() : 0;
	}

	bool CheckQuest(CPlayer* pPlayer) const
	{
		if(!pPlayer || m_QuestID <= 0)
			return false;

		const auto* pQuest = pPlayer->GetQuest(m_QuestID);
		if(!pQuest)
			return false;

		switch(m_QuestState)
		{
			case EQuestState::Accepted: return pQuest->IsAccepted();
			case EQuestState::Finished: return pQuest->IsCompleted();
			case EQuestState::StepFinished: return pQuest->GetStepPos() > m_Step;
		}
		return false;
	}

	bool EvaluatePlayersCount(const std::vector<CPlayer*>& vpPlayers) const
	{
		return CompareInt(static_cast<int>(vpPlayers.size()), m_Value);
	}

	bool EvaluateAlivePlayersCount(const std::vector<CPlayer*>& vpPlayers) const
	{
		const int AliveCount = static_cast<int>(std::count_if(vpPlayers.begin(), vpPlayers.end(), [](const CPlayer* pPlayer)
		{
			return pPlayer && pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive();
		}));
		return CompareInt(AliveCount, m_Value);
	}

	bool EvaluateItemCount(const std::vector<CPlayer*>& vpPlayers) const
	{
		if(vpPlayers.empty() || m_ItemID <= 0)
			return false;

		if(m_ItemScope == EItemScope::Total)
		{
			int Total = 0;
			for(auto* pPlayer : vpPlayers)
				Total += GetPlayerItemCount(pPlayer);
			return CompareInt(Total, m_Value);
		}

		auto HasRequiredItems = [this](CPlayer* pPlayer)
		{
			return CompareInt(GetPlayerItemCount(pPlayer), m_Value);
		};

		if(m_ItemScope == EItemScope::AllPlayers)
			return std::all_of(vpPlayers.begin(), vpPlayers.end(), HasRequiredItems);

		return std::any_of(vpPlayers.begin(), vpPlayers.end(), HasRequiredItems);
	}

	bool EvaluateQuestState(const std::vector<CPlayer*>& vpPlayers) const
	{
		if(vpPlayers.empty() || m_QuestID <= 0)
			return false;

		if(m_EntireGroup)
			return std::all_of(vpPlayers.begin(), vpPlayers.end(), [this](CPlayer* pPlayer) { return CheckQuest(pPlayer); });

		return std::any_of(vpPlayers.begin(), vpPlayers.end(), [this](CPlayer* pPlayer) { return CheckQuest(pPlayer); });
	}

	bool Evaluate() const
	{
		const auto vpPlayers = GetPlayers();
		bool Result = false;
		switch(m_ConditionType)
		{
			case EConditionType::PlayersCount: Result = EvaluatePlayersCount(vpPlayers); break;
			case EConditionType::AlivePlayersCount: Result = EvaluateAlivePlayersCount(vpPlayers); break;
			case EConditionType::ItemCount: Result = EvaluateItemCount(vpPlayers); break;
			case EConditionType::QuestState: Result = EvaluateQuestState(vpPlayers); break;
		}
		return m_Invert ? !Result : Result;
	}

public:
	explicit ScenarioDynamicConditionComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_ConditionType = ParseConditionType(j.value("condition_type", "players_count"));
		m_Compare = ParseCompare(j.value("compare", ">="));
		m_ItemScope = ParseItemScope(j.value("item_scope", "any_player"));
		m_QuestState = ParseQuestState(j.value("quest_state", j.value("condition", "accepted")));
		m_Value = j.value("value", j.value("required", 1));
		m_ItemID = j.value("item_id", 0);
		m_QuestID = j.value("quest_id", -1);
		m_Step = j.value("step", 0);
		m_EntireGroup = j.value("entire_group", false);
		m_Invert = j.value("invert", false);
		m_ShowProgress = j.value("show_progress", true);
	}

	DECLARE_COMPONENT_NAME("dynamic_condition")

private:
	void OnActiveImpl() override
	{
		if(Evaluate())
		{
			Finish();
			return;
		}

		if(m_ShowProgress && Server() && Server()->Tick() % Server()->TickSpeed() == 0)
		{
			ForEachScenarioPlayer([this](CPlayer* pPlayer)
			{
				GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameWarning, Server()->TickSpeed(), "Objective: condition is not met yet.");
			});
		}
	}
};

/**
 * @class ScenarioMovementConditionComponent
 * @brief A conditional component that finishes only when participants have moved to a designated area.
 *
 * This is used to create objectives where players must reach a specific location.
 * The component is configured via a JSON object with the following fields:
 * @param position (vec2): The center of the target destination area.
 * @param entire_group (bool): If true, the component waits for *all* players to be in the area.
 * If false, it completes as soon as *any* single player enters the area.
 */
class ScenarioMovementConditionComponent final : public PlayerAwareComponent<ScenarioMovementConditionComponent>
{
	vec2 m_Position;
	bool m_EntireGroup;

public:
	explicit ScenarioMovementConditionComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Position = j.value("position", vec2());
		m_EntireGroup = j.value("entire_group", false);
	}

    DECLARE_COMPONENT_NAME("condition_movement")

private:
	void OnActiveImpl() override
	{
		const auto vpPlayers = GetPlayers();
		if(vpPlayers.empty())
			return;

		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->CreateHammerHit(m_Position, Scenario()->GetClientsMask());
		}

		auto IsInsideFunc = [&](const CPlayer* pPlayer)
		{
			const auto Dist = m_EntireGroup ? 256.0f : 128.f;
			const auto* pChr = pPlayer->GetCharacter();
			return pChr && distance(pChr->GetPos(), m_Position) < Dist;
		};

		bool ConditionMet = false;
		if(m_EntireGroup)
		{
			const size_t NumPlayersInZone = std::count_if(vpPlayers.begin(), vpPlayers.end(), IsInsideFunc);
			const size_t TotalPlayers = vpPlayers.size();
			if(Server()->Tick() % Server()->TickSpeed() == 0)
			{
				for(const auto* pPlayer : vpPlayers)
				{
					GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameWarning, Server()->TickSpeed(),
						"Move to the designated area! ({}/{})", NumPlayersInZone, TotalPlayers);
				}
			}

			ConditionMet = (NumPlayersInZone == TotalPlayers);
		}
		else
		{
			ConditionMet = std::any_of(vpPlayers.begin(), vpPlayers.end(), IsInsideFunc);
		}

		if(ConditionMet)
			Finish();
	}
};

/**
 * @class ScenarioMovingDisableComponent
 * @brief A component that enables or disables movement for all participants.
 *
 * The component waits until all players have an active character before applying the state.
 * The component is configured via a JSON object with the following field:
 * @param state (bool): True disables movement, false enables it.
 */
class ScenarioMovingDisableComponent final : public PlayerAwareComponent<ScenarioMovingDisableComponent>
{
	bool m_State {};

public:
	explicit ScenarioMovingDisableComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_State = j.value("state", true);
	}

	DECLARE_COMPONENT_NAME("moving_disable")

private:
	void OnActiveImpl() override
	{
		const auto vpPlayers = GetPlayers();
		for(auto* pPlayer : vpPlayers)
		{
			if(!pPlayer || !pPlayer->GetCharacter())
				continue;

			pPlayer->GetCharacter()->MovingDisable(m_State);
		}

		Finish();
	}
};

class ScenarioQuestActionComponent final : public PlayerAwareComponent<ScenarioQuestActionComponent>
{
	enum class Action { Reset, Accept };
	int m_QuestID {};
	Action m_Action {};

public:
	explicit ScenarioQuestActionComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_QuestID = j.value("quest_id", -1);
		m_Action = j.value("action", "reset") == "accept" ? Action::Accept : Action::Reset;
	}

	DECLARE_COMPONENT_NAME("quest_action")

private:
	void OnStartImpl() override
	{
		if(m_QuestID <= 0)
		{
			Finish();
			return;
		}

		for(auto* pPlayer : GetPlayers())
		{
			if(!pPlayer)
				continue;

			auto* pQuest = pPlayer->GetQuest(m_QuestID);
			if(!pQuest)
				continue;

			if(m_Action == Action::Reset)
				pQuest->Reset();
			else
			{
				if(pQuest->IsAccepted())
					pQuest->Reset();
				pQuest->Accept();
			}
		}

		Finish();
	}
};

class ScenarioQuestConditionComponent final : public PlayerAwareComponent<ScenarioQuestConditionComponent>
{
	enum class Condition { Accepted, Finished, StepFinished };
	int m_QuestID {};
	int m_Step {};
	Condition m_Condition {};
	bool m_EntireGroup {};

public:
	explicit ScenarioQuestConditionComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_QuestID = j.value("quest_id", -1);
		m_Step = j.value("step", -1);
		m_EntireGroup = j.value("entire_group", false);
		const auto type = j.value("condition", "accepted");
		if(type == "finished")
			m_Condition = Condition::Finished;
		else if(type == "step_finished")
			m_Condition = Condition::StepFinished;
		else
			m_Condition = Condition::Accepted;
	}

	DECLARE_COMPONENT_NAME("quest_condition")

private:
	void OnActiveImpl() override
	{
		if(m_QuestID <= 0)
			return;

		const auto vpPlayers = GetPlayers();
		if(vpPlayers.empty())
			return;

		auto IsConditionComplete = [&](CPlayer* pPlayer) -> bool
		{
			if(!pPlayer)
				return false;

			auto* pQuest = pPlayer->GetQuest(m_QuestID);
			if(!pQuest)
				return false;

			switch(m_Condition)
			{
				case Condition::Accepted: return pQuest->IsAccepted();
				case Condition::Finished: return pQuest->IsCompleted();
				case Condition::StepFinished: return pQuest->GetStepPos() > m_Step;
			}

			return false;
		};

		const bool complete = m_EntireGroup
			? std::all_of(vpPlayers.begin(), vpPlayers.end(), IsConditionComplete)
			: std::any_of(vpPlayers.begin(), vpPlayers.end(), IsConditionComplete);

		if(complete)
			Finish();
	}
};

/**
 * @class ScenarioEmoteComponent
 * @brief A component that plays an emote and optional emoticon for all participants.
 *
 * The component waits until all players have an active character before performing the emote.
 * The component is configured via a JSON object with the following fields:
 * @param emote_type (int): Character emote type (defaults to EMOTE_NORMAL).
 * @param emoticon_type (int): Emoticon type to send (-1 disables).
 */
class ScenarioEmoteComponent final : public PlayerAwareComponent<ScenarioEmoteComponent>
{
	int m_EmoteType {};
	int m_EmoticonType {};

public:
	explicit ScenarioEmoteComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_EmoteType = j.value("emote_type", (int)EMOTE_NORMAL);
		m_EmoticonType = j.value("emoticon_type", -1);
	}

	DECLARE_COMPONENT_NAME("emote")

private:
	void OnStartImpl() override
	{
		const auto vpPlayers = GetPlayers();
		for(auto* pPlayer : vpPlayers)
		{
			if(!pPlayer || !pPlayer->GetCharacter())
				continue;

			pPlayer->GetCharacter()->SetEmote(m_EmoteType, 1, false);
			if(m_EmoticonType >= 0)
				GS()->SendEmoticon(pPlayer->GetCID(), m_EmoticonType);
		}

		Finish();
	}
};

/**
 * @class ScenarioDefeatMobsComponent
 * @brief A component that spawns mobs and completes once defeat conditions are met.
 *
 * The component is configured via a JSON object with the following fields:
 * @param mode (string): "annihilation", "wave", or "survival".
 * @param mobs (array): List of mob definitions with bot_id, count, level, power.
 * @param position (vec2): Center position for mob spawn.
 * @param radius (float): Spawn radius.
 * @param kill_target (int): Target kills for "wave" mode (defaults to number spawned).
 * @param duration (int): Duration in seconds for "survival" mode.
 */
class ScenarioDefeatMobsComponent final : public PlayerAwareComponent<ScenarioDefeatMobsComponent>, public IEventListener
{
	enum class EMode { Annihilation, Wave, Survival };
	static EMode GetMode(std::string_view modeStr)
	{
		if(modeStr == "wave")
			return EMode::Wave;
		if(modeStr == "survival")
			return EMode::Survival;
		return EMode::Annihilation;
	}

	ScopedEventListener m_ListenerScope {};
	int m_TargetKills {};
	int m_Duration {};
	std::unordered_set<int> m_SpawnedBotIds {};
	std::unordered_set<int> m_AllSpawnedBotIds {};
	EMode m_Mode {};
	vec2 m_Position {};
	float m_Radius {};
	float m_ActiveRadius {};
	int m_KillsMade {};
	int m_TargetTick {};
	nlohmann::json m_MobsData {};
	static inline std::atomic<int> ms_NextReservedMobID { -1 };

public:
	explicit ScenarioDefeatMobsComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Mode = GetMode(j.value("mode", ""));
		m_Radius = j.value("radius", 180.f);
		m_Position = j.value("position", vec2 {});
		m_MobsData = j.value("mobs", nlohmann::json::array());
		m_TargetKills = j.value("kill_target", (int)NOPE);
		m_Duration = j.value("duration", 0);
		m_ActiveRadius = j.value("activeradius", (float)g_Config.m_SvMapDistanceActveBot);
		m_ActiveRadius = m_ActiveRadius > 1.f ? m_ActiveRadius : g_Config.m_SvMapDistanceActveBot;
		m_ListenerScope.Init(this, IEventListener::CharacterDeath);
	}

	DECLARE_COMPONENT_NAME("defeat_mobs")

private:
	void Reset()
	{
		m_KillsMade = 0;
		m_TargetTick = Server()->Tick() + m_Duration * Server()->TickSpeed();
		m_SpawnedBotIds.clear();
		m_AllSpawnedBotIds.clear();
	}

	void OnStartImpl() override
	{
		m_ListenerScope.Register();
		Reset();

		const auto vpPlayers = GetPlayers();
		const bool IsGroupScenario = dynamic_cast<GroupScenarioBase*>(Scenario()) != nullptr;
		for(const auto& mobData : m_MobsData)
		{
			const int botID = mobData.value("bot_id", (int)NOPE);
			if(!DataBotInfo::IsDataBotValid(botID))
				continue;

			const int spawnMobID = ms_NextReservedMobID.fetch_sub(1);
			MobBotInfo mobInfo {};
			mobInfo.m_BotID = botID;

			mobInfo.m_Level = mobData.value("level", 1);
			mobInfo.m_Power = Balance::Get().CalculateScenarioMobPower(vpPlayers, mobData.value("power", 1), IsGroupScenario);
			mobInfo.m_Boss = mobData.value("boss", false);
			mobInfo.m_Position = m_Position;
			mobInfo.m_Radius = m_Radius;
			mobInfo.m_ActiveRadius = m_ActiveRadius;
			mobInfo.m_WorldID = GS()->GetWorldID();

			// initialize behavior
			if(const auto behaviorJson = mobData.value("Behavior", nlohmann::json::array()); behaviorJson.is_array())
			{
				std::string behaviorSet {};
				for(const auto& entry : behaviorJson)
				{
					if(!entry.is_string())
						continue;
					if(!behaviorSet.empty())
						behaviorSet.append(",");
					behaviorSet.append(entry.get<std::string>());
				}
				if(!behaviorSet.empty())
					mobInfo.InitBehaviors(DBSet(behaviorSet));
			}

			// initialize debuffs
			if(const auto debuffsJson = mobData.value("Debuffs", nlohmann::json::array()); debuffsJson.is_array())
			{
				std::string debuffSet {};
				for(const auto& entry : debuffsJson)
				{
					if(!entry.is_string())
						continue;
					if(!debuffSet.empty())
						debuffSet.append(",");
					debuffSet.append(entry.get<std::string>());
				}
				if(!debuffSet.empty())
					mobInfo.InitDebuffs(5, 5, 5.0f, DBSet(debuffSet));
			}

			// initialize drops
			if(const auto dropsJson = mobData.value("drops", nlohmann::json::array()); dropsJson.is_array())
			{
				for(int i = 0; i < MAX_DROPPED_FROM_MOBS; ++i)
				{
					mobInfo.m_aDropItem[i] = 0;
					mobInfo.m_aValueItem[i] = 0;
					mobInfo.m_aRandomItem[i] = 0.f;
				}

				int slot = 0;
				for(const auto& dropData : dropsJson)
				{
					if(slot >= MAX_DROPPED_FROM_MOBS)
						break;

					const int itemID = dropData.value("item_id", 0);
					if(itemID <= 0)
						continue;

					mobInfo.m_aDropItem[slot] = itemID;
					mobInfo.m_aValueItem[slot] = maximum(1, dropData.value("count", 1));
					mobInfo.m_aRandomItem[slot] = maximum(0.0f, dropData.value("chance", 0.0f));
					++slot;
				}
			}

			// amount
			for(int i = 0; i < mobData.value("count", 1); ++i)
			{
				if(auto* pPlayerBot = GS()->CreateBot(TYPE_BOT_MOB, mobInfo.m_BotID, spawnMobID))
				{
					pPlayerBot->InitBotMobInfo(mobInfo);
					m_SpawnedBotIds.insert(pPlayerBot->GetCID());
					m_AllSpawnedBotIds.insert(pPlayerBot->GetCID());
				}
			}
		}

		if(m_Mode == EMode::Wave && m_TargetKills <= 0)
			m_TargetKills = static_cast<int>(m_SpawnedBotIds.size());
	}

	void DestroyScenarioMob(int ClientID, bool KillCharacter) const
	{
		if(auto* pPlayer = GS()->GetPlayer(ClientID))
		{
			pPlayer->m_WantSpawn = false;
			if(KillCharacter && pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
				pPlayer->KillCharacter(WEAPON_WORLD);
			pPlayer->m_WantSpawn = false;
			pPlayer->MarkForDestroy();
		}
	}

	void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) override
	{
		if(!pVictim || !m_SpawnedBotIds.contains(pVictim->GetCID()))
			return;

		if(m_Mode == EMode::Annihilation)
		{
			m_SpawnedBotIds.erase(pVictim->GetCID());
			DestroyScenarioMob(pVictim->GetCID(), false);
		}
		else if(m_Mode == EMode::Wave)
		{
			++m_KillsMade;
		}
	}

	void OnActiveImpl() override
	{
		const int tickSpeed = Server()->TickSpeed();
		const bool shouldBroadcast = (Server()->Tick() % tickSpeed == 0);
		bool shouldFinish = false;

		switch(m_Mode)
		{
			default:
				if(shouldBroadcast)
				{
					for(auto* pPlayer : GetPlayers())
					{
						GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameWarning, tickSpeed,
							"Objective: Defeat all the mobs. Remaining: {} mobs.", m_SpawnedBotIds.size());
					}
				}

				shouldFinish = m_SpawnedBotIds.empty();
				break;

			case EMode::Wave:
				if(shouldBroadcast)
				{
					for(auto* pPlayer : GetPlayers())
					{
						GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameWarning, tickSpeed,
							"Objective: Defeat wave mobs '{} of {}'", m_KillsMade, m_TargetKills);
					}
				}

				shouldFinish = (m_TargetKills <= 0) || (m_KillsMade >= m_TargetKills);
				break;

			case EMode::Survival:
			{
				const int timeLeft = (m_TargetTick - Server()->Tick()) / tickSpeed;
				if(shouldBroadcast && timeLeft >= 0)
				{
					for(auto* pPlayer : GetPlayers())
					{
						GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameWarning, tickSpeed,
							"Objective: Survive! Time left: {}s", timeLeft);
					}
				}

				shouldFinish = Server()->Tick() >= m_TargetTick;
				break;
			}
		}

		if(shouldFinish)
			Finish();
	}

	void OnEndImpl() override
	{
		m_ListenerScope.Unregister();
		for(int CID : m_AllSpawnedBotIds)
			DestroyScenarioMob(CID, true);

		m_SpawnedBotIds.clear();
		m_AllSpawnedBotIds.clear();
	}
};
/**
 * @class ScenarioUseChatComponent
 * @brief A component that requires players to write a specific message in chat to complete the scenario.
 *
 * The component is configured via a JSON object with the following fields:
 * @param code (string): The exact chat message that must be written by the player(s) to complete the objective.
 * @param hidden (bool): If true, the objective message will not be broadcast to players.
 *
 * Behavior:
 * - Periodically broadcasts the objective text to players (if not hidden).
 * - Listens for player chat events.
 * - Completes the scenario when a player writes the exact required message.
 *
 * Scope:
 * - Supports both group scenarios (all players in group) and single-player scenarios.
 */
class ScenarioUseChatComponent final : public PlayerAwareComponent<ScenarioUseChatComponent>, public IEventListener
{
	ScopedEventListener m_ListenerScope {};
	std::string m_ChatCode {};
	bool m_Hidden {};

public:
	explicit ScenarioUseChatComponent(const nlohmann::json& j)
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

		// send broadcast information
		ForEachScenarioPlayer([this](CPlayer* pPlayer)
		{
			GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameAlert, Server()->TickSpeed(), "Objective: Write in the chat '{}'", m_ChatCode);
		});
	}

	void OnPlayerChat(CPlayer* pFrom, const char* pMessage) override
	{
		if(std::string_view(pMessage) != m_ChatCode)
			return;

		// check valid player
		bool ValidPlayer = false;
		ForEachScenarioPlayer([&](CPlayer* pPlayer)
		{
			if(pPlayer->GetCID() == pFrom->GetCID())
				ValidPlayer = true;
		});

		if(ValidPlayer)
			Finish();
	}

	void OnEndImpl() override
	{
		m_ListenerScope.Unregister();
	}
};


/*
***************************** DEFAULT GROUP
*/
class ScenarioGroupFlagsComponent final : public PlayerAwareComponent<ScenarioGroupFlagsComponent>
{
	int m_FlagsToApply { GroupScenarioBase::SCENARIOFLAG_NONE };

public:
	explicit ScenarioGroupFlagsComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		if(j.value("disable_group_damage", false))
			m_FlagsToApply |= GroupScenarioBase::SCENARIOFLAG_DISABLE_GROUP_DAMAGE;
		if(j.value("disable_group_collision", false))
			m_FlagsToApply |= GroupScenarioBase::SCENARIOFLAG_DISABLE_GROUP_COLLISION;
		if(j.value("disable_group_hooking", false))
			m_FlagsToApply |= GroupScenarioBase::SCENARIOFLAG_DISABLE_GROUP_HOOKING;
		if(j.value("disable_group_pvp", false))
			m_FlagsToApply |= GroupScenarioBase::SCENARIOFLAG_DISABLE_GROUP_PVP;
	}

	DECLARE_COMPONENT_NAME("set_group_flags")

private:
	void OnStartImpl() override
	{
		if(auto* pGroupScenario = dynamic_cast<GroupScenarioBase*>(Scenario()))
			pGroupScenario->SetScenarioFlags(m_FlagsToApply);
		Finish();
	}
};

class ScenarioGroupLivesComponent final : public PlayerAwareComponent<ScenarioGroupLivesComponent>
{
	enum class EAction { Set, Add, Subtract };
	EAction m_Action { EAction::Set };
	int m_Value {};

public:
	explicit ScenarioGroupLivesComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		const auto action = j.value("action", "set");
		m_Value = j.value("value", 0);
		if(action == "add")
			m_Action = EAction::Add;
		else if(action == "sub" || action == "subtract")
			m_Action = EAction::Subtract;
	}

	DECLARE_COMPONENT_NAME("set_group_lives")

private:
	void OnStartImpl() override
	{
		if(auto* pGroupScenario = dynamic_cast<GroupScenarioBase*>(Scenario()))
		{
			if(m_Action == EAction::Set)
				pGroupScenario->SetGroupLives(m_Value);
			else if(m_Action == EAction::Add)
				pGroupScenario->AddGroupLives(m_Value);
			else
				pGroupScenario->AddGroupLives(-m_Value);
		}

		Finish();
	}
};

class ScenarioGroupSpawnComponent final : public PlayerAwareComponent<ScenarioGroupSpawnComponent>
{
	bool m_EnableSpawn {};
	vec2 m_GroupSpawnPos {};

public:
	explicit ScenarioGroupSpawnComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_GroupSpawnPos = j.value("position", vec2 {});
		m_EnableSpawn = j.value("enable_spawn", true);
	}

	DECLARE_COMPONENT_NAME("set_group_spawn")

private:
	void OnStartImpl() override
	{
		if(auto* pGroupScenario = dynamic_cast<GroupScenarioBase*>(Scenario()))
		{
			if(m_EnableSpawn)
				pGroupScenario->SetGroupSpawnPos(m_GroupSpawnPos);
			else
				pGroupScenario->ResetGroupSpawnPos();
		}

		Finish();
	}
};

class ScenarioRestoreResourcesComponent final : public PlayerAwareComponent<ScenarioRestoreResourcesComponent>
{
	bool m_RestoreHealth {};
	bool m_RestoreMana {};
	bool m_RestoreAmmo {};

public:
	explicit ScenarioRestoreResourcesComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_RestoreHealth = j.value("restore_hp", true);
		m_RestoreMana = j.value("restore_mp", true);
		m_RestoreAmmo = j.value("restore_ammo", true);
	}

	DECLARE_COMPONENT_NAME("restore_resources")

private:
	void OnStartImpl() override
	{
		ForEachScenarioPlayer([this](CPlayer* pPlayer)
		{
			CCharacter* pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(pChr)
			{
				if(m_RestoreHealth)
					pChr->IncreaseHealth(pPlayer->GetMaxHealth());

				if(m_RestoreMana)
					pChr->IncreaseMana(pPlayer->GetMaxMana());

				if(m_RestoreAmmo)
				{
					const int MaxAmmo = 10 + pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
					for(int Weapon = WEAPON_GUN; Weapon < NUM_WEAPONS; Weapon++)
						pChr->GiveWeapon(Weapon, MaxAmmo);
				}
			}
		});

		Finish();
	}
};

#endif
