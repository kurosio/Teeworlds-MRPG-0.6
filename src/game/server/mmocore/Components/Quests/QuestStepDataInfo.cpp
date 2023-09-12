/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestStepDataInfo.h"

#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include <game/server/mmocore/GameEntities/Items/drop_quest_items.h>
#include <game/server/mmocore/Components/Inventory/InventoryManager.h>
#include "QuestManager.h"

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
		//dbg_msg("quest sync", "quest to step bot active, but mob not found create");
		pGS->CreateBot(TYPE_BOT_QUEST, m_Bot.m_BotID, m_Bot.m_SubBotID);
	}
	// if the bot is not active for more than one player
	if(!ActiveStepBot && BotClientID >= MAX_PLAYERS)
	{
		//dbg_msg("quest sync", "mob found, but quest to step not active on players");
		delete pGS->m_apPlayers[BotClientID];
		pGS->m_apPlayers[BotClientID] = nullptr;
	}
}

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
		CQuest* pQuest = pPlayer->GetQuest(QuestID);
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
	for(auto& pEnt : m_apEntitiesMoveTo)
		delete pEnt;
	for(auto& pEnt : m_apEntitiesNavigator)
		delete pEnt;
	
	m_aMobProgress.clear();
	m_aMoveToProgress.clear();
	m_apEntitiesMoveTo.clear();
	m_apEntitiesNavigator.clear();
	m_ClientQuitting = true;
	UpdateBot();
}

int CPlayerQuestStep::GetNumberBlockedItem(int ItemID) const
{
	int Amount = 0;
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			if(p.m_ItemID == ItemID)
				Amount += p.m_Count;
		}
	}

	return Amount;
}

bool CPlayerQuestStep::IsComplete()
{
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			if(GetPlayer()->GetItem(p.m_ItemID)->GetValue() < p.m_Count)
				return false;
		}
	}

	if(!m_Bot.m_RequiredDefeat.empty())
	{
		for(auto& [m_BotID, m_Count] : m_Bot.m_RequiredDefeat)
		{
			if(m_aMobProgress[m_BotID] < m_Count)
				return false;
		}
	}

	if(!m_aMoveToProgress.empty())
	{
		if(GetCountMoveToComplected() < (int)m_aMoveToProgress.size())
			return false;
	}

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
			dbg_msg("quest step", "[quest step post finish] can't save file.");
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
			if(pRequired.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW)
			{
				GS()->Chat(pPlayer->GetCID(), "[Done] Show the {STR}x{VAL} to the {STR}!", pPlayer->GetItem(pRequired.m_ItemID)->Info()->GetName(), pRequired.m_Count, m_Bot.GetName());
				continue;
			}

			// check for stress database
			if(!m_Bot.m_RewardItems.empty())
			{
				for(auto& [m_ItemID, m_Count] : m_Bot.m_RewardItems)
				{
					AntiDatabaseStress = (pRequired.m_ItemID == m_ItemID);
				}
			}

			// remove item
			pPlayer->GetItem(pRequired.m_ItemID)->Remove(pRequired.m_Count);
			GS()->Chat(pPlayer->GetCID(), "[Done] Give the {STR}x{VAL} to the {STR}!", pPlayer->GetItem(pRequired.m_ItemID)->Info()->GetName(), pRequired.m_Count, m_Bot.GetName());
		}
	}

	// reward item's
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& pReward : m_Bot.m_RewardItems)
		{
			// under stress, add a delay
			if(AntiDatabaseStress)
			{
				GS()->Mmo()->Item()->AddItemSleep(pPlayer->Acc().m_UserID, pReward.m_ItemID, pReward.m_Count, 300);
				continue;
			}

			// check for enchant item
			if(pPlayer->GetItem(pReward.m_ItemID)->Info()->IsEnchantable() && pPlayer->GetItem(pReward.m_ItemID)->GetValue() >= 1)
			{
				GS()->SendInbox("System", pPlayer, "No place for item", "You already have this item, but we can't put it in inventory", pReward.m_ItemID, 1);
				continue;
			}

			// give item
			pPlayer->GetItem(pReward.m_ItemID)->Add(pReward.m_Count);
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
	CQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// check complecte mob
	for(auto& [DefeatBotID, DefeatCount] : m_Bot.m_RequiredDefeat)
	{
		if(DefeatedBotID != DefeatBotID || m_aMobProgress[DefeatedBotID] >= DefeatCount)
			continue;

		m_aMobProgress[DefeatedBotID]++;
		if(m_aMobProgress[DefeatedBotID] >= DefeatCount)
			GS()->Chat(pPlayer->GetCID(), "[Done] Defeat the {STR}'s for the {STR}!", DataBotInfo::ms_aDataBot[DefeatedBotID].m_aNameBot, m_Bot.GetName());

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
	CQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// navigator
	pQuest->AddEntityMobNavigator(&m_Bot);
}

void CPlayerQuestStep::UpdateTaskMoveTo()
{
	// check default action
	CPlayer* pPlayer = GetPlayer();
	if(m_StepComplete || m_ClientQuitting || m_aMoveToProgress.empty() || !pPlayer || !pPlayer->GetCharacter())
		return;

	// only with received task
	if(!m_TaskListReceived)
		return;

	// check quest action
	CQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// check and add entities
	const int CurrentStep = GetMoveToCurrentStepPos();
	for(int i = 0; i < (int)m_Bot.m_RequiredMoveTo.size(); i++)
	{
		// skip completed and not current step's
		QuestBotInfo::TaskRequiredMoveTo& pRequired = m_Bot.m_RequiredMoveTo[i];
		if(CurrentStep != pRequired.m_Step || m_aMoveToProgress[i])
			continue;

		// add entity move to
		if(pRequired.m_WorldID == pPlayer->GetPlayerWorldID())
			AddEntityMoveTo(&pRequired, &m_aMoveToProgress[i]);

		// add entity path navigator
		if(pRequired.m_Navigator)
			AddEntityNavigator(pRequired.m_Position, pRequired.m_WorldID, &m_aMoveToProgress[i]);
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
	CQuest* pQuest = pPlayer->GetQuest(GetQuestID());
	if(pQuest->GetState() != QuestState::ACCEPT || pQuest->GetCurrentStepPos() != GetStepPos())
		return;

	// create variety types
	const int ClientID = pPlayer->GetCID();
	for(auto& [m_ItemID, m_Count, m_Type] : m_Bot.m_RequiredItems)
	{
		// TYPE Drop and Pick up
		if(m_Type == QuestBotInfo::TaskRequiredItems::Type::PICKUP)
		{
			// check whether items are already available for pickup
			for(CDropQuestItem* pHh = (CDropQuestItem*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_DROPQUEST); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
			{
				if(pHh->m_ClientID == ClientID && pHh->m_QuestID == GetQuestID() && pHh->m_ItemID == m_ItemID && pHh->m_Step == GetStepPos())
					return;
			}

			// create items
			const int Value = 3 + m_Count;
			for(int i = 0; i < Value; i++)
			{
				vec2 Vel = vec2(frandom_num(-40.0f, 40.0f), frandom_num(-40.0f, 40.0f));
				float AngleForce = Vel.x * (0.15f + frandom() * 0.1f);
				new CDropQuestItem(&GS()->m_World, m_Bot.m_Position, Vel, AngleForce, m_ItemID, m_Count, GetQuestID(), GetStepPos(), ClientID);
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

	dynamic_string Buffer("\n\n");
	const char* pLang = pPlayer->GetLanguage();

	// show required bots
	if(!m_Bot.m_RequiredDefeat.empty())
	{
		for(auto& p : m_Bot.m_RequiredDefeat)
		{
			Buffer.append_at(Buffer.length(), "\n");
			GS()->Server()->Localization()->Format(Buffer, pLang, "- Defeat {STR} ({INT}/{INT})", DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, m_aMobProgress[p.m_BotID], p.m_Count);
		}
	}

	// show required items
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(p.m_ItemID);
			Buffer.append_at(Buffer.length(), "\n");

			const char* pInteractiveType = p.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW ? "Show" : "Need";
			GS()->Server()->Localization()->Format(Buffer, pLang, "- {STR} {STR} ({VAL}/{VAL})",
				GS()->Server()->Localization()->Localize(pLang, pInteractiveType), pPlayerItem->Info()->GetName(), pPlayerItem->GetValue(), p.m_Count);
		}
	}

	// show reward items
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& p : m_Bot.m_RewardItems)
		{
			Buffer.append_at(Buffer.length(), "\n");
			GS()->Server()->Localization()->Format(Buffer, pLang, "- Receive {STR} ({VAL})", pPlayer->GetItem(p.m_ItemID)->Info()->GetName(), p.m_Count);
		}
	}

	// show move to
	if(!m_Bot.m_RequiredMoveTo.empty())
	{
		Buffer.append_at(Buffer.length(), "\n");
		GS()->Server()->Localization()->Format(Buffer, pLang, "- Requires some action ({VAL}/{VAL})", GetCountMoveToComplected(), m_Bot.m_RequiredMoveTo.size());
	}

	str_copy(aBufQuestTask, Buffer.buffer(), Size);
	Buffer.clear();
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

CEntityMoveTo* CPlayerQuestStep::AddEntityMoveTo(const QuestBotInfo::TaskRequiredMoveTo* pTaskMoveTo, bool* pComplete)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || !pTaskMoveTo || !pComplete || (*pComplete) == true)
		return nullptr;

	const int ClientID = pPlayer->GetCID();
	CEntityMoveTo* pEntMoveTo = FoundEntityMoveTo(pTaskMoveTo->m_Position);
	if(!pEntMoveTo)
		pEntMoveTo = m_apEntitiesMoveTo.emplace_back(new CEntityMoveTo(&GS()->m_World, pTaskMoveTo, ClientID, GetQuestID(), pComplete, &m_apEntitiesMoveTo));

	return pEntMoveTo;
}

CEntityPathFinder* CPlayerQuestStep::AddEntityNavigator(vec2 Position, int WorldID, bool* pComplete)
{
	CPlayer* pPlayer = GetPlayer();
	if(!pComplete || (*pComplete) == true)
		return nullptr;

	const int ClientID = pPlayer->GetCID();
	CEntityPathFinder* pEntMoveToFinder = FoundEntityNavigator(Position);
	if(!pEntMoveToFinder)
		pEntMoveToFinder = m_apEntitiesNavigator.emplace_back(new CEntityPathFinder(&GS()->m_World, Position, WorldID, ClientID, pComplete, &m_apEntitiesNavigator));

	return pEntMoveToFinder;
}
