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
void CQuestStepDataInfo::UpdateBot(CGS* pGS)
{
	CGS* pBotGS = (CGS*)pGS->Server()->GameServer(m_Bot->m_WorldID);
	if(!pBotGS)
		return;

	// check it's if there's a active bot
	int BotClientID = -1;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if(!pBotGS->m_apPlayers[i] || pBotGS->m_apPlayers[i]->GetBotType() != TYPE_BOT_QUEST || pBotGS->m_apPlayers[i]->GetBotSub() != m_Bot->m_SubBotID)
			continue;
		BotClientID = i;
	}

	// seek if all players have an active bot
	const bool ActiveStepBot = IsActiveStep(pGS);
	if(ActiveStepBot && BotClientID <= -1)
	{
		//dbg_msg("quest sync", "quest to step bot active, but mob not found create");
		pBotGS->CreateBot(TYPE_BOT_QUEST, m_Bot->m_BotID, m_Bot->m_SubBotID);
	}
	// if the bot is not active for more than one player
	if(!ActiveStepBot && BotClientID >= MAX_PLAYERS)
	{
		//dbg_msg("quest sync", "mob found, but quest to step not active on players");
		delete pBotGS->m_apPlayers[BotClientID];
		pBotGS->m_apPlayers[BotClientID] = nullptr;
	}
}

bool CQuestStepDataInfo::IsActiveStep(CGS* pGS) const
{
	const int QuestID = m_Bot->m_QuestID;
	const int SubBotID = m_Bot->m_SubBotID;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = pGS->m_apPlayers[i];
		if(!pPlayer || !pPlayer->IsAuthed())
			continue;

		CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
		if(pPlayerQuest.GetState() != QUEST_ACCEPT || m_Bot->m_Step != pPlayerQuest.m_Step)
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
		const int BlockedItemID = m_Bot->m_aItemSearch[i];
		const int BlockedItemValue = m_Bot->m_aItemSearchValue[i];
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
		const int ItemID = m_Bot->m_aItemSearch[i];
		const int Value = m_Bot->m_aItemSearchValue[i];
		if(ItemID <= 0 || Value <= 0)
			continue;
		if(pPlayer->GetItem(ItemID).m_Value < Value)
			return false;
	}
	return true;
}

bool CPlayerQuestStepDataInfo::IsCompleteMobs(CPlayer* pPlayer) const
{
	for(int i = 0; i < 2; i++)
	{
		const int MobID = m_Bot->m_aNeedMob[i];
		const int Value = m_Bot->m_aNeedMobValue[i];
		if(MobID <= 0 || Value <= 0)
			continue;
		if(m_MobProgress[i] < Value)
			return false;
	}
	return true;
}

bool CPlayerQuestStepDataInfo::Finish(CPlayer* pPlayer, bool FinalStepTalking)
{
	CGS* pGS = pPlayer->GS();
	const int ClientID = pPlayer->GetCID();
	const int QuestID = m_Bot->m_QuestID;
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
	DataBotInfo::ms_aDataBot[m_Bot->m_BotID].m_aVisibleActive[ClientID] = false;
	CQuestData::ms_aPlayerQuests[ClientID][QuestID].SaveSteps();
	UpdateBot(pGS);

	CQuestData::ms_aPlayerQuests[ClientID][QuestID].CheckaAvailableNewStep();
	pGS->StrongUpdateVotes(ClientID, MENU_JOURNAL_MAIN);
	return true;
}

void CPlayerQuestStepDataInfo::DoCollectItem(CPlayer* pPlayer)
{
	// anti stressing with double thread sql result what work one (item)
	CGS* pGS = pPlayer->GS();
	bool antiStressing = false;
	for(int i = 0; i < 2; i++)
	{
		const int ItemID = m_Bot->m_aItemSearch[i];
		const int Value = m_Bot->m_aItemSearchValue[i];
		if(ItemID > 0 && Value > 0)
		{
			pGS->Chat(pPlayer->GetCID(), "[Done] Give the {STR}x{INT} to the {STR}!", pPlayer->GetItem(ItemID).Info().GetName(), Value, m_Bot->GetName());
			antiStressing = (bool)(ItemID == m_Bot->m_aItemGives[0] || ItemID == m_Bot->m_aItemGives[1]);
			pPlayer->GetItem(ItemID).Remove(Value);
		}
	}

	for(int i = 0; i < 2; i++)
	{
		const int ItemID = m_Bot->m_aItemGives[i];
		const int Value = m_Bot->m_aItemGivesValue[i];
		if(ItemID > 0 && Value > 0)
		{
			if(antiStressing)
			{
				pGS->Mmo()->Item()->AddItemSleep(pPlayer->Acc().m_UserID, ItemID, Value, 300);
				continue;
			}

			if(pPlayer->GetItem(ItemID).Info().IsEnchantable() && pPlayer->GetItem(ItemID).m_Value >= 1)
			{
				pGS->SendInbox("System", pPlayer, "No place for item", "You already have this item, but we can't put it in inventory", ItemID, 1);
				continue;
			}
			pPlayer->GetItem(ItemID).Add(Value);
		}
	}
}

void CPlayerQuestStepDataInfo::AddMobProgress(CPlayer* pPlayer, int BotID)
{
	const int QuestID = m_Bot->m_QuestID;
	if(!pPlayer || DataBotInfo::ms_aDataBot.find(BotID) == DataBotInfo::ms_aDataBot.end() || pPlayer->GetQuest(QuestID).GetState() != QUEST_ACCEPT)
		return;

	CGS* pGS = pPlayer->GS();
	const int ClientID = pPlayer->GetCID();

	// check complecte mob
	for(int i = 0; i < 2; i++)
	{
		if(BotID != m_Bot->m_aNeedMob[i] || m_MobProgress[i] >= m_Bot->m_aNeedMobValue[i])
			continue;

		m_MobProgress[i]++;
		if(m_MobProgress[i] >= m_Bot->m_aNeedMobValue[i])
			pGS->Chat(ClientID, "[Done] Defeat the {STR}'s for the {STR}!", DataBotInfo::ms_aDataBot[BotID].m_aNameBot, m_Bot->GetName());

		CQuestData::ms_aPlayerQuests[ClientID][QuestID].SaveSteps();
		break;
	}
}

void CPlayerQuestStepDataInfo::CreateStepArrow(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->GetCharacter() || m_StepComplete)
		return;

	if(pPlayer->GetQuest(m_Bot->m_QuestID).GetState() == QUEST_ACCEPT && pPlayer->GetQuest(m_Bot->m_QuestID).m_Step == m_Bot->m_Step)
	{
		CGS* pGS = pPlayer->GS();
		const int ClientID = pPlayer->GetCID();
		new CQuestPathFinder(&pGS->m_World, pPlayer->GetCharacter()->m_Core.m_Pos, ClientID, *m_Bot);
	}
}

void CPlayerQuestStepDataInfo::CreateStepDropTakeItems(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->GetCharacter() || m_Bot->m_InteractiveType != (int)INTERACTIVE_DROP_AND_TAKE_IT)
		return;

	CGS* pGS = pPlayer->GS();
	const int ClientID = pPlayer->GetCID();
	for(CDropQuestItem* pHh = (CDropQuestItem*)pGS->m_World.FindFirst(CGameWorld::ENTTYPE_DROPQUEST); pHh; pHh = (CDropQuestItem*)pHh->TypeNext())
	{
		if(pHh->m_ClientID == ClientID && pHh->m_QuestBot.m_QuestID == m_Bot->m_QuestID)
			return;
	}

	const int Value = 3 + m_Bot->m_aItemSearchValue[0];
	for(int i = 0; i < Value; i++)
	{
		const vec2 Vel = vec2(frandom() * 40.0f - frandom() * 80.0f, frandom() * 40.0f - frandom() * 80.0f);
		const float AngleForce = Vel.x * (0.15f + frandom() * 0.1f);
		new CDropQuestItem(&pGS->m_World, m_Bot->m_Position, Vel, AngleForce, *m_Bot, ClientID);
	}
}


void CPlayerQuestStepDataInfo::ShowRequired(CPlayer* pPlayer, const char* TextTalk)
{
	dynamic_string Buffer;
	CGS* pGS = pPlayer->GS();
	const int ClientID = pPlayer->GetCID();

	if(pGS->IsMmoClient(ClientID))
	{
		// search item's
		for (int i = 0; i < 2; i++)
		{
			const int ItemID = m_Bot->m_aItemSearch[i];
			const int ValueItem = m_Bot->m_aItemSearchValue[i];
			if(ItemID <= 0 || ValueItem <= 0)
				continue;

			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}", pPlayer->GetItem(ItemID).Info().GetName());
			pGS->Mmo()->Quest()->QuestTableAddItem(ClientID, Buffer.buffer(), ValueItem, ItemID, false);
			Buffer.clear();
		}

		// search mob's
		for (int i = 0; i < 2; i++)
		{
			const int BotID = m_Bot->m_aNeedMob[i];
			const int ValueMob = m_Bot->m_aNeedMobValue[i];
			if (BotID <= 0 || ValueMob <= 0 || DataBotInfo::ms_aDataBot.find(BotID) == DataBotInfo::ms_aDataBot.end())
				continue;

			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "Defeat {STR}", DataBotInfo::ms_aDataBot[BotID].m_aNameBot);
			pGS->Mmo()->Quest()->QuestTableAddInfo(ClientID, Buffer.buffer(), ValueMob, m_MobProgress[i]);
			Buffer.clear();
		}

		// reward item's
		for (int i = 0; i < 2; i++)
		{
			const int ItemID = m_Bot->m_aItemGives[i];
			const int ValueItem = m_Bot->m_aItemGivesValue[i];
			if (ItemID <= 0 || ValueItem <= 0)
				continue;

			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "Receive {STR}", pPlayer->GetItem(ItemID).Info().GetName());
			pGS->Mmo()->Quest()->QuestTableAddItem(ClientID, Buffer.buffer(), ValueItem, ItemID, true);
			Buffer.clear();
		}
		return;
	}

	bool IsActiveTask = false;

	// search item's and mob's
	for(int i = 0; i < 2; i++)
	{
		const int BotID = m_Bot->m_aNeedMob[i];
		const int ValueMob = m_Bot->m_aNeedMobValue[i];
		if(BotID > 0 && ValueMob > 0 && DataBotInfo::ms_aDataBot.find(BotID) != DataBotInfo::ms_aDataBot.end())
		{
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Defeat {STR} [{INT}/{INT}]", DataBotInfo::ms_aDataBot[BotID].m_aNameBot, m_MobProgress[i], ValueMob);
			IsActiveTask = true;
		}

		const int ItemID = m_Bot->m_aItemSearch[i];
		const int ValueItem = m_Bot->m_aItemSearchValue[i];
		if(ItemID > 0 && ValueItem > 0)
		{
			CItemData PlayerQuestItem = pPlayer->GetItem(ItemID);
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Need {STR} [{INT}/{INT}]", PlayerQuestItem.Info().GetName(), PlayerQuestItem.m_Value, ValueItem);
			IsActiveTask = true;
		}
	}

	// reward item's
	for(int i = 0; i < 2; i++)
	{
		const int ItemID = m_Bot->m_aItemGives[i];
		const int ValueItem = m_Bot->m_aItemGivesValue[i];
		if(ItemID > 0 && ValueItem > 0)
		{
			Buffer.append_at(Buffer.length(), "\n");
			pGS->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "- Receive {STR} [{INT}]", pPlayer->GetItem(ItemID).Info().GetName(), ValueItem);
		}
	}

	pGS->Motd(ClientID, "{STR}\n\n{STR}{STR}\n\n", TextTalk, (IsActiveTask ? "### Task" : "\0"), Buffer.buffer());
	pPlayer->ClearDialogText();
	Buffer.clear();
}
