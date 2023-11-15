/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestStepDataInfo.h"

#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include <game/server/mmocore/GameEntities/Items/drop_quest_items.h>
#include <game/server/mmocore/Components/Inventory/InventoryManager.h>
#include "QuestManager.h"

#include <game/server/mmocore/GameEntities/laser_orbite.h>
#include "Entities/move_to.h"
#include "Entities/path_finder.h"

// ##############################################################
// ################# GLOBAL STEP STRUCTURE ######################
void CQuestStepDescription::UpdateBot()
{
	CGS* pGS = (CGS*)Instance::GetServer()->GameServer(m_Bot.m_WorldID);
	if(!pGS)
		return;

	// check it's if there's a active bot
	int BotClientID = -1;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if(!pGS->m_apPlayers[i] || pGS->m_apPlayers[i]->GetBotType() != TYPE_BOT_QUEST || pGS->m_apPlayers[i]->GetBotMobID() != m_Bot.m_SubBotID)
			continue;

		BotClientID = i;
		break;
	}

	// seek if all players have an active bot
	const bool ActiveStepBot = IsActiveStep(pGS);
	if(ActiveStepBot && BotClientID <= -1)
	{
		dbg_msg(QUEST_PREFIX_DEBUG, "The mob was not found, but the quest step remains active for players.");
		pGS->CreateBot(TYPE_BOT_QUEST, m_Bot.m_BotID, m_Bot.m_SubBotID);
	}
	// if the bot is not active for more than one player
	if(!ActiveStepBot && BotClientID >= MAX_PLAYERS)
	{
		dbg_msg(QUEST_PREFIX_DEBUG, "The mob was found, but the quest step is not active for players.");
		delete pGS->m_apPlayers[BotClientID];
		pGS->m_apPlayers[BotClientID] = nullptr;
	}
}

//Optimized
bool CQuestStepDescription::IsActiveStep(CGS* pGS) const
{
	const int QuestID = GetQuestID();
	const int SubBotID = m_Bot.m_SubBotID;

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = pGS->m_apPlayers[i];
		if(!pPlayer || !pPlayer->IsAuthed())
			continue;

		// invalid data
		if(CQuestDescription::Data().find(QuestID) == CQuestDescription::Data().end())
			continue;

		// skip some quest actions
		CPlayerQuest* pQuest = pPlayer->GetQuest(QuestID);
		if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != m_Bot.m_Step)
			continue;

		// skip some step actions
		CPlayerQuestStep* pStep = pQuest->GetStepByMob(SubBotID);
		if(pStep->m_StepComplete || pStep->m_ClientQuitting)
			continue;

		return true;
	}
	return false;
}
// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
CGS* CPlayerQuestStep::GS() const
{
	return (CGS*)Instance::GetServer()->GameServerPlayer(m_ClientID);
}

CPlayer* CPlayerQuestStep::GetPlayer() const
{
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		return GS()->m_apPlayers[m_ClientID];
	}
	return nullptr;
}

void CPlayerQuestStep::Clear()
{
	m_aMobProgress.clear();
	m_aMoveToProgress.clear();
	m_ClientQuitting = true;
	UpdateBot();

	for(auto& pEnt : m_apEntitiesMoveTo)
	{
		pEnt->ClearPointers();
		pEnt->MarkForDestroy();
	}
	for(auto& pEnt : m_apEntitiesNavigator)
	{
		pEnt->ClearPointers();
		pEnt->MarkForDestroy();
	}

	m_apEntitiesMoveTo.clear();
	m_apEntitiesNavigator.clear();
}

//Optimized
int CPlayerQuestStep::GetNumberBlockedItem(int ItemID) const
{
	int Amount = 0;
	for(auto& p : m_Bot.m_RequiredItems)
	{
		if(p.m_Item.GetID() == ItemID)
			Amount += p.m_Item.GetValue();
	}
	return Amount;
}

bool CPlayerQuestStep::IsComplete()
{
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			if(GetPlayer()->GetItem(p.m_Item)->GetValue() < p.m_Item.GetValue())
				return false;
		}
	}

	if(!m_Bot.m_RequiredDefeat.empty())
	{
		for(auto& [m_BotID, m_Count] : m_Bot.m_RequiredDefeat)
		{
			if(m_aMobProgress[m_BotID].m_Count < m_Count)
				return false;
		}
	}

	if(GetCountMoveToComplected() < (int)m_aMoveToProgress.size())
		return false;

	return true;
}

bool CPlayerQuestStep::Finish()
{
	CPlayer* pPlayer = GetPlayer();

	// quest completion
	if(IsComplete())
	{
		// save file or dissable post finish
		m_StepComplete = true;

		const int QuestID = GetQuestID();
		if(!pPlayer->GetQuest(QuestID)->SaveSteps())
		{
			GS()->Chat(pPlayer->GetCID(), "A system error has occurred, contact administrator.");
			dbg_msg(QUEST_PREFIX_DEBUG, "After completing the quest step, I am unable to save the file.");
			m_StepComplete = false;
		}

		if(m_StepComplete)
		{
			PostFinish();
		}

		return m_StepComplete;
	}

	// quest not yet completed
	return false;
}

void CPlayerQuestStep::PostFinish()
{
	bool AntiDatabaseStress = false;
	CPlayer* pPlayer = GetPlayer();
	int ClientID = pPlayer->GetCID();

	// required item's
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& pRequired : m_Bot.m_RequiredItems)
		{
			// show type element
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequired.m_Item);

			if(pRequired.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW)
			{
				GS()->Chat(pPlayer->GetCID(), "[Done] Show the {STR}x{VAL} to the {STR}!", pPlayerItem->Info()->GetName(), pRequired.m_Item.GetValue(), m_Bot.GetName());
				continue;
			}

			// check for stress database
			if(!m_Bot.m_RewardItems.empty())
			{
				for(auto& pRewardItem : m_Bot.m_RewardItems)
				{
					AntiDatabaseStress = (pRequired.m_Item.GetID() == pRewardItem.GetID());
				}
			}

			// remove item
			pPlayerItem->Remove(pRequired.m_Item.GetValue());
			GS()->Chat(pPlayer->GetCID(), "[Done] Give the {STR}x{VAL} to the {STR}!", pPlayerItem->Info()->GetName(), pRequired.m_Item.GetValue(), m_Bot.GetName());
		}
	}

	// reward item's
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& pRewardItem : m_Bot.m_RewardItems)
		{
			// under stress, add a delay
			if(AntiDatabaseStress)
			{
				GS()->Mmo()->Item()->AddItemSleep(pPlayer->Acc().GetID(), pRewardItem.GetID(), pRewardItem.GetValue(), 300);
				continue;
			}

			// check for enchant item
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRewardItem);
			if(pPlayerItem->Info()->IsEnchantable() && pPlayerItem->GetValue() >= 1)
			{
				GS()->SendInbox("System", pPlayer, "No place for item", "You already have this item, but we can't put it in inventory", pRewardItem.GetID(), 1);
				continue;
			}

			// give item
			pPlayerItem->Add(pRewardItem.GetValue());
		}
	}

	// update bot status
	DataBotInfo::ms_aDataBot[m_Bot.m_BotID].m_aVisibleActive[ClientID] = false;
	UpdateBot();

	pPlayer->GetQuest(GetQuestID())->CheckAvailableNewStep();
	GS()->StrongUpdateVotes(ClientID, MENU_JOURNAL_MAIN);
}

void CPlayerQuestStep::AppendDefeatProgress(int DefeatedBotID)
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(m_StepComplete || m_ClientQuitting || m_Bot.m_RequiredDefeat.empty() || !pPlayer || !DataBotInfo::IsDataBotValid(DefeatedBotID))
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// check complecte mob
	for(auto& [DefeatBotID, DefeatCount] : m_Bot.m_RequiredDefeat)
	{
		if(DefeatedBotID != DefeatBotID || m_aMobProgress[DefeatedBotID].m_Count >= DefeatCount)
			continue;

		m_aMobProgress[DefeatedBotID].m_Count++;
		if(m_aMobProgress[DefeatedBotID].m_Count >= DefeatCount)
		{
			m_aMobProgress[DefeatBotID].m_Complete = true;
			GS()->Chat(pPlayer->GetCID(), "[Done] Defeat the {STR}'s for the {STR}!", DataBotInfo::ms_aDataBot[DefeatedBotID].m_aNameBot, m_Bot.GetName());
		}

		pQuest->SaveSteps();
		break;
	}
}

void CPlayerQuestStep::UpdatePathNavigator()
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(m_StepComplete || m_ClientQuitting || !pPlayer || !pPlayer->GetCharacter() || !m_Bot.m_HasAction)
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// navigator
	pQuest->AddEntityNPCNavigator(&m_Bot);
}

void CPlayerQuestStep::UpdateTaskMoveTo()
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(!m_TaskListReceived || m_StepComplete || m_ClientQuitting || !pPlayer || !pPlayer->GetCharacter())
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// check and mark required mob's
	for(auto& [DefeatBotID, DefeatCount] : m_Bot.m_RequiredDefeat)
	{
		if(m_aMobProgress[DefeatBotID].m_Count >= DefeatCount)
			continue;

		if(const MobBotInfo* pMob = DataBotInfo::FindMobByBot(DefeatBotID))
			AddEntityNavigator(pMob->m_Position, pMob->m_WorldID, 400.f, &m_aMobProgress[DefeatBotID].m_Complete);
	}

	// check and add entities
	if(!m_aMoveToProgress.empty())
	{
		const int CurrentStep = GetMoveToCurrentStepPos();
		for(int i = 0; i < (int)m_Bot.m_RequiredMoveTo.size(); i++)
		{
			// skip completed and not current step's
			QuestBotInfo::TaskRequiredMoveTo& pRequired = m_Bot.m_RequiredMoveTo[i];
			if(CurrentStep != pRequired.m_Step || m_aMoveToProgress[i])
				continue;

			// Always creating navigator in other worlds 
			if(pRequired.m_WorldID != pPlayer->GetPlayerWorldID())
			{
				AddEntityNavigator(pRequired.m_Position, pRequired.m_WorldID, 0.f, &m_aMoveToProgress[i]);
				continue;
			}

			// Add move to point questing mob
			CPlayerBot* pPlayerBot = nullptr;
			if(pRequired.IsHasDefeatMob())
			{
				for(int c = MAX_PLAYERS; c < MAX_CLIENTS; c++)
				{
					CPlayerBot* pPlBotSearch = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[c]);
					if(pPlBotSearch && pPlBotSearch->GetQuestBotMobInfo().m_QuestID == pQuest->GetID() &&
						pPlBotSearch->GetQuestBotMobInfo().m_QuestStep == GetStepPos() &&
						pPlBotSearch->GetQuestBotMobInfo().m_MoveToStep == i)
					{
						pPlayerBot = pPlBotSearch;
						break;
					}
				}

				if(!pPlayerBot)
				{
					int MobClientID = GS()->CreateBot(TYPE_BOT_QUEST_MOB, pRequired.m_DefeatMobInfo.m_BotID, -1);
					pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[MobClientID]);
					pPlayerBot->InitQuestBotMobInfo(
						{
							GetQuestID(),
							GetStepPos(),
							i,
							pRequired.m_DefeatMobInfo.m_AttributePower,
							pRequired.m_DefeatMobInfo.m_AttributeSpread,
							pRequired.m_DefeatMobInfo.m_WorldID,
							pRequired.m_Position
						});

					dbg_msg(QUEST_PREFIX_DEBUG, "Creating a quest mob");
				}

				pPlayerBot->GetQuestBotMobInfo().m_ActiveForClient[pPlayer->GetCID()] = true;
				pPlayerBot->GetQuestBotMobInfo().m_CompleteClient[pPlayer->GetCID()] = false;
			}

			// Check if there is a move-to entity at the required position
			CEntityMoveTo* pEntMoveTo = FoundEntityMoveTo(pRequired.m_Position);
			if(!pEntMoveTo)
			{
				// If there is no move-to entity, create a new one
				pEntMoveTo = AddEntityMoveTo(&pRequired, &m_aMoveToProgress[i], pPlayerBot);

				// Check if the required task is navigation or defeating a mob
				if(!pRequired.m_Navigator || pRequired.m_Type == QuestBotInfo::TaskRequiredMoveTo::Types::DEFEAT_MOB)
				{
					// Create orbital path and navigator for it
					float Radius;
					CLaserOrbite* pEntOrbite;

					// If the required task is to defeat a mob, set a smaller radius for the orbit
					if(pRequired.m_Type == QuestBotInfo::TaskRequiredMoveTo::Types::DEFEAT_MOB)
					{
						Radius = 400.f;
						pEntOrbite = GS()->CreateLaserOrbite(pEntMoveTo, (int)(Radius / 50.f), EntLaserOrbiteType::INSIDE_ORBITE, Radius, LASERTYPE_SHOTGUN, CmaskOne(pPlayer->GetCID()));
					}
					else
					{
						Radius = 400.f + frandom() * 2000.f;
						pEntOrbite = GS()->CreateLaserOrbite(pEntMoveTo, (int)(Radius / 50.f), EntLaserOrbiteType::INSIDE_ORBITE_RANDOM, Radius, LASERTYPE_SHOTGUN, CmaskOne(pPlayer->GetCID()));
					}

					// Add navigator to the orbital path
					AddEntityNavigator(pEntOrbite->GetPos(), pRequired.m_WorldID, Radius, &m_aMoveToProgress[i]);
					continue;
				}

				// Add navigator to the orbital path
				AddEntityNavigator(pRequired.m_Position, pRequired.m_WorldID, 0.f, &m_aMoveToProgress[i]);
			}
		}
	}
}

void CPlayerQuestStep::Update()
{
	UpdateBot();
	UpdatePathNavigator();
	UpdateTaskMoveTo();
}

void CPlayerQuestStep::CreateVarietyTypesRequiredItems()
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(m_StepComplete || m_ClientQuitting || m_Bot.m_RequiredItems.empty() || !pPlayer || !pPlayer->GetCharacter())
		return;

	// check quest action
	CPlayerQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// create variety types
	const int ClientID = pPlayer->GetCID();
	for(auto& [RequiredItem, Type] : m_Bot.m_RequiredItems)
	{
		// TYPE Drop and Pick up
		if(Type == QuestBotInfo::TaskRequiredItems::Type::PICKUP)
		{
			// check whether items are already available for pickup
			for(CDropQuestItem* pHh = (CDropQuestItem*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_DROPQUEST); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
			{
				if(pHh->m_ClientID == ClientID && pHh->m_QuestID == GetQuestID() && pHh->m_ItemID == RequiredItem.GetID() && pHh->m_Step == GetStepPos())
					return;
			}

			// create items
			const int Value = 3 + RequiredItem.GetValue();
			for(int i = 0; i < Value; i++)
			{
				vec2 Vel = vec2(frandom_num(-40.0f, 40.0f), frandom_num(-40.0f, 40.0f));
				float AngleForce = Vel.x * (0.15f + frandom() * 0.1f);
				new CDropQuestItem(&GS()->m_World, m_Bot.m_Position, Vel, AngleForce, RequiredItem.GetID(), RequiredItem.GetValue(), GetQuestID(), GetStepPos(), ClientID);
			}
		}

		// TODO: add new types
	}
}


void CPlayerQuestStep::FormatStringTasks(char* aBufQuestTask, int Size)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	dynamic_string Buffer("");
	const char* pLang = pPlayer->GetLanguage();

	// show required bots
	if(!m_Bot.m_RequiredDefeat.empty())
	{
		Buffer.append_at(Buffer.length(), "\n\n");
		Buffer.append_at(Buffer.length(), GS()->Server()->Localization()->Localize(pLang, "- \u270E Slay enemies:"));
		for(auto& p : m_Bot.m_RequiredDefeat)
		{
			const char* pCompletePrefix = (m_aMobProgress[p.m_BotID].m_Count == p.m_Value ? "\u2611" : "\u2610");

			Buffer.append_at(Buffer.length(), "\n");
			GS()->Server()->Localization()->Format(Buffer, pLang, "{STR} Defeat {STR} ({INT}/{INT})",
				pCompletePrefix, DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, m_aMobProgress[p.m_BotID].m_Count, p.m_Value);
		}
	}

	// show required items
	if(!m_Bot.m_RequiredItems.empty())
	{
		Buffer.append_at(Buffer.length(), "\n\n");
		Buffer.append_at(Buffer.length(), GS()->Server()->Localization()->Localize(pLang, "- \u270E Retrieve an item's:"));
		for(auto& pRequied : m_Bot.m_RequiredItems)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequied.m_Item);
			const char* pCompletePrefix = (pPlayerItem->GetValue() == pRequied.m_Item.GetValue() ? "\u2611" : "\u2610");
			const char* pInteractiveType = pRequied.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW ? "Show a" : "Require a";

			Buffer.append_at(Buffer.length(), "\n");
			GS()->Server()->Localization()->Format(Buffer, pLang, "{STR} {STR} {STR} ({VAL}/{VAL}).",
				pCompletePrefix, GS()->Server()->Localization()->Localize(pLang, pInteractiveType), pPlayerItem->Info()->GetName(), pPlayerItem->GetValue(), pRequied.m_Item.GetValue());
		}
	}

	// show move to
	if(!m_Bot.m_RequiredMoveTo.empty())
	{
		Buffer.append_at(Buffer.length(), "\n\n");
		Buffer.append_at(Buffer.length(), GS()->Server()->Localization()->Localize(pLang, "- \u270E Trigger some action's:"));

		// Create an unordered map called m_Order with key type int and value type unordered_map<string, pair<int, int>> for special order task's
		std::unordered_map<int /* step */, std::unordered_map<std::string /* task name */, std::pair<int /* complected */, int /* count */>>> m_Order;
		m_Order.reserve(m_Bot.m_RequiredMoveTo.size());
		for(int i = 0; i < (int)m_Bot.m_RequiredMoveTo.size(); i++)
		{
			// If TaskMapID is empty, assign it the value "Demands a bit of action"
			std::string TaskMapID = m_Bot.m_RequiredMoveTo[i].m_TaskName;

			// If m_aMoveToProgress[i] is true, increment the first value of the pair in m_Order[Step][TaskMapID]
			int Step = m_Bot.m_RequiredMoveTo[i].m_Step;
			if(m_aMoveToProgress[i])
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
				Buffer.append_at(Buffer.length(), "\n");

				// Check for one task
				const int& TaskNum = StepCount.second;
				const int& TaskCompleted = StepCount.first;
				const char* pCompletePrefix = (StepCount.first == StepCount.second ? "\u2611" : "\u2610");
				if(TaskNum == 1)
				{
					GS()->Server()->Localization()->Format(Buffer, pLang, "{INT}. {STR} {STR}.", Step, pCompletePrefix, Name.c_str());
					continue;
				}

				// Multi task
				GS()->Server()->Localization()->Format(Buffer, pLang, "{INT}. {STR} {STR} ({INT}/{INT}).", Step, pCompletePrefix, Name.c_str(), TaskCompleted, TaskNum);
			}
		}
	}

	// show reward items
	if(!m_Bot.m_RewardItems.empty())
	{
		Buffer.append_at(Buffer.length(), "\n\n");
		Buffer.append_at(Buffer.length(), GS()->Server()->Localization()->Localize(pLang, "- \u270E Reward for completing a task:"));
		for(auto& p : m_Bot.m_RewardItems)
		{
			Buffer.append_at(Buffer.length(), "\n");
			GS()->Server()->Localization()->Format(Buffer, pLang, "Obtain a {STR} ({INT}).", p.Info()->GetName(), p.GetValue());
		}
	}

	// Copy the contents of the buffer `Buffer` into the character array `aBufQuestTask`,
	str_copy(aBufQuestTask, Buffer.buffer(), Size);
	Buffer.clear();
}

int CPlayerQuestStep::GetMoveToNum() const
{
	return m_aMoveToProgress.size();
}

int CPlayerQuestStep::GetMoveToCurrentStepPos() const
{
	for(int i = 0; i < (int)m_Bot.m_RequiredMoveTo.size(); i++)
	{
		if(m_aMoveToProgress[i])
			continue;

		return m_Bot.m_RequiredMoveTo[i].m_Step;
	}

	return 1;
}

int CPlayerQuestStep::GetCountMoveToComplected()
{
	return (int)std::count_if(m_aMoveToProgress.begin(), m_aMoveToProgress.end(), [](const bool State){return State == true; });
}

CEntityMoveTo* CPlayerQuestStep::FoundEntityMoveTo(vec2 Position) const
{
	for(const auto& pMove : m_apEntitiesMoveTo)
	{
		if(pMove && pMove->GetPos() == Position)
			return pMove;
	}

	return nullptr;
}

CEntityPathFinder* CPlayerQuestStep::FoundEntityNavigator(vec2 Position) const
{
	for(const auto& pPath : m_apEntitiesNavigator)
	{
		if(pPath && pPath->GetPosTo() == Position)
			return pPath;
	}

	return nullptr;
}

CEntityMoveTo* CPlayerQuestStep::AddEntityMoveTo(const QuestBotInfo::TaskRequiredMoveTo* pTaskMoveTo, bool* pComplete, CPlayerBot* pDefeatMobPlayer)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || !pTaskMoveTo || !pComplete || (*pComplete) == true)
		return nullptr;

	const int ClientID = pPlayer->GetCID();
	CEntityMoveTo* pEntMoveTo = FoundEntityMoveTo(pTaskMoveTo->m_Position);
	if(!pEntMoveTo)
	{
		pEntMoveTo = m_apEntitiesMoveTo.emplace_back(new CEntityMoveTo(&GS()->m_World, pTaskMoveTo, ClientID, GetQuestID(), pComplete, &m_apEntitiesMoveTo, m_Bot.IsAutoCompletesQuestStep(), pDefeatMobPlayer));
	}

	return pEntMoveTo;
}

CEntityPathFinder* CPlayerQuestStep::AddEntityNavigator(vec2 Position, int WorldID, float AreaClipped, bool* pComplete)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pComplete || (*pComplete) == true)
		return nullptr;

	const int ClientID = pPlayer->GetCID();
	CEntityPathFinder* pEntMoveToFinder = FoundEntityNavigator(Position);
	if(!pEntMoveToFinder)
	{
		pEntMoveToFinder = m_apEntitiesNavigator.emplace_back(new CEntityPathFinder(&GS()->m_World, Position, WorldID, ClientID, AreaClipped, pComplete, &m_apEntitiesNavigator));
	}

	return pEntMoveToFinder;
}
