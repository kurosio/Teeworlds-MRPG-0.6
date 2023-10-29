/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "game/server/mmocore/Components/Bots/BotData.h"
#include "move_to.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Quests/QuestManager.h"

constexpr unsigned int s_Particles = 4;

CEntityMoveTo::CEntityMoveTo(CGameWorld* pGameWorld, const QuestBotInfo::TaskRequiredMoveTo* pTaskMoveTo, int ClientID, int QuestID, bool* pComplete,
	std::deque < CEntityMoveTo* >* apCollection, bool AutoCompletesQuestStep, CPlayerBot* pDefeatMobPlayer)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVE_TO, pTaskMoveTo->m_Position, 32.f), m_QuestID(QuestID), m_ClientID(ClientID), m_pTaskMoveTo(pTaskMoveTo)
{
	// Initialize base
	m_Radius = GetProximityRadius();
	m_pComplete = pComplete;
	m_apCollection = apCollection;
	m_AutoCompletesQuestStep = AutoCompletesQuestStep;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	m_pDefeatMobPlayer = pDefeatMobPlayer;
	GameWorld()->InsertEntity(this);

	// Set the size of the container m_IDs to hold s_Particles number of elements
	m_IDs.set_size(s_Particles);

	// Iterate over each element in m_IDs container
	for(int i = 0; i < m_IDs.size(); i++)
		// Assign a new ID to each element in m_IDs using Server()'s SnapNewID() function
		m_IDs[i] = Server()->SnapNewID();
}

CEntityMoveTo::~CEntityMoveTo()
{
	// Check if m_pPlayer and m_pPlayer's character exist
	if(m_pPlayer && m_pPlayer->GetCharacter())
	{
		// Update the steps of the quest for the player
		GS()->Mmo()->Quest()->UpdateSteps(m_pPlayer);
	}

	// Check if m_pDefeatMobPlayer exists
	if(m_pDefeatMobPlayer)
	{
		// Disable the quest progress for the current client
		m_pDefeatMobPlayer->GetQuestBotMobInfo().m_ActiveForClient[m_ClientID] = false;
		m_pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID] = false;

		// Flag to check if m_pDefeatMobPlayer should be cleared
		bool ClearDefeatMobPlayer = true;

		// Iterate through each player
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			// Check if this player is active for the client 
			// and the client has not completed it yet
			if(m_pDefeatMobPlayer->GetQuestBotMobInfo().m_ActiveForClient[i] &&
				!m_pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID])
			{
				// Set the flag to false since there is at least one player who
				// has not completed the defeat mob quest
				ClearDefeatMobPlayer = false;
			}
		}

		// Clear m_pDefeatMobPlayer if ClearDefeatMobPlayer is true
		if(ClearDefeatMobPlayer)
		{
			const int CID = m_pDefeatMobPlayer->GetCID();
			delete GS()->m_apPlayers[CID];
			GS()->m_apPlayers[CID] = nullptr;

			dbg_msg("test", "DELETE MOVE TO MOB");
		}
	}

	// Check if m_apCollection is not null and is not empty
	if(m_apCollection && !m_apCollection->empty())
	{
		// Iterate through all elements in m_apCollection
		for(auto it = m_apCollection->begin(); it != m_apCollection->end(); ++it)
		{
			// Compare the current element (*it) with this object (CEntityMoveTo)
			// using the mem_comp function and size of CEntityMoveTo
			if(mem_comp((*it), this, sizeof(CEntityMoveTo)) == 0)
			{
				// If the comparison is equal, erase the element from m_apCollection
				m_apCollection->erase(it);
				break; // Exit the loop
			}
		}
	}

	// Iterate through each element in the m_IDs
	for(int i = 0; i < m_IDs.size(); i++)
	{
		// Free the ID associated with the current element
		Server()->SnapFreeID(m_IDs[i]);
	}
}

bool CEntityMoveTo::PressedFire() const
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter())
		return false;

	if(m_pPlayer->GetCharacter()->m_ReloadTimer)
	{
		m_pPlayer->GetCharacter()->m_ReloadTimer = 0;
		return true;
	}

	return false;
}

void CEntityMoveTo::Handler(const QuestBotInfo::TaskRequiredMoveTo& TaskData, const std::function<bool()> pCallbackSuccesful)
{
	CQuest* pPlayerQuest = m_pPlayer->GetQuest(m_QuestID);
	CPlayerQuestStep* pPlayerQuestStep = pPlayerQuest->GetStepByMob(TaskData.m_QuestBotID);
	bool FailedFinish = !pCallbackSuccesful();
	const bool IsLastElement = (pPlayerQuestStep->GetCountMoveToComplected() == (pPlayerQuestStep->GetMoveToNum() - 1));
	const bool AutoCompleteQuestStep = (m_AutoCompletesQuestStep ? IsLastElement : false);

	// in case move it completes a quest step
	if(!FailedFinish && AutoCompleteQuestStep)
	{
		// START for correct try check current quest step
		(*m_pComplete) = true;

		// check quest state
		if(!pPlayerQuestStep->IsComplete())
		{
			char aBufQuestTask[256] {};
			GS()->Mmo()->Quest()->QuestShowRequired(m_pPlayer, QuestBotInfo::ms_aQuestBot[TaskData.m_QuestBotID], aBufQuestTask, sizeof(aBufQuestTask));
			str_append(aBufQuestTask, "\n### List of tasks to be completed. ###", sizeof(aBufQuestTask));
			GS()->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 100, aBufQuestTask);
			GS()->Chat(m_ClientID, "The tasks haven't been completed yet!");
			FailedFinish = true;
		}

		// END for correct try check current quest step
		(*m_pComplete) = false;
	}

	if(!FailedFinish)
	{
		// required item
		if(TaskData.m_Type & QuestBotInfo::TaskRequiredMoveTo::Types::REQUIRED_ITEM && TaskData.m_RequiredItem.IsValid())
		{
			ItemIdentifier ItemID = TaskData.m_RequiredItem.GetID();
			int RequiredValue = TaskData.m_RequiredItem.GetValue();

			// check required value
			if(!m_pPlayer->SpendCurrency(RequiredValue, ItemID))
				return;

			// remove item
			CPlayerItem* pPlayerItem = m_pPlayer->GetItem(ItemID);
			GS()->Chat(m_ClientID, "You've used on the point {STR}x{INT}", pPlayerItem->Info()->GetName(), RequiredValue);
		}

		// pickup item
		if(TaskData.m_Type & QuestBotInfo::TaskRequiredMoveTo::Types::PICKUP_ITEM && TaskData.m_PickupItem.IsValid())
		{
			ItemIdentifier ItemID = TaskData.m_PickupItem.GetID();
			int PickupValue = TaskData.m_PickupItem.GetValue();
			CPlayerItem* pPlayerItem = m_pPlayer->GetItem(ItemID);

			GS()->Chat(m_ClientID, "You've picked up {STR}x{INT}.", pPlayerItem->Info()->GetName(), PickupValue);
			pPlayerItem->Add(PickupValue);
		}

		// finish success
		if(!m_pTaskMoveTo->m_CompletionText.empty())
		{
			GS()->Chat(m_ClientID, m_pTaskMoveTo->m_CompletionText.c_str());
		}

		(*m_pComplete) = true;
		pPlayerQuest->SaveSteps();
		GS()->CreateDeath(m_Pos, m_ClientID);
		GameWorld()->DestroyEntity(this);

		// finish quest step
		if(AutoCompleteQuestStep)
		{
			pPlayerQuestStep->Finish();
		}

		// if quest is completed, reset task and collection pointers they're cleared in the quest data
		if(pPlayerQuest->IsCompleted())
		{
			m_pTaskMoveTo = nullptr;
			m_apCollection = nullptr;
		}
	}
}

void CEntityMoveTo::Tick()
{
	// Check if any of the required variables are null or if the completion flag is true
	if(!m_pTaskMoveTo || !m_pPlayer || !m_pPlayer->GetCharacter() || !m_pComplete || (*m_pComplete == true))
	{
		// Destroy the entity and exit the function
		GameWorld()->DestroyEntity(this);
		return;
	}

	// Check the type of the required move task
	const unsigned Type = m_pTaskMoveTo->m_Type;

	// distance for defeat mob larger
	if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::DEFEAT_MOB)
		m_Radius = 600.f;

	// check distance to check complete
	if(distance(m_pPlayer->m_ViewPos, m_Pos) > m_Radius)
		return;

	// Check if the Type includes the DEFEAT_MOB flag
	if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::DEFEAT_MOB)
	{
		Handler(*m_pTaskMoveTo, [this]
		{
			// Complete the task by defeating a mob
			return m_pDefeatMobPlayer && m_pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID];
		});
	}
	// Check if the Type includes the MOVE_ONLY flag
	else if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::MOVE_ONLY)
	{
		Handler(*m_pTaskMoveTo, [this]
		{
			// Complete the task by only moving
			return true;
		});
	}
	// Check if the Type includes the INTERACTIVE flag
	else if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::INTERACTIVE)
	{
		// mark
		if(Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
		{
			GS()->CreateHammerHit(m_pTaskMoveTo->m_Interaction.m_Position, CmaskOne(m_ClientID));
		}

		Handler(*m_pTaskMoveTo, [this]
		{
			// Complete the task by interacting
			return PressedFire() && distance(m_pPlayer->GetCharacter()->GetMousePos(), m_pTaskMoveTo->m_Interaction.m_Position) < 48.f;
		});
	}
	// Check if the Type includes the PICKUP_ITEM or REQUIRED_ITEM flags
	else if(Type & (QuestBotInfo::TaskRequiredMoveTo::PICKUP_ITEM | QuestBotInfo::TaskRequiredMoveTo::REQUIRED_ITEM))
	{
		Handler(*m_pTaskMoveTo, [this]
		{
			// Complete the task by pressing "fire" button
			return PressedFire();
		});
	}

	// handle broadcast
	HandleBroadcastInformation();
}

void CEntityMoveTo::HandleBroadcastInformation() const
{
	// in case the quest ended in Handler
	if(!m_pTaskMoveTo)
		return;

	// var
	auto& pPickupItem = m_pTaskMoveTo->m_PickupItem;
	auto& pRequireItem = m_pTaskMoveTo->m_RequiredItem;
	const auto Type = m_pTaskMoveTo->m_Type;

	// defeat mob skip
	if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::DEFEAT_MOB)
		return;

	// text information
	dynamic_string Buffer;
	if(pRequireItem.IsValid())
	{
		const char* pLang = m_pPlayer->GetLanguage();
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(pRequireItem.GetID());

		GS()->Server()->Localization()->Format(Buffer, pLang, "- Required [{STR}x{VAL}({VAL})]", pPlayerItem->Info()->GetName(), pRequireItem.GetValue(), pPlayerItem->GetValue());
		Buffer.append("\n");
	}
	if(pPickupItem.IsValid())
	{
		const char* pLang = m_pPlayer->GetLanguage();
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(pPickupItem.GetID());

		GS()->Server()->Localization()->Format(Buffer, pLang, "- Pick up [{STR}x{VAL}({VAL})]", pPlayerItem->Info()->GetName(), pPickupItem.GetValue(), pPlayerItem->GetValue());
		Buffer.append("\n");
	}

	// select by type
	if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::INTERACTIVE)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 10, "Click on the highlighted area to interact with it.\n{STR}", Buffer.buffer());
	}
	else if(Type & QuestBotInfo::TaskRequiredMoveTo::Types::PICKUP_ITEM || Type & QuestBotInfo::TaskRequiredMoveTo::Types::REQUIRED_ITEM)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 10, "Press 'Fire', to interact.\n{STR}", Buffer.buffer());
	}
}

void CEntityMoveTo::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	for(int i = 0; i < m_IDs.size(); i++)
	{
		CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_Type = WEAPON_HAMMER;
		pObj->m_X = m_Pos.x + frandom_num(-m_Radius / 1.5f, m_Radius / 1.5f);
		pObj->m_Y = m_Pos.y + frandom_num(-m_Radius / 1.5f, m_Radius / 1.5f);
		pObj->m_StartTick = Server()->Tick() - 3;
	}
}