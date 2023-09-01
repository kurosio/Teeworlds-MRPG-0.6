/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestStepDataInfo.h"

#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include "Entities/quest_path_finder.h"
#include <game/server/mmocore/GameEntities/Items/drop_quest_items.h>

#include <game/server/mmocore/Components/Inventory/InventoryManager.h>
#include "QuestManager.h"

// ##############################################################
// ################# GLOBAL STEP STRUCTURE ######################
/*
 *	.... REQUIRED TYPE .....
 *	required_items
 *	[{
 *		{
 *			id 1
 *			count 20
 *		},
 *		{
 *			id 2
 *			count 25
 *		}
 *	}]
 *
 *	struct TaskRequiredItems
 *	{
 *		int m_ItemID;
 *		int m_Count;
 *	}
 *	std::deque < TaskRequiredItems > m_RequiredItems;
 *
 *	.... COLECT TYPE ....
 *	collect_items
 *	[{
 *		{
 *			id 33
 *			posX 1023
 *			posY 2032
 *		},
 *		{
 *			id 33
 *			posX 1026
 *			posY 2032
 *		}
 *	}]
 *
 *	struct CollectItems
 *	{
 *		int m_ItemID;
 *		vec2 m_Position;
 *	}
 *
 *	std::deque < CollectItems > m_CollectItems;
 *
 *	.... MOVE TYPE .....
 *	move_to
 *	{
 *		posX 2303
 *		posY 5124
 *		arrow 1
 *	}
 *
 *	struct MoveTo
 *	{
 *		vec2 m_MoveTo;
 *		bool m_Arrow;
 *	}
 *
 *	.... DEFEAT TYPE .....
 *	defeat
 *	[{
 *		{
 *			id 23
 *			count 20
 *		}
 *	}]
 *
 *	struct DefeatMob
 *	{
 *		int m_MobID;
 *		int m_Count;
 *	}
 */
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
	const int QuestID = m_Bot.m_QuestID;
	const int SubBotID = m_Bot.m_SubBotID;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = pGS->m_apPlayers[i];
		if(!pPlayer || !pPlayer->IsAuthed())
			continue;

		CQuest* pPlayerQuest = pPlayer->GetQuest(QuestID);
		if(pPlayerQuest->GetState() != QuestState::ACCEPT || m_Bot.m_Step != pPlayerQuest->GetCurrentStep())
			continue;

		// skip complete steps and players who come out to clear the world of bots
		if(pPlayerQuest->GetMobStep(SubBotID)->m_StepComplete || pPlayerQuest->GetMobStep(SubBotID)->m_ClientQuitting)
			continue;

		return true;
	}
	return false;
}

// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
int CPlayerQuestStep::GetValueBlockedItem(CPlayer* pPlayer, int ItemID) const
{
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			if(p.m_ItemID != ItemID)
				continue;
			return p.m_Count;
		}
	}

	return 0;
}

bool CPlayerQuestStep::IsComplete(CPlayer* pPlayer)
{
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			if(pPlayer->GetItem(p.m_ItemID)->GetValue() < p.m_Count)
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
	return true; 
}

bool CPlayerQuestStep::Finish(CPlayer* pPlayer)
{
	// quest completion
	if(IsComplete(pPlayer))
	{
		const int QuestID = m_Bot.m_QuestID;
		int ClientID = pPlayer->GetCID();
		CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

		// save file or dissable post finish
		m_StepComplete = true;
		if(!pPlayer->GetQuest(QuestID)->SaveSteps())
		{
			pGS->Chat(pPlayer->GetCID(), "A system error has occurred, contact administrator.");
			dbg_msg("quest step", "[quest step post finish] can't save file.");
			m_StepComplete = false;
		}

		if(m_StepComplete)
		{
			PostFinish(pPlayer);
		}

		return m_StepComplete;
	}

	// quest not yet completed
	return false;
}

void CPlayerQuestStep::PostFinish(CPlayer* pPlayer)
{
	bool AntiStressing = false;
	int ClientID = pPlayer->GetCID();
	const int QuestID = m_Bot.m_QuestID;
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// required item's
	if(!m_Bot.m_RequiredItems.empty())
	{
		for(auto& p : m_Bot.m_RequiredItems)
		{
			if(p.m_Type == QuestBotInfo::TaskRequiredItems::Type::SHOW)
			{
				pGS->Chat(pPlayer->GetCID(), "[Done] Show the {STR}x{VAL} to the {STR}!", pPlayer->GetItem(p.m_ItemID)->Info()->GetName(), p.m_Count, m_Bot.GetName());
				continue;
			}

			// anti stressing with double thread sql result what work one (item)
			if(!m_Bot.m_RewardItems.empty())
			{
				for(auto& [m_ItemID, m_Count] : m_Bot.m_RewardItems)
				{
					AntiStressing = (p.m_ItemID == m_ItemID);
				}
			}

			pPlayer->GetItem(p.m_ItemID)->Remove(p.m_Count);
			pGS->Chat(pPlayer->GetCID(), "[Done] Give the {STR}x{VAL} to the {STR}!", pPlayer->GetItem(p.m_ItemID)->Info()->GetName(), p.m_Count, m_Bot.GetName());
		}
	}

	// reward item's
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& p : m_Bot.m_RewardItems)
		{
			if(AntiStressing)
			{
				pGS->Mmo()->Item()->AddItemSleep(pPlayer->Acc().m_UserID, p.m_ItemID, p.m_Count, 300);
				continue;
			}

			if(pPlayer->GetItem(p.m_ItemID)->Info()->IsEnchantable() && pPlayer->GetItem(p.m_ItemID)->GetValue() >= 1)
			{
				pGS->SendInbox("System", pPlayer, "No place for item", "You already have this item, but we can't put it in inventory", p.m_ItemID, 1);
				continue;
			}

			pPlayer->GetItem(p.m_ItemID)->Add(p.m_Count);
		}
	}

	// update bot status
	DataBotInfo::ms_aDataBot[m_Bot.m_BotID].m_aVisibleActive[ClientID] = false;
	UpdateBot();

	pPlayer->GetQuest(QuestID)->CheckAvailableNewStep();
	pGS->StrongUpdateVotes(ClientID, MENU_JOURNAL_MAIN);
}

void CPlayerQuestStep::AppendDefeatProgress(CPlayer* pPlayer, int DefeatedBotID)
{
	if(m_Bot.m_RequiredDefeat.empty() || !pPlayer || !DataBotInfo::IsDataBotValid(DefeatedBotID))
		return;

	const int QuestID = m_Bot.m_QuestID;
	if(pPlayer->GetQuest(QuestID)->GetState() != QuestState::ACCEPT)
		return;

	int ClientID = pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// check complecte mob
	for(auto& [m_BotID, m_Count] : m_Bot.m_RequiredDefeat)
	{
		if(DefeatedBotID != m_BotID || m_aMobProgress[DefeatedBotID] >= m_Count)
			continue;

		m_aMobProgress[DefeatedBotID]++;
		if(m_aMobProgress[DefeatedBotID] >= m_Count)
			pGS->Chat(ClientID, "[Done] Defeat the {STR}'s for the {STR}!", DataBotInfo::ms_aDataBot[DefeatedBotID].m_aNameBot, m_Bot.GetName());

		pPlayer->GetQuest(QuestID)->SaveSteps();
		break;
	}
}

void CPlayerQuestStep::UpdatePathNavigator(int ClientID)
{
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);
	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];

	if(m_StepComplete || m_ClientQuitting || !pPlayer || !pPlayer->GetCharacter() || !m_Bot.m_HasAction)
		return;

	CQuest* pQuest = pPlayer->GetQuest(m_Bot.m_QuestID);
	if(pQuest->GetState() == QuestState::ACCEPT && pQuest->GetCurrentStep() == m_Bot.m_Step)
		new CQuestPathFinder(&pGS->m_World, m_Bot.m_Position, ClientID, m_Bot);
}

void CPlayerQuestStep::CreateVarietyTypesRequiredItems(CPlayer* pPlayer)
{
	if(m_Bot.m_RequiredItems.empty() || !pPlayer || !pPlayer->GetCharacter())
		return;

	const int ClientID = pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);
	for(auto& [m_ItemID, m_Count, m_Type] : m_Bot.m_RequiredItems)
	{
		// Drop and pickup type
		if(m_Type == QuestBotInfo::TaskRequiredItems::Type::PICKUP)
		{
			// check whether items are already available for pickup
			for(CDropQuestItem* pHh = (CDropQuestItem*)pGS->m_World.FindFirst(CGameWorld::ENTTYPE_DROPQUEST); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
			{
				if(pHh->m_ClientID == ClientID && pHh->m_QuestID == m_Bot.m_QuestID && pHh->m_ItemID == m_ItemID && pHh->m_Step == m_Bot.m_Step)
					return;
			}

			// create items
			const int Value = 3 + m_Count;
			for(int i = 0; i < Value; i++)
			{
				vec2 Vel = vec2(frandom_num(-40.0f, 40.0f), frandom_num(-40.0f, 40.0f));
				float AngleForce = Vel.x * (0.15f + frandom() * 0.1f);
				new CDropQuestItem(&pGS->m_World, m_Bot.m_Position, Vel, AngleForce, m_ItemID, m_Count, m_Bot.m_QuestID, m_Bot.m_Step, ClientID);
			}
		}
	}
}


void CPlayerQuestStep::FormatStringTasks(CPlayer* pPlayer, char* aBufQuestTask, int Size)
{
	dynamic_string Buffer("\n\n");
	CGS* pGS = pPlayer->GS();

	// show required bots
	if(!m_Bot.m_RequiredDefeat.empty())
	{
		for(auto& p : m_Bot.m_RequiredDefeat)
		{
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Defeat {STR} ({INT}/{INT})", DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, m_aMobProgress[p.m_BotID], p.m_Count);
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
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- {STR} {STR} ({VAL}/{VAL})",
				pGS->Server()->Localization()->Localize(pPlayer->GetLanguage(), pInteractiveType), pPlayerItem->Info()->GetName(), pPlayerItem->GetValue(), p.m_Count);
		}
	}

	// show reward items
	if(!m_Bot.m_RewardItems.empty())
	{
		for(auto& p : m_Bot.m_RewardItems)
		{
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Receive {STR} ({VAL})", pPlayer->GetItem(p.m_ItemID)->Info()->GetName(), p.m_Count);
		}
	}

	str_copy(aBufQuestTask, Buffer.buffer(), Size);
	Buffer.clear();
}
