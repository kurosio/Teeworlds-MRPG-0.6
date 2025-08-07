#ifndef GAME_SERVER_CORE_SCENARIOS_IMPL_COMPONENTS_DEFAULT_H
#define GAME_SERVER_CORE_SCENARIOS_IMPL_COMPONENTS_DEFAULT_H

#include <scenarios/base/scenario_base.h>
#include <scenarios/base/scenario_base_group.h>
#include <scenarios/base/scenario_base_player.h>
#include <game/server/core/tools/event_listener.h>
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
				GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::TitleInformation, Server()->TickSpeed(), m_Broadcast.c_str());
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
 * @param delay (int): The duration, in game ticks, for which the camera lock will be active.
 * @param smooth (bool): If true, the camera will move smoothly to the target position.
 *
 * During its active phase, it continuously applies the camera lock. Upon completion, it
 * automatically releases the lock, restoring normal camera control to the players.
 */
class ScenarioFollowCameraComponent final : public PlayerAwareComponent<ScenarioFollowCameraComponent>
{
	vec2 m_Pos {};
	int m_Delay {};
	bool m_Smooth {};

public:
	explicit ScenarioFollowCameraComponent(const nlohmann::json& j)
	{
		InitBaseJsonField(j);
		m_Pos = j.value("position", vec2());
		m_Delay = j.value("delay", 200);
		m_Smooth = j.value("smooth", true);
	}

    DECLARE_COMPONENT_NAME("follow_camera")

private:
	void OnActiveImpl() override
	{
		m_Delay--;
		if(m_Delay <= 0)
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
class ScenarioWaitComponent final : public Component<ScenarioBase, ScenarioWaitComponent>
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
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
			GS()->CreateHammerHit(m_Position);

		const auto vpPlayers = GetPlayers();
		if(vpPlayers.empty())
			return;

		auto IsInsideFunc = [&](const CPlayer* pPlayer)
		{
			const auto* pChr = pPlayer->GetCharacter();
			return pChr && distance(pChr->GetPos(), m_Position) < 128.f;
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
					GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::TitleInformation, Server()->TickSpeed(),
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
		{
			Finish();
		}
	}
};

#endif