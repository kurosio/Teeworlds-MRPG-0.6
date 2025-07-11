#include "scenario_universal.h"

#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/drop_items.h>
#include <game/server/core/entities/group/entitiy_group.h>
#include <game/server/entities/projectile.h>

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
	return true;//!GetPlayer() || !GetCharacter();
}

void CUniversalScenario::OnSetupScenario()
{
	if(!m_JsonData.is_object() || !m_JsonData.contains("steps"))
	{
		dbg_msg("scenario-tutorial", "Invalid JSON format or missing 'steps'");
		return;
	}

	// initialize steps
	for(const auto& step : m_JsonData["steps"])
		ProcessStep(step);
}

void CUniversalScenario::ProcessStep(const nlohmann::json& step)
{
	/*// check valid action
	if(!step.contains("action") || !step["action"].is_string())
	{
		dbg_msg("scenario-tutorial", "Missing or invalid 'action' key in JSON");
		return;
	}

	std::string action = step["action"];
	if(action == "check_has_item")
	{
		int itemID = step.value("item_id", -1);
		int required = step.value("required", 0);
		bool remove = step.value("remove", false);
		bool showProgress = step.value("show_progress", false);

		auto& hasItemStep = AddStep();
		if(showProgress)
		{
			hasItemStep.WhenActive([this, itemID, required]()
			{
				if(Server()->Tick() % Server()->TickSpeed() == 0)
				{
					auto* pItem = GetPlayer()->GetItem(itemID);
					GS()->Broadcast(GetClientID(), BroadcastPriority::GameBasicStats, Server()->TickSpeed(),
						"Objective: to get {} ({} of {}).", pItem->Info()->GetName(), pItem->GetValue(), required);
				}
			});
		}
		if(remove)
		{
			hasItemStep.WhenFinished([this, itemID, required]()
			{
				GetPlayer()->GetItem(itemID)->Remove(required);
			});
		}

		hasItemStep.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, itemID, required]()
		{
			return GetPlayer()->GetItem(itemID)->GetValue() >= required;
		});
	}

	// reset quest
	else if(action == "reset_quest")
	{
		const auto QuestID = step.value("quest_id", -1);
		auto& resetQuestStep = AddStep();
		resetQuestStep.WhenStarted([QuestID, this]()
		{
			if(QuestID > 0)
			{
				const auto* pPlayer = GetPlayer();
				auto* pQuest = pPlayer->GetQuest(QuestID);
				pQuest->Reset();
			}
		});
	}

	// reset quest
	else if(action == "accept_quest")
	{
		const auto QuestID = step.value("quest_id", -1);
		auto& acceptQuestStep = AddStep();
		acceptQuestStep.WhenStarted([QuestID, this]()
		{
			if(QuestID > 0)
			{
				const auto* pPlayer = GetPlayer();
				auto* pQuest = pPlayer->GetQuest(QuestID);
				if(pQuest->IsAccepted())
					pQuest->Reset();
				pQuest->Accept();
			}
		});
	}

	// create new door
	else if(action == "new_door")
	{
		bool Follow = step.value("follow", false);
		std::string Key = step.value("key", "");
		m_vpPersonalDoors[Key].m_Pos =
		{
			step["position"].value("x", 0.0f),
			step["position"].value("y", 0.0f)
		};

		auto& newDoorStep = AddStep();
		newDoorStep.WhenStarted([Key, this]()
		{
			vec2 Pos = m_vpPersonalDoors[Key].m_Pos;
			m_vpPersonalDoors[Key].m_EntPtr = std::make_unique<CEntityPersonalDoor>(&GS()->m_World, GetClientID(), Pos, vec2(0, -1));
		});

		if(Follow)
		{
			StepMessage(0, "The door's locked!", "\0");
			StepFixedCam(100, m_vpPersonalDoors[Key].m_Pos);
		}
	}

	// remove door
	else if(action == "remove_door")
	{
		bool Follow = step.value("follow", false);
		std::string Key = step.value("key", "");
		vec2 DoorPos = m_vpPersonalDoors[Key].m_Pos;

		auto& removeDoorStep = AddStep();
		removeDoorStep.WhenStarted([Follow, Key, this]()
		{
			if(m_vpPersonalDoors.contains(Key))
			{
				m_vpPersonalDoors.erase(Key);
			}
		});

		if(Follow)
		{
			StepMessage(0, "The door's is oppened!", "\0");
			StepFixedCam(100, DoorPos);
		}
	}

	// message
	else if(action == "message")
	{
		int delay = step.value("delay", 0);
		std::string chatMsg = step.value("chat", "");
		std::string broadcastMsg = step.value("broadcast", "");
		std::string fullMsg = step.value("full", "");

		if(!fullMsg.empty())
			StepMessage(delay, fullMsg, fullMsg);
		else
			StepMessage(delay, broadcastMsg, chatMsg);
	}

	// dropped item task
	else if(action == "pick_item_task")
	{
		if(!step.contains("item"))
			return;

		vec2 position =
		{
			step["position"].value("x", 0.0f),
			step["position"].value("y", 0.0f)
		};
		std::string chatMsg = step.value("chat", "");
		std::string broadcastMsg = step.value("broadcast", "");
		std::string fullMsg = step.value("full", "");

		if(!fullMsg.empty())
			StepPickItemTask(position, step["item"], fullMsg, fullMsg);
		else
			StepPickItemTask(position, step["item"], broadcastMsg, chatMsg);
	}

	// emote message
	else if(action == "emote")
	{
		int emoteType = step.value("emote_type", (int)EMOTE_NORMAL);
		int emoticonType = step.value("emoticon_type", -1);

		StepEmote(emoteType, emoticonType);
	}

	// teleport
	else if(action == "teleport")
	{
		vec2 position =
		{
			step["position"].value("x", 0.0f),
			step["position"].value("y", 0.0f)
		};
		int worldID = step.value("world_id", -1);
		StepTeleport(position, worldID);
	}

	// chat task
	else if(action == "use_chat_task")
	{
		auto& useChatTaskStep = AddStep();
		std::string message = step.value("chat", "@");
		useChatTaskStep.WhenActive([this, message]()
		{
			if(Server()->Tick() % Server()->TickSpeed() == 0)
			{
				GS()->Broadcast(GetClientID(), BroadcastPriority::MainInformation, Server()->TickSpeed(), "Objective: Write in the chat: '{}'", message);
			}
		});
		useChatTaskStep.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, message]()
		{
			std::string lastMessage = GetPlayer()->m_aLastMsg;
			return lastMessage.find(message) == 0;
		});
	}

	// movement task
	else if(action == "movement_task")
	{
		int delay = step.value("delay", 0);
		vec2 position =
		{
			step["position"].value("x", 0.0f),
			step["position"].value("y", 0.0f)
		};
		std::string targetLookText = step.value("target_lock_text", "");
		bool targetLook = step.value("target_look", true);

		std::string chatMsg = step.value("chat", "");
		std::string broadcastMsg = step.value("broadcast", "");
		std::string fullMsg = step.value("full", "");

		if(!fullMsg.empty())
			StepMovementTask(delay, position, targetLookText, fullMsg, fullMsg, targetLook);
		else
			StepMovementTask(delay, position, targetLookText, broadcastMsg, chatMsg, targetLook);
	}

	// fixed cam
	else if(action == "fix_cam")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		StepFixedCam(delay, position);
	}

	// freeze movements
	else if(action == "freeze_movements")
	{
		bool freezeState = step.value("state", false);
		StepMovingDisable(freezeState);
	}

	// check quest accepted
	else if(action == "check_quest_accepted")
	{
		if(int questID = step.value("quest_id", -1); questID > 0)
		{
			auto& checkQuestAcceptedState = AddStep();
			checkQuestAcceptedState.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID]()
			{
				return GetPlayer()->GetQuest(questID)->IsAccepted();
			});
		}
	}

	// check quest completed
	else if(action == "check_quest_finished")
	{
		if(int questID = step.value("quest_id", -1); questID > 0)
		{
			auto& checkQuestFinishedState = AddStep();
			checkQuestFinishedState.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID]()
			{
				return GetPlayer()->GetQuest(questID)->IsCompleted();
			});
		}
	}

	// check quest step completed
	else if(action == "check_quest_step_finished")
	{
		if(int questID = step.value("quest_id", -1); questID > 0)
		{
			auto stepQuest = step.value("step", -1);
			auto& checkQuestStepFinishedState = AddStep();
			checkQuestStepFinishedState.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID, stepQuest]()
			{
				return GetPlayer()->GetQuest(questID)->GetStepPos() > stepQuest;
			});
		}
	}

	// shotmarks
	else if(action == "shootmarkers")
	{
		std::vector<std::pair<vec2, int>> markers;
		if(step.contains("markers") && step["markers"].is_array())
		{
			for(const auto& marker : step["markers"])
			{
				vec2 position = { marker["position"].value("x", 0.0f), marker["position"].value("y", 0.0f) };
				int health = marker.value("health", 1);
				markers.emplace_back(position, health);
			}
		}

		StepShootmarkers(markers);
	}
	else
	{
		dbg_msg("scenario-tutorial", "Unknown action: %s", action.c_str());
	}*/
}

/*
void CUniversalScenario::StepMovementTask(int delay, const vec2& pos, const std::string& targetLookText, const std::string& broadcastMsg, const std::string& chatMsg, bool targetLook)
{
	// is has lockView
	if(targetLook)
	{
		StepMovingDisable(true);

		auto& lockStep = AddStep(delay);
		lockStep.WhenStarted([this, pos]()
		{
			m_MovementPos = pos;
		});
		lockStep.WhenActive([this, targetLookText]()
		{
			if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
			{
				GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
			}

			GetPlayer()->LockedView().ViewLock(m_MovementPos, true);
			SendBroadcast(targetLookText);
		});

		StepMovingDisable(false);
	}

	// movements
	auto& moveStep = AddStep();
	moveStep.WhenStarted([this, chatMsg]()
	{
		GetCharacter()->MovingDisable(false);

		if(!chatMsg.empty())
			GS()->Chat(GetClientID(), chatMsg.c_str());
	});
	moveStep.WhenActive([this, broadcastMsg]()
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
		}

		if(!broadcastMsg.empty())
			GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, Server()->TickSpeed(), broadcastMsg.c_str());
	});
	moveStep.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this]()
	{
		return distance(GetCharacter()->GetPos(), m_MovementPos) < 32.f;
	});
}

void CUniversalScenario::StepPickItemTask(const vec2& pos, const nlohmann::json& itemJson, const std::string& broadcastMsg, const std::string& chatMsg)
{
	auto& pickItemStep = AddStep();

	CItem item;
	itemJson.get_to(item);
	if(!item.IsValid())
	{
		dbg_msg("scenario-universal", "skip step (StepPickItemTask) invalid item.");
		return;
	}

	pickItemStep.WhenStarted([this, pos, item, chatMsg]()
	{
		if(!item.Info()->IsStackable() && GetPlayer()->GetItem(item)->HasItem())
			return;

		const float Angle = angle(normalize(vec2 {}));
		m_pEntDroppedItem = new CEntityDropItem(&GS()->m_World, pos, vec2{ }, Angle, item, GetClientID());
		if(m_pEntDroppedItem)
		{
			GS()->CreatePlayerSpawn(pos, CmaskOne(GetClientID()));
			if(!chatMsg.empty())
				GS()->Chat(GetClientID(), chatMsg.c_str());
		}
	});
	pickItemStep.WhenActive([this, broadcastMsg]()
	{
		if(m_pEntDroppedItem)
		{
			m_pEntDroppedItem->SetLifetime(Server()->TickSpeed() * g_Config.m_SvDroppedItemLifetime);
			if(!broadcastMsg.empty())
				GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, Server()->TickSpeed(), broadcastMsg.c_str());
		}
	});
	pickItemStep.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, item]()
	{
		if(!GS()->m_World.ExistEntity(m_pEntDroppedItem))
		{
			m_pEntDroppedItem = nullptr;
			return true;
		}
		return false;
	});
}

void CUniversalScenario::StepFixedCam(int delay, const vec2& pos)
{
	auto& step = AddStep(delay);
	step.WhenActive([this, pos]()
	{
		GetPlayer()->LockedView().ViewLock(pos, true);
	});
}

void CUniversalScenario::StepTeleport(const vec2& pos, int worldID)
{
	auto& teleportStep = AddStep();
	teleportStep.WhenStarted([this, pos, worldID]()
	{
		auto* pPlayer = GetPlayer();
		if(worldID >= 0 && !GS()->IsPlayerInWorld(GetClientID(), worldID))
		{
			pPlayer->ChangeWorld(worldID, pos);
			return;
		}

		pPlayer->GetCharacter()->ChangePosition(pos);
		pPlayer->m_VotesData.UpdateCurrentVotes();
	});
}

void CUniversalScenario::StepMovingDisable(bool State)
{
	auto& movingStep = AddStep();
	movingStep.WhenStarted([this, State]()
	{
		GetCharacter()->MovingDisable(State);
	});
}


void CUniversalScenario::StepShootmarkers(const std::vector<std::pair<vec2, int>>& vShotmarkers)
{
	auto& stepCreateShootmarkers = AddStep();
	stepCreateShootmarkers.WhenStarted([this, vShotmarkers]()
	{
		for(const auto& [position, health] : vShotmarkers)
			CreateEntityShootmarkersTask(position, health);
	});

	for(const auto& [position, health] : vShotmarkers)
		StepFixedCam(100, position);

	auto& stepShootmarkers = AddStep();
	stepShootmarkers.WhenActive([this]()
	{
		GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, Server()->TickSpeed(), "Shoot the targets!");
	});
	stepShootmarkers.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this]()
	{
		return std::ranges::all_of(m_vpShootmarkers, [](const std::weak_ptr<CEntityGroup>& weakPtr)
		{
			return weakPtr.expired();
		});
	});
}

void CUniversalScenario::StepMessage(int delay, const std::string& broadcastMsg, const std::string& chatMsg)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, delay, chatMsg, broadcastMsg]()
	{
		if(!broadcastMsg.empty())
		{
			GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, delay, broadcastMsg.c_str());
		}

		if(!chatMsg.empty())
		{
			GS()->Chat(GetClientID(), chatMsg.c_str());
		}
	});
}

void CUniversalScenario::StepEmote(int emoteType, int emoticonType)
{
	auto& emoteStep = AddStep();
	emoteStep.WhenStarted([this, emoteType, emoticonType]()
	{
		GetCharacter()->SetEmote(emoteType, 1, false);
		GS()->SendEmoticon(GetClientID(), emoticonType);
	});
}

void CUniversalScenario::CreateEntityShootmarkersTask(const vec2& pos, int health)
{
	// initialize group
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_ACTION, GetClientID());
	groupPtr->SetConfig("health", health);

	// initialize effect
	const auto pEntity = groupPtr->CreatePickup(pos);
	pEntity->SetMask(CmaskOne(GetClientID()));
	pEntity->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// health
		int& Health = pBase->GetGroup()->GetRefConfig("health", 0);
		if(Health <= 0)
		{
			pBase->GS()->CreateCyrcleExplosion(6, 64.f, pBase->GetPos(), -1, WEAPON_WORLD, 0);
			pBase->MarkForDestroy();
			return;
		}

		// destroy projectiles
		for(auto* pProj = (CProjectile*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile*)pProj->TypeNext())
		{
			if(!CmaskIsSet(pBase->GetMask(), pProj->GetOwnerCID()))
				continue;

			if(distance(pBase->GetPos(), pProj->GetCurrentPos()) < 48.f)
			{
				Health -= 1;
				pBase->GS()->CreateDamage(pBase->GetPos(), pBase->GetClientID(), 1, random_angle(), CmaskOne(pBase->GetClientID()));
				pProj->MarkForDestroy();
			}
		}
	});
	m_vpShootmarkers.emplace_back(groupPtr);
}

void CUniversalScenario::SendBroadcast(const std::string& text) const
{
	GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, 300, text.c_str());
}
*/