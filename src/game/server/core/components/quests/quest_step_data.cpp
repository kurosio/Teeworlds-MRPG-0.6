/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_step_data.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/Inventory/InventoryManager.h>
#include "quest_manager.h"

#include <game/server/core/entities/items/drop_quest_items.h>
#include <game/server/core/entities/tools/arrow_navigator.h>
#include <game/server/core/entities/tools/laser_orbite.h>

#include "entities/move_action.h"
#include "entities/path_finder.h"
#include "game/server/entity_manager.h"

void CQuestStepBase::UpdateBot() const
{
	auto* pGS = dynamic_cast<CGS*>(Instance::GameServer(m_Bot.m_WorldID));

	// search active bot
	int BotClientID = -1;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; ++i)
	{
		CPlayer* pPlayer = pGS->GetPlayer(i);
		if(pPlayer && pPlayer->GetBotType() == TYPE_BOT_QUEST && pPlayer->GetBotMobID() == m_Bot.m_ID)
		{
			BotClientID = i;
			break;
		}
	}

	// update bot state
	const bool ActiveStepBot = IsActiveStep();
	if(ActiveStepBot && BotClientID == -1)
	{
		dbg_msg(PRINT_QUEST_PREFIX, "The mob was not found, but the quest step remains active for players.");
		pGS->CreateBot(TYPE_BOT_QUEST, m_Bot.m_BotID, m_Bot.m_ID);
	}
	else if(!ActiveStepBot && BotClientID != -1)
	{
		dbg_msg(PRINT_QUEST_PREFIX, "The mob was found, but the quest step is not active for players.");
		pGS->DestroyPlayer(BotClientID);
	}
}

bool CQuestStepBase::IsActiveStep() const
{
	const auto* pGS = dynamic_cast<CGS*>(Instance::GameServer(m_Bot.m_WorldID));
	const int QuestID = m_Bot.m_QuestID;
	const int QuestBotID = m_Bot.m_ID;

	// check valid quest
	if(!CQuestDescription::Data().contains(QuestID))
		return false;

	for(int i = 0; i < MAX_PLAYERS; ++i)
	{
		CPlayer* pPlayer = pGS->GetPlayer(i);
		if(!pPlayer || !pPlayer->IsAuthed())
			continue;

		CPlayerQuest* pQuest = pPlayer->GetQuest(QuestID);
		if(!pQuest || pQuest->GetState() != QuestState::ACCEPT || pQuest->GetStepPos() != m_Bot.m_StepPos)
			continue;

		CQuestStep* pStep = pQuest->GetStepByMob(QuestBotID);
		if(!pStep || pStep->m_StepComplete || pStep->m_ClientQuitting)
			continue;

		return true;
	}

	return false;
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
	m_aMobProgress.clear();
	m_aMoveActionProgress.clear();
	CQuestStepBase::UpdateBot();
	m_vpEntitiesAction.clear();
	m_vpEntitiesNavigator.clear();

	// update bot and path navigator
	UpdatePathNavigator();
}

int CQuestStep::GetNumberBlockedItem(int ItemID) const
{
	return std::accumulate(m_Bot.m_vRequiredItems.begin(), m_Bot.m_vRequiredItems.end(), 0, [ItemID](int Amount, const auto& p) 
	{
		return Amount + (p.m_Item.GetID() == ItemID ? p.m_Item.GetValue() : 0);
	});
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
	if(GetCompletedMoveActionCount() < (int)m_aMoveActionProgress.size())
		return false;

	return true;
}

bool CQuestStep::Finish()
{
	// initialize variables
	CPlayer* pPlayer = GetPlayer();
	const int QuestID = m_Bot.m_QuestID;

	// check is competed quest
	if(!IsComplete())
		return false;

	// set flag to complete
	m_StepComplete = true;

	// save quest progress
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
	CPlayer* pPlayer = GetPlayer();
	int ClientID = pPlayer->GetCID();
	ska::unordered_set<int> vInteractItemIds {};

	// required item's
	if(!m_Bot.m_vRequiredItems.empty())
	{
		for(auto& pRequired : m_Bot.m_vRequiredItems)
		{
			// show type element
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequired.m_Item);

			if(pRequired.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW)
			{
				GS()->Chat(pPlayer->GetCID(), "[Done] Show the {}x{} to the {}!", pPlayerItem->Info()->GetName(), pRequired.m_Item.GetValue(), m_Bot.GetName());
				continue;
			}

			// remove item
			vInteractItemIds.emplace(pPlayerItem->GetID());
			pPlayerItem->Remove(pRequired.m_Item.GetValue());
			GS()->Chat(pPlayer->GetCID(), "[Done] Give the {}x{} to the {}!", pPlayerItem->Info()->GetName(), pRequired.m_Item.GetValue(), m_Bot.GetName());
		}
	}

	// reward item's
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& pRewardItem : m_Bot.m_RewardItems)
		{
			// no use same giving and receiving for it can use "show"
			dbg_assert(vInteractItemIds.find(pRewardItem.GetID()) != vInteractItemIds.end(), "the quest has (the same item of giving and receiving)");

			// check for enchant item
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRewardItem);

			// give item
			pPlayerItem->Add(pRewardItem.GetValue());
		}
	}

	// update bot status
	DataBotInfo::ms_aDataBot[m_Bot.m_BotID].m_aVisibleActive[ClientID] = false;
	pPlayer->GetQuest(m_Bot.m_QuestID)->Update();
	pPlayer->m_VotesData.UpdateVotesIf(MENU_JOURNAL_MAIN);
}

void CQuestStep::AppendDefeatProgress(int DefeatedBotID)
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(m_StepComplete || m_ClientQuitting || m_Bot.m_vRequiredDefeats.empty() || !pPlayer || !DataBotInfo::IsDataBotValid(DefeatedBotID))
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetStepPos() != m_Bot.m_StepPos)
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
			GS()->Chat(pPlayer->GetCID(), "[Done] Defeat the {}'s for the {}!", DataBotInfo::ms_aDataBot[DefeatedBotID].m_aNameBot, m_Bot.GetName());
		}

		pQuest->Datafile().Save();
		break;
	}
}

void CQuestStep::UpdatePathNavigator()
{
	// skip if the bot is without action
	if(!m_Bot.m_HasAction)
		return;

	CPlayer* pPlayer = GetPlayer();
	const bool Exists = m_pEntNavigator && GS()->m_World.ExistEntity(m_pEntNavigator);
	const bool DependLife = !m_StepComplete && !m_ClientQuitting && pPlayer && pPlayer->GetCharacter();

	if(!DependLife && Exists)
	{
		dbg_msg("test", "delete navigator");
		delete m_pEntNavigator;
		m_pEntNavigator = nullptr;
	}
	else if(DependLife && !Exists)
	{
		dbg_msg("test", "create navigator");
		m_pEntNavigator = new CEntityArrowNavigator(&GS()->m_World, m_ClientID, m_Bot.m_Position, m_Bot.m_WorldID);
	}
}

void CQuestStep::UpdateTaskMoveTo()
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(!m_TaskListReceived || m_StepComplete || m_ClientQuitting || !pPlayer || !pPlayer->GetCharacter())
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetStepPos() != m_Bot.m_StepPos)
		return;

	// check and mark required mob's
	for(auto& [DefeatBotID, DefeatCount] : m_Bot.m_vRequiredDefeats)
	{
		if(m_aMobProgress[DefeatBotID].m_Count >= DefeatCount)
			continue;

		if(const MobBotInfo* pMob = DataBotInfo::FindMobByBot(DefeatBotID))
		{
			CreateEntityArrowNavigator(pMob->m_Position, pMob->m_WorldID, 400.f, CEntityPathArrow::CONDITION_DEFEAT_BOT, DefeatBotID);
		}
	}

	// check and add entities
	if(!m_aMoveActionProgress.empty())
	{
		const int CurrentStep = GetMoveActionCurrentStepPos();
		for(int i = 0; i < (int)m_Bot.m_vRequiredMoveAction.size(); i++)
		{
			// skip completed and not current step's
			auto* pTaskData = &m_Bot.m_vRequiredMoveAction[i];
			if(CurrentStep != pTaskData->m_Step || m_aMoveActionProgress[i])
				continue;

			// Always creating navigator in other worlds 
			if(pTaskData->m_WorldID != pPlayer->GetPlayerWorldID())
			{
				CreateEntityArrowNavigator(pTaskData->m_Position, pTaskData->m_WorldID, 0.f, CEntityPathArrow::CONDITION_MOVE_TO, i);
				continue;
			}

			// Add move to point questing mob
			std::optional <int> OptDefeatBotCID = std::nullopt;
			if(pTaskData->IsHasDefeatMob())
			{
				CPlayerBot* pPlayerBot = nullptr;
				for(int c = MAX_PLAYERS; c < MAX_CLIENTS; c++)
				{
					CPlayerBot* pPlBotSearch = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(c));
					if(pPlBotSearch && pPlBotSearch->GetQuestBotMobInfo().m_QuestID == pQuest->GetID() &&
						pPlBotSearch->GetQuestBotMobInfo().m_QuestStep == m_Bot.m_StepPos &&
						pPlBotSearch->GetQuestBotMobInfo().m_MoveToStep == i)
					{
						pPlayerBot = pPlBotSearch;
						break;
					}
				}

				if(!pPlayerBot)
				{
					const auto& defeatMobInfo = pTaskData->m_DefeatMobInfo;
					const int MobClientID = GS()->CreateBot(TYPE_BOT_QUEST_MOB, defeatMobInfo.m_BotID, -1);
					pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(MobClientID));
					pPlayerBot->InitQuestBotMobInfo(
						{
							m_Bot.m_QuestID,
							m_Bot.m_StepPos,
							i,
							defeatMobInfo.m_AttributePower,
							defeatMobInfo.m_AttributeSpread,
							defeatMobInfo.m_WorldID,
							pTaskData->m_Position
						});

					dbg_msg(PRINT_QUEST_PREFIX, "Creating a quest mob");
				}

				pPlayerBot->GetQuestBotMobInfo().m_ActiveForClient[pPlayer->GetCID()] = true;
				pPlayerBot->GetQuestBotMobInfo().m_CompleteClient[pPlayer->GetCID()] = false;
				OptDefeatBotCID = pPlayerBot->GetCID();
			}

			// update entity quest action
			CreateEntityQuestAction(i, OptDefeatBotCID);
		}
	}
}

void CQuestStep::Update()
{
	UpdateBot();
	UpdatePathNavigator();
	UpdateTaskMoveTo();
}

void CQuestStep::CreateVarietyTypesRequiredItems()
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(m_StepComplete || m_ClientQuitting || m_Bot.m_vRequiredItems.empty() || !pPlayer || !pPlayer->GetCharacter())
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetStepPos() != m_Bot.m_StepPos)
		return;

	// create variety types
	const int ClientID = pPlayer->GetCID();
	for(auto& [RequiredItem, Type] : m_Bot.m_vRequiredItems)
	{
		// TYPE Drop and Pick up
		if(Type == QuestBotInfo::TaskRequiredItems::Type::PICKUP)
		{
			// check whether items are already available for pickup
			for(CDropQuestItem* pHh = (CDropQuestItem*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_QUEST_DROP); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
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

		// TODO: add new types
	}
}

void CQuestStep::FormatStringTasks(char* aBufQuestTask, int Size)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	std::string strBuffer {};
	const char* pLang = pPlayer->GetLanguage();

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

int CQuestStep::GetMoveActionNum() const
{
	return (int)m_aMoveActionProgress.size();
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

int CQuestStep::GetCompletedMoveActionCount()
{
	return (int)std::ranges::count_if(m_aMoveActionProgress, [](const bool State){return State == true; });
}

void CQuestStep::CreateEntityQuestAction(int MoveToIndex, std::optional<int> OptDefeatBotCID)
{
	if(MoveToIndex < 0 || MoveToIndex >= (int)m_Bot.m_vRequiredMoveAction.size())
		return;

	// find the action by move to index
	const auto iter = std::ranges::find_if(m_vpEntitiesAction, [MoveToIndex](const std::shared_ptr<CEntityQuestAction>& pPtr)
	{
		return MoveToIndex == pPtr->GetMoveToIndex();
	});

	// create a new move to item if it does not exist
	if(iter == m_vpEntitiesAction.end())
	{
		auto pSharedAction = std::make_shared<CEntityQuestAction>(&GS()->m_World, m_ClientID, MoveToIndex, weak_from_this(), m_Bot.IsAutoCompletesQuestStep(), OptDefeatBotCID);
		m_vpEntitiesAction.emplace_back(pSharedAction);

		// create navigations
		auto* pTaskData = &m_Bot.m_vRequiredMoveAction[MoveToIndex];
		if(pTaskData->m_TypeFlags & QuestBotInfo::TaskAction::Types::DEFEAT_MOB)
		{
			CEntityLaserOrbite* pEntOrbite;
			constexpr float Radius = 400.f;
			GS()->EntityManager()->LaserOrbite(pEntOrbite, pSharedAction.get(), (int)(Radius / 50.f),
				LaserOrbiteType::INSIDE_ORBITE, 0.f, pSharedAction->GetRadius(), LASERTYPE_FREEZE, CmaskOne(m_ClientID));

			CreateEntityArrowNavigator(pTaskData->m_Position, pTaskData->m_WorldID, Radius, CEntityPathArrow::CONDITION_MOVE_TO, MoveToIndex);
		}
		else if(!pTaskData->m_Navigator)
		{
			CEntityLaserOrbite* pEntOrbite;
			const float Radius = 400.f + random_float(2000.f);
			GS()->EntityManager()->LaserOrbite(pEntOrbite, pSharedAction.get(), (int)(Radius / 50.f),
				LaserOrbiteType::INSIDE_ORBITE_RANDOM, 0.f, Radius, LASERTYPE_FREEZE, CmaskOne(m_ClientID));

			CreateEntityArrowNavigator(pTaskData->m_Position, pTaskData->m_WorldID, Radius, CEntityPathArrow::CONDITION_MOVE_TO, MoveToIndex);
		}
		else
		{
			CreateEntityArrowNavigator(pTaskData->m_Position, pTaskData->m_WorldID, 0.f, CEntityPathArrow::CONDITION_MOVE_TO, MoveToIndex);
		}

	}
}

void CQuestStep::CreateEntityArrowNavigator(vec2 Position, int WorldID, float AreaClipped, int ConditionType, int ConditionIndex)
{
	// find the bot navigator by the position
	const auto iter = std::ranges::find_if(m_vpEntitiesNavigator, [Position](const std::shared_ptr<CEntityPathArrow>& pPtr)
	{
		return pPtr->GetPosTo() == Position;
	});

	// create a new arrow navigator if it does not exist
	if(iter == m_vpEntitiesNavigator.end())
	{
		auto pSharedArrow = std::make_shared<CEntityPathArrow>(&GS()->m_World, m_ClientID, AreaClipped, Position, WorldID, weak_from_this(), ConditionType, ConditionIndex);
		m_vpEntitiesNavigator.emplace_back(pSharedArrow);
	}
}
