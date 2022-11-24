/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestStepDataInfo.h"

#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include <game/server/mmocore/GameEntities/quest_path_finder.h>
#include <game/server/mmocore/GameEntities/Items/drop_quest_items.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>
#include "QuestCore.h"

// ##############################################################
// ################# GLOBAL STEP STRUCTURE ######################
void CQuestStepDataInfo::UpdateBot()
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

bool CQuestStepDataInfo::IsActiveStep(CGS* pGS) const
{
	const int QuestID = m_Bot.m_QuestID;
	const int SubBotID = m_Bot.m_SubBotID;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = pGS->m_apPlayers[i];
		if(!pPlayer || !pPlayer->IsAuthed())
			continue;

		CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
		if(pPlayerQuest.GetState() != QuestState::ACCEPT || m_Bot.m_Step != pPlayerQuest.m_Step)
			continue;

		// skip complete steps and players who come out to clear the world of bots
		if(pPlayerQuest.m_StepsQuestBot[SubBotID].m_StepComplete || pPlayerQuest.m_StepsQuestBot[SubBotID].m_ClientQuitting)
			continue;

		return true;
	}
	return false;
}

// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
int CPlayerQuestStepDataInfo::GetValueBlockedItem(CPlayer* pPlayer, int ItemID) const
{
	for(int i = 0; i < 2; i++)
	{
		const int BlockedItemID = m_Bot.m_aItemSearch[i];
		const int BlockedItemValue = m_Bot.m_aItemSearchValue[i];
		if(BlockedItemID <= 0 || BlockedItemValue <= 0 || ItemID != BlockedItemID)
			continue;
		return BlockedItemValue;
	}
	return 0;
}

bool CPlayerQuestStepDataInfo::IsCompleteItems(CPlayer* pPlayer) const
{
	for(int i = 0; i < 2; i++)
	{
		const int ItemID = m_Bot.m_aItemSearch[i];
		const int Value = m_Bot.m_aItemSearchValue[i];
		if(ItemID <= 0 || Value <= 0)
			continue;
		if(pPlayer->GetItem(ItemID)->GetValue() < Value)
			return false;
	}
	return true;
}

bool CPlayerQuestStepDataInfo::IsCompleteMobs(CPlayer* pPlayer) const
{
	for(int i = 0; i < 2; i++)
	{
		const int MobID = m_Bot.m_aNeedMob[i];
		const int Value = m_Bot.m_aNeedMobValue[i];
		if(MobID <= 0 || Value <= 0)
			continue;
		if(m_MobProgress[i] < Value)
			return false;
	}
	return true;
}

bool CPlayerQuestStepDataInfo::Finish(CPlayer* pPlayer, bool FinalStepTalking)
{
	int ClientID = pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	const int QuestID = m_Bot.m_QuestID;
	if(!IsCompleteItems(pPlayer) || !IsCompleteMobs(pPlayer))
	{
		pGS->Chat(ClientID, "Task has not been completed yet!");
		return false;
	}

	if(!FinalStepTalking)
	{
		pGS->CreatePlayerSound(ClientID, SOUND_CTF_RETURN);
		return true;
	}

	DoCollectItem(pPlayer);

	// update state complete
	m_StepComplete = true;
	DataBotInfo::ms_aDataBot[m_Bot.m_BotID].m_aVisibleActive[ClientID] = false;
	CQuestData::ms_aPlayerQuests[ClientID][QuestID].SaveSteps();
	UpdateBot();

	CQuestData::ms_aPlayerQuests[ClientID][QuestID].CheckAvailableNewStep();
	pGS->StrongUpdateVotes(ClientID, MENU_JOURNAL_MAIN);
	return true;
}

void CPlayerQuestStepDataInfo::DoCollectItem(CPlayer* pPlayer)
{
	bool antiStressing = false;
	int ClientID = pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// type of show TODO: update structures and checker
	if(m_Bot.m_InteractiveType == (int)INTERACTIVE_SHOW_ITEMS)
	{
		for(int i = 0; i < 2; i++)
		{
			const int ItemID = m_Bot.m_aItemSearch[i];
			const int Value = m_Bot.m_aItemSearchValue[i];
			if(ItemID > 0 && Value > 0)
				pGS->Chat(pPlayer->GetCID(), "[Done] Show the {STR}x{VAL} to the {STR}!", pPlayer->GetItem(ItemID)->Info()->GetName(), Value, m_Bot.GetName());
		}
	}
	else
	{
		// anti stressing with double thread sql result what work one (item)
		for(int i = 0; i < 2; i++)
		{
			const int ItemID = m_Bot.m_aItemSearch[i];
			const int Value = m_Bot.m_aItemSearchValue[i];
			if(ItemID > 0 && Value > 0)
			{
				pGS->Chat(pPlayer->GetCID(), "[Done] Give the {STR}x{VAL} to the {STR}!", pPlayer->GetItem(ItemID)->Info()->GetName(), Value, m_Bot.GetName());
				antiStressing = ItemID == m_Bot.m_aItemGives[0] || ItemID == m_Bot.m_aItemGives[1];
				pPlayer->GetItem(ItemID)->Remove(Value);
			}
		}
	}

	for(int i = 0; i < 2; i++)
	{
		const int ItemID = m_Bot.m_aItemGives[i];
		const int Value = m_Bot.m_aItemGivesValue[i];
		if(ItemID > 0 && Value > 0)
		{
			if(antiStressing)
			{
				pGS->Mmo()->Item()->AddItemSleep(pPlayer->Acc().m_UserID, ItemID, Value, 300);
				continue;
			}

			if(pPlayer->GetItem(ItemID)->Info()->IsEnchantable() && pPlayer->GetItem(ItemID)->GetValue() >= 1)
			{
				pGS->SendInbox("System", pPlayer, "No place for item", "You already have this item, but we can't put it in inventory", ItemID, 1);
				continue;
			}
			pPlayer->GetItem(ItemID)->Add(Value);
		}
	}
}

void CPlayerQuestStepDataInfo::AddMobProgress(CPlayer* pPlayer, int BotID)
{
	const int QuestID = m_Bot.m_QuestID;
	if(!pPlayer || DataBotInfo::ms_aDataBot.find(BotID) == DataBotInfo::ms_aDataBot.end() || pPlayer->GetQuest(QuestID).GetState() != QuestState::ACCEPT)
		return;

	int ClientID = pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// check complecte mob
	for(int i = 0; i < 2; i++)
	{
		if(BotID != m_Bot.m_aNeedMob[i] || m_MobProgress[i] >= m_Bot.m_aNeedMobValue[i])
			continue;

		m_MobProgress[i]++;
		if(m_MobProgress[i] >= m_Bot.m_aNeedMobValue[i])
			pGS->Chat(ClientID, "[Done] Defeat the {STR}'s for the {STR}!", DataBotInfo::ms_aDataBot[BotID].m_aNameBot, m_Bot.GetName());

		CQuestData::ms_aPlayerQuests[ClientID][QuestID].SaveSteps();
		break;
	}
}

void CPlayerQuestStepDataInfo::CreateStepArrow(int ClientID)
{
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);
	CPlayer* pPlayer = pGS->m_apPlayers[ClientID];

	if(!pPlayer || !pPlayer->GetCharacter() || m_StepComplete || !m_Bot.m_HasAction)
		return;

	if(pPlayer->GetQuest(m_Bot.m_QuestID).GetState() == QuestState::ACCEPT && pPlayer->GetQuest(m_Bot.m_QuestID).m_Step == m_Bot.m_Step)
		new CQuestPathFinder(&pGS->m_World, pPlayer->GetCharacter()->m_Core.m_Pos, ClientID, m_Bot);
}

void CPlayerQuestStepDataInfo::CreateStepDropTakeItems(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->GetCharacter() || m_Bot.m_InteractiveType != (int)INTERACTIVE_DROP_AND_TAKE_IT)
		return;

	const int ClientID = pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);
	
	for(CDropQuestItem* pHh = (CDropQuestItem*)pGS->m_World.FindFirst(CGameWorld::ENTTYPE_DROPQUEST); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
	{
		if(pHh->m_ClientID == ClientID && pHh->m_QuestBot.m_QuestID == m_Bot.m_QuestID)
			return;
	}

	const int Value = 3 + m_Bot.m_aItemSearchValue[0];
	for(int i = 0; i < Value; i++)
	{
		const vec2 Vel = vec2(frandom() * 40.0f - frandom() * 80.0f, frandom() * 40.0f - frandom() * 80.0f);
		const float AngleForce = Vel.x * (0.15f + frandom() * 0.1f);
		new CDropQuestItem(&pGS->m_World, m_Bot.m_Position, Vel, AngleForce, m_Bot, ClientID);
	}
}


void CPlayerQuestStepDataInfo::ShowRequired(CPlayer* pPlayer, const char* pBuffer)
{
	dynamic_string Buffer;
	CGS* pGS = pPlayer->GS();
	const int ClientID = pPlayer->GetCID();

	bool IsActiveTask = false;

	// search item's and mob's
	for(int i = 0; i < 2; i++)
	{
		const int BotID = m_Bot.m_aNeedMob[i];
		const int ValueMob = m_Bot.m_aNeedMobValue[i];
		if(BotID > 0 && ValueMob > 0 && DataBotInfo::ms_aDataBot.find(BotID) != DataBotInfo::ms_aDataBot.end())
		{
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Defeat {STR} ({INT}/{INT})", DataBotInfo::ms_aDataBot[BotID].m_aNameBot, m_MobProgress[i], ValueMob);
			IsActiveTask = true;
		}

		const int ItemID = m_Bot.m_aItemSearch[i];
		const int ValueItem = m_Bot.m_aItemSearchValue[i];
		if(ItemID > 0 && ValueItem > 0)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID);
			Buffer.append_at(Buffer.length(), "\n");

			const char* pInteractiveType = m_Bot.m_InteractiveType == (int)INTERACTIVE_SHOW_ITEMS ? "Show" : "Need";
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- {STR} {STR} ({VAL}/{VAL})", 
				pGS->Server()->Localization()->Localize(pPlayer->GetLanguage(), pInteractiveType), pPlayerItem->Info()->GetName(), pPlayerItem->GetValue(), ValueItem);

			IsActiveTask = true;
		}
	}

	// reward item's
	for(int i = 0; i < 2; i++)
	{
		const int ItemID = m_Bot.m_aItemGives[i];
		const int ValueItem = m_Bot.m_aItemGivesValue[i];
		if(ItemID > 0 && ValueItem > 0)
		{
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Receive {STR} ({VAL})", pPlayer->GetItem(ItemID)->Info()->GetName(), ValueItem);
		}
	}

	pGS->Motd(ClientID, "{STR}\n\n{STR}{STR}\n\n", pBuffer, (IsActiveTask ? "~~ Task" : "\0"), Buffer.buffer());
	pPlayer->ClearDialogText();
	Buffer.clear();
}
