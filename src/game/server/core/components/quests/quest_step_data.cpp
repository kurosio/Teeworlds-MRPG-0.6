/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_step_data.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/Inventory/InventoryManager.h>
#include "quest_manager.h"

#include <game/server/core/entities/items/drop_quest_items.h>
#include <game/server/core/entities/tools/laser_orbite.h>

#include "entities/move_action.h"
#include "entities/dir_navigator.h"
#include "game/server/entity_manager.h"

void CQuestStepBase::UpdateBot() const
{
	auto* pGS = dynamic_cast<CGS*>(Instance::GameServer(m_Bot.m_WorldID));

	// search active bot
	CPlayerBot* pPlayerBot = nullptr;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; ++i)
	{
		auto* pPlayer = dynamic_cast<CPlayerBot*>(pGS->GetPlayer(i));
		if(pPlayer && pPlayer->GetBotType() == TYPE_BOT_QUEST && pPlayer->GetBotMobID() == m_Bot.m_ID)
		{
			pPlayerBot = pPlayer;
			break;
		}
	}

	// update bot state
	const bool ActiveStepBot = IsActiveStep();
	if(ActiveStepBot && !pPlayerBot)
	{
		dbg_msg(PRINT_QUEST_PREFIX, "The mob was not found, but the quest step remains active for players.");
		pGS->CreateBot(TYPE_BOT_QUEST, m_Bot.m_BotID, m_Bot.m_ID);
	}
	else if(!ActiveStepBot && pPlayerBot)
	{
		dbg_msg(PRINT_QUEST_PREFIX, "The mob was found, but the quest step is not active for players.");
		pPlayerBot->MarkForDestroy();
	}
}

bool CQuestStepBase::IsActiveStep() const
{
	const auto* pGS = dynamic_cast<CGS*>(Instance::GameServer(m_Bot.m_WorldID));
	const int QuestID = m_Bot.m_QuestID;
	const int QuestBotID = m_Bot.m_ID;
	bool ActiveQuestStep = false;

	// check valid quest
	if(!CQuestDescription::Data().contains(QuestID))
		return false;

	for(int i = 0; i < MAX_PLAYERS; ++i)
	{
		bool& refActiveByQuest = DataBotInfo::ms_aDataBot[m_Bot.m_BotID].m_aActiveByQuest[i];
		refActiveByQuest = false;

		auto* pPlayer = pGS->GetPlayer(i);
		if(!pPlayer || !pPlayer->IsAuthed())
			continue;

		auto* pQuest = pPlayer->GetQuest(QuestID);
		if(!pQuest || pQuest->GetState() != QuestState::Accepted || pQuest->GetStepPos() != m_Bot.m_StepPos)
			continue;

		auto* pStep = pQuest->GetStepByMob(QuestBotID);
		if(!pStep || pStep->m_StepComplete || pStep->m_ClientQuitting)
			continue;

		refActiveByQuest = true;
		ActiveQuestStep = true;
	}

	return ActiveQuestStep;
}

// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
CGS* CQuestStep::GS() const
{
	return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* CQuestStep::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CQuestStep::~CQuestStep()
{
	m_ClientQuitting = true;
	CQuestStep::Update();

	m_aMobProgress.clear();
	m_aMoveActionProgress.clear();
}

bool CQuestStep::IsComplete()
{
	// check if all required items are gathered
	if(!m_Bot.m_vRequiredItems.empty())
	{
		for(auto& p : m_Bot.m_vRequiredItems)
		{
			if(GetPlayer()->GetItem(p.m_Item)->GetValue() < p.m_Item.GetValue())
				return false;
		}
	}

	// check if all required defeats are met
	if(!m_Bot.m_vRequiredDefeats.empty())
	{
		for(auto& [botID, requiredCount] : m_Bot.m_vRequiredDefeats)
		{
			if(m_aMobProgress[botID].m_Count < requiredCount)
				return false;
		}
	}

	// check if all move-to actions are completed
	bool allActionsFinished = std::ranges::all_of(m_aMoveActionProgress, [](const bool F){ return F; });
	if(!allActionsFinished)
		return false;

	return true;
}

bool CQuestStep::Finish()
{
	if(!IsComplete())
		return false;

	m_StepComplete = true;

	// save quest progress
	auto* pPlayer = GetPlayer();
	const auto QuestID = m_Bot.m_QuestID;
	if(!pPlayer->GetQuest(QuestID)->Datafile().Save())
	{
		GS()->Chat(pPlayer->GetCID(), "A system error has occurred, contact administrator.");
		dbg_msg(PRINT_QUEST_PREFIX, "After completing the quest step, unable to save the file.");
		m_StepComplete = false;
		return false;
	}

	// apply post finish
	PostFinish();
	return true;
}

void CQuestStep::PostFinish()
{
	auto* pPlayer = GetPlayer();
	ska::unordered_set<int> vInteractItemIds {};

	// required item's
	if(!m_Bot.m_vRequiredItems.empty())
	{
		for(auto& pRequired : m_Bot.m_vRequiredItems)
		{
			// show type element
			auto* pPlayerItem = pPlayer->GetItem(pRequired.m_Item);
			if(pRequired.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW)
			{
				GS()->Chat(pPlayer->GetCID(), "[Done] Show the '{} x{}' to the '{}'!", pPlayerItem->Info()->GetName(), pRequired.m_Item.GetValue(), m_Bot.GetName());
				continue;
			}

			// remove item
			vInteractItemIds.emplace(pPlayerItem->GetID());
			pPlayerItem->Remove(pRequired.m_Item.GetValue());
			GS()->Chat(pPlayer->GetCID(), "[Done] Give the '{} x{}' to the '{}'!", pPlayerItem->Info()->GetName(), pRequired.m_Item.GetValue(), m_Bot.GetName());
		}
	}

	// reward item's
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& pRewardItem : m_Bot.m_RewardItems)
		{
			// no use same giving and receiving for it can use "show"
			if(vInteractItemIds.find(pRewardItem.GetID()) != vInteractItemIds.end())
				dbg_assert(false, "the quest has (the same item of giving and receiving)");

			// add item
			auto* pPlayerItem = pPlayer->GetItem(pRewardItem);
			pPlayerItem->Add(pRewardItem.GetValue());
		}
	}

	// update
	pPlayer->StartUniversalScenario(m_Bot.m_ScenarioJson, EScenarios::SCENARIO_ON_END_STEP);
	pPlayer->GetQuest(m_Bot.m_QuestID)->Update();
	pPlayer->m_VotesData.UpdateVotesIf(MENU_JOURNAL_MAIN);
}

bool CQuestStep::TryAutoFinish()
{
	const auto* pPlayer = GetPlayer();
	if(pPlayer && !pPlayer->m_Dialog.IsActive())
	{
		bool allActionsFinished = std::ranges::all_of(m_aMoveActionProgress, [](const bool F){ return F; });
		if(allActionsFinished && IsComplete())
			return Finish();
	}

	return false;
}

void CQuestStep::AppendDefeatProgress(int DefeatedBotID)
{
	const auto* pPlayer = GetPlayer();
	if(!pPlayer || m_ClientQuitting)
		return;

	if(m_StepComplete || m_Bot.m_vRequiredDefeats.empty() || !DataBotInfo::IsDataBotValid(DefeatedBotID))
		return;

	// check quest action
	auto* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(pQuest->GetState() != QuestState::Accepted || pQuest->GetStepPos() != m_Bot.m_StepPos)
		return;

	// check complecte mob
	for(auto& [DefeatBotID, DefeatCount] : m_Bot.m_vRequiredDefeats)
	{
		if(DefeatedBotID != DefeatBotID || m_aMobProgress[DefeatedBotID].m_Count >= DefeatCount)
			continue;

		m_aMobProgress[DefeatedBotID].m_Count++;
		if(m_aMobProgress[DefeatedBotID].m_Count >= DefeatCount)
		{
			m_aMobProgress[DefeatBotID].m_Complete = true;
			GS()->Chat(pPlayer->GetCID(), "[Done] Defeat the '{}'s' for the '{}'!", DataBotInfo::ms_aDataBot[DefeatedBotID].m_aNameBot, m_Bot.GetName());
			Update();
		}

		pQuest->Datafile().Save();
		break;
	}
}

void CQuestStep::UpdateNavigator()
{
	// clearing is quitting or invlaid player
	auto* pPlayer = GetPlayer();
	if(m_ClientQuitting || m_StepComplete || !pPlayer || !pPlayer->GetCharacter())
	{
		if(m_pEntNavigator)
		{
			delete m_pEntNavigator;
			m_pEntNavigator = nullptr;
			dbg_msg("quest_step", "navigator to step (bot) removed successfully!");
		}

		return;
	}

	// try create navigator
	if(m_Bot.m_HasAction && !m_pEntNavigator)
	{
		auto* pNavigator = new CEntityDirNavigator(&GS()->m_World, POWERUP_HEALTH, 0, true, m_ClientID, 32.f, m_Bot.m_Position, m_Bot.m_WorldID);
		if(!pNavigator->IsMarkedForDestroy())
		{
			m_pEntNavigator = pNavigator;
			dbg_msg("quest_step", "navigator to step (bot) created successfully!");
		}
	}
}

void CQuestStep::UpdateObjectives()
{
	// clearing is quitting or invlaid player
	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter() || m_ClientQuitting)
	{
		ClearObjectives();
		return;
	}

	// check conditions where does not creating objectives
	auto* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(m_StepComplete || m_ClientQuitting || !m_TaskListReceived || !pQuest ||
		pQuest->GetState() != QuestState::Accepted || pQuest->GetStepPos() != m_Bot.m_StepPos)
	{
		ClearObjectives();
		return;
	}

	// create navigator for defeat mobs
	for(const auto& [DefeatBotID, DefeatCount] : m_Bot.m_vRequiredDefeats)
	{
		if(const MobBotInfo* pMob = DataBotInfo::FindMobByBot(DefeatBotID))
		{
			if(m_aMobProgress[DefeatBotID].m_Count < DefeatCount)
			{
				if(!m_vpEntitiesDefeatBotNavigator.contains(DefeatBotID))
				{
					auto* pNewNavigator = new CEntityDirNavigator(&GS()->m_World, POWERUP_ARMOR, 0, false, m_ClientID, 400.f, pMob->m_Position, pMob->m_WorldID);
					if(!pNewNavigator->IsMarkedForDestroy())
						m_vpEntitiesDefeatBotNavigator[DefeatBotID] = pNewNavigator;
				}
			}
			else
			{
				if(m_vpEntitiesDefeatBotNavigator.contains(DefeatBotID))
				{
					delete m_vpEntitiesDefeatBotNavigator[DefeatBotID];
					m_vpEntitiesDefeatBotNavigator.erase(DefeatBotID);
				}
			}
		}
	}

	// check and add entities
	const int CurrentStep = GetMoveActionCurrentStepPos();
	for(int i = 0; i < (int)m_Bot.m_vRequiredMoveAction.size(); ++i)
	{
		const auto& TaskData = m_Bot.m_vRequiredMoveAction[i];

		// clearing already completes or by swap step
		if(m_aMoveActionProgress[i] || TaskData.m_Step != CurrentStep)
		{
			if(m_vpEntitiesMoveAction.contains(i))
			{
				delete m_vpEntitiesMoveAction[i];
				m_vpEntitiesMoveAction.erase(i);
			}

			continue;
		}

		if(TaskData.m_WorldID != pPlayer->GetCurrentWorldID())
		{
			// navigator to other world
			if(!m_vpEntitiesMoveAction.contains(i))
			{
				auto* pActionNavigator = new CEntityDirNavigator(&GS()->m_World, POWERUP_ARMOR, 0, false, m_ClientID, 32.f, TaskData.m_Position, TaskData.m_WorldID);
				if(!pActionNavigator->IsMarkedForDestroy())
					m_vpEntitiesMoveAction[i] = pActionNavigator;
			}
		}
		else
		{
			// action to correct world
			if(!m_vpEntitiesMoveAction.contains(i))
			{
				auto* pAction = new CEntityQuestAction(&GS()->m_World, m_ClientID, i, this);
				if(!pAction->IsMarkedForDestroy())
					m_vpEntitiesMoveAction[i] = pAction;
			}
		}
	}
}

void CQuestStep::Update()
{
	UpdateBot();
	UpdateNavigator();
	UpdateObjectives();
}

void CQuestStep::ClearObjectives()
{
	for(auto& pPair : m_vpEntitiesDefeatBotNavigator)
		delete pPair.second;
	for(auto& pPair : m_vpEntitiesMoveAction)
		delete pPair.second;

	m_vpEntitiesMoveAction.clear();
	m_vpEntitiesDefeatBotNavigator.clear();
	dbg_msg("quest_step", "clearing objectives is done");
}

void CQuestStep::CreateVarietyTypesRequiredItems()
{
	// check default action
	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter() || m_ClientQuitting)
		return;

	if(m_StepComplete || m_Bot.m_vRequiredItems.empty())
		return;

	// check quest action
	const auto* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(pQuest->GetState() != QuestState::Accepted || pQuest->GetStepPos() != m_Bot.m_StepPos)
		return;

	// create variety types
	const auto ClientID = pPlayer->GetCID();
	for(auto& [RequiredItem, Type] : m_Bot.m_vRequiredItems)
	{
		// TYPE Drop and Pick up
		if(Type == QuestBotInfo::TaskRequiredItems::Type::PICKUP)
		{
			// check whether items are already available for pickup
			for(const auto* pHh = (CDropQuestItem*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_QUEST_DROP); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
			{
				if(pHh->m_ClientID == ClientID && pHh->m_QuestID == m_Bot.m_QuestID && pHh->m_ItemID == RequiredItem.GetID() && pHh->m_Step == m_Bot.m_StepPos)
					return;
			}

			// create items
			const int Value = 3 + RequiredItem.GetValue();
			for(int i = 0; i < Value; i++)
			{
				vec2 Vel = vec2(random_float(-40.0f, 40.0f), random_float(-40.0f, 40.0f));
				float AngleForce = Vel.x * (0.15f + random_float(0.1f));
				new CDropQuestItem(&GS()->m_World, m_Bot.m_Position, Vel, AngleForce, RequiredItem.GetID(), RequiredItem.GetValue(), m_Bot.m_QuestID, m_Bot.m_StepPos, ClientID);
			}
		}
	}
}

void CQuestStep::FormatStringTasks(char* aBufQuestTask, int Size)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	std::string strBuffer {};

	// show required bots
	if(!m_Bot.m_vRequiredDefeats.empty())
	{
		strBuffer += "\n\n" + fmt_localize(m_ClientID, "- \u270E Slay enemies:");
		for(auto& p : m_Bot.m_vRequiredDefeats)
		{
			const char* pCompletePrefix = (m_aMobProgress[p.m_BotID].m_Count >= p.m_RequiredCount ? "\u2611" : "\u2610");
			strBuffer += "\n" + fmt_localize(m_ClientID, "{} Defeat {} ({}/{})",
				pCompletePrefix, DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, m_aMobProgress[p.m_BotID].m_Count, p.m_RequiredCount);
		}
	}

	// show required items
	if(!m_Bot.m_vRequiredItems.empty())
	{
		strBuffer += "\n\n" + fmt_localize(m_ClientID, "- \u270E Retrieve an item's:");
		for(auto& pRequied : m_Bot.m_vRequiredItems)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequied.m_Item);
			const char* pCompletePrefix = (pPlayerItem->GetValue() >= pRequied.m_Item.GetValue() ? "\u2611" : "\u2610");
			const char* pInteractiveType = pRequied.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW ? "Show a" : "Require a";
			strBuffer += "\n" + fmt_localize(m_ClientID, "{} {} {} ({}/{}).",
				pCompletePrefix, pInteractiveType, pPlayerItem->Info()->GetName(), pPlayerItem->GetValue(), pRequied.m_Item.GetValue());
		}
	}

	// show move to
	if(!m_Bot.m_vRequiredMoveAction.empty())
	{
		strBuffer += "\n\n" + fmt_localize(m_ClientID, "- \u270E Trigger some action's:");

		// Create an unordered map called m_Order with key type int and value type unordered_map<string, pair<int, int>> for special order task's
		std::map<int /* step */, ska::unordered_map<std::string /* task name */, std::pair<int /* complected */, int /* count */>>> m_Order;
		for(int i = 0; i < (int)m_Bot.m_vRequiredMoveAction.size(); i++)
		{
			// If TaskMapID is empty, assign it the value "Demands a bit of action"
			std::string TaskMapID = m_Bot.m_vRequiredMoveAction[i].m_TaskName;

			// If m_aMoveActionProgress[i] is true, increment the first value of the pair in m_Order[Step][TaskMapID]
			int Step = m_Bot.m_vRequiredMoveAction[i].m_Step;
			if(m_aMoveActionProgress[i])
				m_Order[Step][TaskMapID].first++;

			// Increment the second value of the pair in m_Order[Step][TaskMapID]
			m_Order[Step][TaskMapID].second++;
		}

		// Loop through each element in m_Order
		for(auto& [Step, TaskMap] : m_Order)
		{
			// Loop through each element in TaskMap
			for(auto& [Name, StepCount] : TaskMap)
			{
				// Append a newline character to Buffer
				strBuffer += "\n";

				// Check for one task
				const int& TaskNum = StepCount.second;
				const int& TaskCompleted = StepCount.first;
				const char* pCompletePrefix = (TaskCompleted >= TaskNum ? "\u2611" : "\u2610");
				if(TaskNum == 1)
				{
					strBuffer += fmt_localize(m_ClientID, "{}. {} {}.", Step, pCompletePrefix, Name.c_str());
					continue;
				}

				// Multi task
				strBuffer += fmt_localize(m_ClientID, "{}. {} {} ({}/{}).", Step, pCompletePrefix, Name.c_str(), TaskCompleted, TaskNum);
			}
		}
	}

	// show reward items
	if(!m_Bot.m_RewardItems.empty())
	{
		strBuffer += "\n\n" + fmt_localize(m_ClientID, "- \u270E Reward for completing a task:");
		for(auto& p : m_Bot.m_RewardItems)
			strBuffer += "\n" + fmt_localize(m_ClientID, "Obtain a {} ({}).", p.Info()->GetName(), p.GetValue());
	}

	// Copy the contents of the buffer `Buffer` into the character array `aBufQuestTask`,
	str_copy(aBufQuestTask, strBuffer.c_str(), Size);
}

int CQuestStep::GetMoveActionCurrentStepPos() const
{
	for(int i = 0; i < (int)m_Bot.m_vRequiredMoveAction.size(); i++)
	{
		if(m_aMoveActionProgress[i])
			continue;
		return m_Bot.m_vRequiredMoveAction[i].m_Step;
	}

	return 1;
}