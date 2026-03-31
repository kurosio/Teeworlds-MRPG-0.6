#include "scenario_universal.h"

#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/drop_items.h>
#include <game/server/core/entities/group/entitiy_group.h>
#include <game/server/entities/projectile.h>
#include <game/server/core/scenarios/base/component_registry.h>

namespace
{
	class UniversalDoorControlComponent final : public Component<CUniversalScenario, UniversalDoorControlComponent>
	{
		enum class DoorAction { Create, Remove };
		std::string m_Key {};
		vec2 m_Pos {};
		DoorAction m_Action {};

	public:
		explicit UniversalDoorControlComponent(const nlohmann::json& j)
		{
			InitBaseJsonField(j);
			m_Key = j.value("key", "");
			m_Pos = j.value("position", vec2 {});
			m_Action = j.value("action", "create") == "remove" ? DoorAction::Remove : DoorAction::Create;
		}

		DECLARE_COMPONENT_NAME("universal_door_control")

	private:
		void OnStartImpl() override
		{
			if(m_Key.empty())
			{
				Finish();
				return;
			}

			if(m_Action == DoorAction::Create)
				Scenario()->CreatePersonalDoor(m_Key, m_Pos);
			else
				Scenario()->RemovePersonalDoor(m_Key);
			Finish();
		}
	};

	class UniversalUseChatComponent final : public Component<CUniversalScenario, UniversalUseChatComponent>, public IEventListener
	{
		ScopedEventListener m_ListenerScope {};
		std::string m_ChatCode {};

	public:
		explicit UniversalUseChatComponent(const nlohmann::json& j)
		{
			InitBaseJsonField(j);
			m_ChatCode = j.value("chat", "@");
			m_ListenerScope.Init(this, IEventListener::PlayerChat);
		}

		DECLARE_COMPONENT_NAME("universal_use_chat")

	private:
		void OnStartImpl() override { m_ListenerScope.Register(); }

		void OnActiveImpl() override
		{
			if(Server()->Tick() % Server()->TickSpeed() == 0)
				GS()->Broadcast(Scenario()->GetClientID(), BroadcastPriority::MainInformation, Server()->TickSpeed(), "Objective: Write in the chat: '{}'", m_ChatCode);
		}

		void OnPlayerChat(CPlayer* pFrom, const char* pMessage) override
		{
			if(!pFrom || pFrom->GetCID() != Scenario()->GetClientID())
				return;

			if(std::string_view(pMessage).find(m_ChatCode) == 0)
				Finish();
		}

		void OnEndImpl() override { m_ListenerScope.Unregister(); }
	};

	class UniversalConditionItemComponent final : public Component<CUniversalScenario, UniversalConditionItemComponent>
	{
		int m_ItemID {};
		int m_Required {};
		bool m_Remove {};
		bool m_ShowProgress {};

	public:
		explicit UniversalConditionItemComponent(const nlohmann::json& j)
		{
			InitBaseJsonField(j);
			m_ItemID = j.value("item_id", -1);
			m_Required = j.value("required", 0);
			m_Remove = j.value("remove", false);
			m_ShowProgress = j.value("show_progress", false);
		}

		DECLARE_COMPONENT_NAME("universal_condition_item")

	private:
		void OnActiveImpl() override
		{
			auto* pPlayer = Scenario()->GetPlayer();
			if(!pPlayer || m_ItemID < 0)
				return;

			auto* pItem = pPlayer->GetItem(m_ItemID);
			if(!pItem)
				return;

			if(m_ShowProgress && Server()->Tick() % Server()->TickSpeed() == 0)
				GS()->Broadcast(Scenario()->GetClientID(), BroadcastPriority::GameBasicStats, Server()->TickSpeed(),
					"Objective: to get {} ({} of {}).", pItem->Info()->GetName(), pItem->GetValue(), m_Required);

			if(pItem->GetValue() >= m_Required)
			{
				if(m_Remove)
					pItem->Remove(m_Required);
				Finish();
			}
		}
	};

	class UniversalTeleportComponent final : public Component<CUniversalScenario, UniversalTeleportComponent>
	{
		vec2 m_Pos {};
		int m_WorldID {};

	public:
		explicit UniversalTeleportComponent(const nlohmann::json& j)
		{
			InitBaseJsonField(j);
			m_Pos = j.value("position", vec2 {});
			m_WorldID = j.value("world_id", -1);
		}

		DECLARE_COMPONENT_NAME("universal_teleport")

	private:
		void OnActiveImpl() override
		{
			if(GetDelayTick() <= 0)
				Finish();
		}

		void OnEndImpl() override
		{
			auto* pPlayer = Scenario()->GetPlayer();
			if(!pPlayer || !pPlayer->GetCharacter())
				return;

			if(m_WorldID >= 0 && !GS()->IsPlayerInWorld(Scenario()->GetClientID(), m_WorldID))
				pPlayer->ChangeWorld(m_WorldID, m_Pos);
			else
				pPlayer->GetCharacter()->ChangePosition(m_Pos);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
	};

	class UniversalPickItemTaskComponent final : public Component<CUniversalScenario, UniversalPickItemTaskComponent>
	{
		vec2 m_Pos {};
		CItem m_Item {};
		std::string m_Chat {};
		std::string m_Broadcast {};
		CEntityDropItem* m_pEntDroppedItem {};

	public:
		explicit UniversalPickItemTaskComponent(const nlohmann::json& j)
		{
			InitBaseJsonField(j);
			m_Pos = j.value("position", vec2 {});
			m_Chat = j.value("chat", "");
			m_Broadcast = j.value("broadcast", "");
			if(j.contains("item"))
				j["item"].get_to(m_Item);
		}

		DECLARE_COMPONENT_NAME("universal_pick_item_task")

	private:
		void OnStartImpl() override
		{
			auto* pPlayer = Scenario()->GetPlayer();
			if(!pPlayer || !m_Item.IsValid())
			{
				Finish();
				return;
			}

			if(!m_Item.Info()->IsStackable() && pPlayer->GetItem(m_Item)->HasItem())
			{
				Finish();
				return;
			}

			const float Angle = angle(normalize(vec2 {}));
			m_pEntDroppedItem = new CEntityDropItem(&GS()->m_World, m_Pos, vec2 {}, Angle, m_Item, Scenario()->GetClientID());
			if(m_pEntDroppedItem)
			{
				GS()->CreatePlayerSpawn(m_Pos, Scenario()->GetClientsMask());
				if(!m_Chat.empty())
					GS()->Chat(Scenario()->GetClientID(), m_Chat.c_str());
			}
		}

		void OnActiveImpl() override
		{
			if(!m_pEntDroppedItem)
			{
				Finish();
				return;
			}

			m_pEntDroppedItem->SetLifetime(Server()->TickSpeed() * g_Config.m_SvDroppedItemLifetime);
			if(!m_Broadcast.empty())
				GS()->Broadcast(Scenario()->GetClientID(), BroadcastPriority::VeryImportant, Server()->TickSpeed(), m_Broadcast.c_str());

			if(!GS()->m_World.ExistEntity(m_pEntDroppedItem))
			{
				m_pEntDroppedItem = nullptr;
				Finish();
			}
		}
	};

	class UniversalShootmarkersComponent final : public Component<CUniversalScenario, UniversalShootmarkersComponent>
	{
		std::vector<std::pair<vec2, int>> m_vMarkers {};

	public:
		explicit UniversalShootmarkersComponent(const nlohmann::json& j)
		{
			InitBaseJsonField(j);
			if(j.contains("markers") && j["markers"].is_array())
			{
				for(const auto& Marker : j["markers"])
					m_vMarkers.emplace_back(Marker.value("position", vec2 {}), Marker.value("health", 1));
			}
		}

		DECLARE_COMPONENT_NAME("universal_shootmarkers")

	private:
		void OnStartImpl() override
		{
			Scenario()->ResetShootmarkers();
			for(const auto& [Pos, Health] : m_vMarkers)
				Scenario()->CreateShootmarker(Pos, Health);
		}

		void OnActiveImpl() override
		{
			GS()->Broadcast(Scenario()->GetClientID(), BroadcastPriority::VeryImportant, Server()->TickSpeed(), "Shoot the targets!");
			if(Scenario()->IsShootmarkersDestroyed())
				Finish();
		}
	};

}

template struct ComponentRegistrar<UniversalDoorControlComponent>;
template struct ComponentRegistrar<UniversalUseChatComponent>;
template struct ComponentRegistrar<UniversalConditionItemComponent>;
template struct ComponentRegistrar<UniversalTeleportComponent>;
template struct ComponentRegistrar<UniversalPickItemTaskComponent>;
template struct ComponentRegistrar<UniversalShootmarkersComponent>;

CUniversalScenario::CUniversalScenario(const nlohmann::json& jsonData)
	: PlayerScenarioBase()
{
	m_JsonData = jsonData;
}

CUniversalScenario::~CUniversalScenario()
{
	m_vpPersonalDoors.clear();
	m_vpShootmarkers.clear();
}

bool CUniversalScenario::OnStopConditions()
{
	return PlayerScenarioBase::OnStopConditions() || !GS()->GetPlayerChar(m_ClientID);
}

void CUniversalScenario::OnSetupScenario()
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

void CUniversalScenario::ProcessStep(const nlohmann::json& stepJson)
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

void CUniversalScenario::CreatePersonalDoor(const std::string& key, const vec2& pos)
{
	m_vpPersonalDoors[key].m_Pos = pos;
	m_vpPersonalDoors[key].m_EntPtr = std::make_unique<CEntityPersonalDoor>(&GS()->m_World, GetClientID(), pos, vec2(0, -1));
}

void CUniversalScenario::RemovePersonalDoor(const std::string& key)
{
	if(m_vpPersonalDoors.contains(key))
		m_vpPersonalDoors.erase(key);
}

bool CUniversalScenario::IsShootmarkersDestroyed() const
{
	return std::ranges::all_of(m_vpShootmarkers, [](const std::weak_ptr<CEntityGroup>& weakPtr)
	{
		return weakPtr.expired();
	});
}

void CUniversalScenario::CreateShootmarker(const vec2& pos, int health)
{
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_ACTION, GetClientID());
	groupPtr->SetConfig("health", health);

	const auto pEntity = groupPtr->CreatePickup(pos);
	pEntity->SetMask(CmaskOne(GetClientID()));
	pEntity->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		int& Health = pBase->GetGroup()->GetRefConfig("health", 0);
		if(Health <= 0)
		{
			pBase->GS()->CreateCircleExplosion(6, 64.f, pBase->GetPos(), -1, WEAPON_WORLD, 0);
			pBase->MarkForDestroy();
			return;
		}

		for(auto* pProj = (CProjectile*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile*)pProj->TypeNext())
		{
			if(!CmaskIsSet(pBase->GetMask(), pProj->GetOwnerCID()))
				continue;

			if(distance(pBase->GetPos(), pProj->GetCurrentPos()) < 48.f)
			{
				Health -= 1;
				pBase->GS()->CreateDamage(pBase->GetPos(), pBase->GetClientID(), 1, random_angle(), pBase->GetMask());
				pProj->MarkForDestroy();
			}
		}
	});

	m_vpShootmarkers.emplace_back(groupPtr);
}

void CUniversalScenario::ResetShootmarkers()
{
	m_vpShootmarkers.clear();
}
