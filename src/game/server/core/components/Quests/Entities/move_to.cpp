/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "game/server/core/components/Bots/BotData.h"
#include "move_to.h"

#include <game/server/gamecontext.h>

#include "game/server/core/components/Quests/QuestManager.h"

constexpr unsigned int s_Particles = 4;

CEntityQuestAction::CEntityQuestAction(CGameWorld* pGameWorld, const QuestBotInfo::TaskAction& TaskMoveTo, int ClientID, int QuestID, bool* pComplete,
		bool AutoCompletesQuestStep, CPlayerBot* pDefeatMobPlayer)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVE_TO_POINT, TaskMoveTo.m_Position, 32.f, ClientID), m_QuestID(QuestID), m_pTaskMoveTo(&TaskMoveTo)
{
	// Initialize base
	m_Radius = GetProximityRadius();
	m_pComplete = pComplete;
	m_AutoCompletesQuestStep = AutoCompletesQuestStep;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	m_pDefeatMobPlayer = pDefeatMobPlayer;
	GameWorld()->InsertEntity(this);

	// Set the size of the container m_IDs to hold s_Particles number of elements
	m_IDs.set_size(s_Particles);

	// Iterate over each element in m_IDs container
	for(int i = 0; i < m_IDs.size(); i++)
		m_IDs[i] = Server()->SnapNewID();
}

CEntityQuestAction::~CEntityQuestAction()
{
	// Check if m_pPlayer and m_pPlayer's character exist
	if(m_pPlayer)
	{
		// Update the steps of the quest for the player
		if(m_pPlayer->GetCharacter())
			GS()->Core()->QuestManager()->Update(m_pPlayer);
	}

	// Check if m_pDefeatMobPlayer exists
	if(m_pDefeatMobPlayer)
	{
		// Disable the quest progress for the current client
		m_pDefeatMobPlayer->GetQuestBotMobInfo().m_ActiveForClient[m_ClientID] = false;
		m_pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID] = false;

		// Flag to check if m_pDefeatMobPlayer should be cleared
		bool ClearDefeatMobPlayer = true;
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(m_pDefeatMobPlayer->GetQuestBotMobInfo().m_ActiveForClient[i] && !m_pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID])
				ClearDefeatMobPlayer = false;
		}

		// Clear m_pDefeatMobPlayer if ClearDefeatMobPlayer is true
		if(ClearDefeatMobPlayer)
		{
			const int CID = m_pDefeatMobPlayer->GetCID();
			GS()->DestroyPlayer(CID);
			dbg_msg(PRINT_QUEST_PREFIX, "Delete questing mob");
		}
	}

	// Iterate through each element in the m_IDs
	for(int i = 0; i < m_IDs.size(); i++)
		Server()->SnapFreeID(m_IDs[i]);
}

bool CEntityQuestAction::PressedFire() const
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter())
		return false;

	return m_pPlayer->IsClickedKey(KEY_EVENT_FIRE_HAMMER);
}

void CEntityQuestAction::Handler(const std::function<bool()>& pCallbackSuccesful)
{
	if(!pCallbackSuccesful())
		return;

	// Cooldown type
	if(m_pTaskMoveTo->m_Cooldown > 0)
	{
		m_pPlayer->m_Cooldown.Start(m_pTaskMoveTo->m_Cooldown, m_pTaskMoveTo->m_TaskName, std::bind(&CEntityQuestAction::TryFinish, this));
		return;
	}

	// Default try finish
	TryFinish();
}

void CEntityQuestAction::TryFinish()
{
	const QuestBotInfo::TaskAction& TaskData = *m_pTaskMoveTo;
	CPlayerQuest* pQuest = m_pPlayer->GetQuest(m_QuestID);
	CQuestStep* pQuestStep = pQuest->GetStepByMob(TaskData.m_QuestBotID);

	// required item
	if(TaskData.m_Type & QuestBotInfo::TaskAction::Types::REQUIRED_ITEM && TaskData.m_RequiredItem.IsValid())
	{
		ItemIdentifier ItemID = TaskData.m_RequiredItem.GetID();
		int RequiredValue = TaskData.m_RequiredItem.GetValue();

		// check required value
		if(!m_pPlayer->Account()->SpendCurrency(RequiredValue, ItemID))
			return;

		// remove item
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(ItemID);
		GS()->Chat(m_ClientID, "You've used on the point {}x{}", pPlayerItem->Info()->GetName(), RequiredValue);
	}

	// pickup item
	if(TaskData.m_Type & QuestBotInfo::TaskAction::Types::PICKUP_ITEM && TaskData.m_PickupItem.IsValid())
	{
		ItemIdentifier ItemID = TaskData.m_PickupItem.GetID();
		int PickupValue = TaskData.m_PickupItem.GetValue();
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(ItemID);

		GS()->Chat(m_ClientID, "You've picked up {}x{}.", pPlayerItem->Info()->GetName(), PickupValue);
		pPlayerItem->Add(PickupValue);
	}

	// Completion text
	if(!m_pTaskMoveTo->m_CompletionText.empty())
		GS()->Chat(m_ClientID, m_pTaskMoveTo->m_CompletionText.c_str());

	// Set the complete flag to true
	*m_pComplete = true;
	pQuest->m_Datafile.Save();

	// Create a death entity at the current position and destroy this entity
	GS()->CreateDeath(m_Pos, m_ClientID);
	GameWorld()->DestroyEntity(this);
	pQuestStep->m_vpEntitiesAction.erase(std::remove(pQuestStep->m_vpEntitiesAction.begin(), pQuestStep->m_vpEntitiesAction.end(), this), pQuestStep->m_vpEntitiesAction.end());

	// Finish the quest step if AutoCompleteQuestStep is true
	if(m_AutoCompletesQuestStep)
	{
		const bool IsLastElement = (pQuestStep->GetCountMoveToComplected() == pQuestStep->GetMoveToNum());
		if(IsLastElement && pQuestStep->IsComplete())
			pQuestStep->Finish();
	}
}

void CEntityQuestAction::Tick()
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
	if(Type & QuestBotInfo::TaskAction::Types::DEFEAT_MOB)
		m_Radius = 600.f;

	// check distance to check complete
	if(distance(m_pPlayer->m_ViewPos, m_Pos) > m_Radius)
		return;

	// Check if the Type includes the DEFEAT_MOB flag
	if(Type & QuestBotInfo::TaskAction::Types::DEFEAT_MOB)
	{
		Handler([this]
		{
			// Complete the task by defeating a mob
			return m_pDefeatMobPlayer && m_pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID];
		});
	}
	// Check if the Type includes the MOVE_ONLY flag
	else if(Type & QuestBotInfo::TaskAction::Types::MOVE_ONLY)
	{
		Handler([this]
		{
			// Complete the task by only moving
			return true;
		});
	}
	// Check if the Type includes the INTERACTIVE flag
	else if(Type & QuestBotInfo::TaskAction::Types::INTERACTIVE)
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
			GS()->CreateHammerHit(m_pTaskMoveTo->m_Interaction.m_Position, CmaskOne(m_ClientID));

		Handler([this]
		{
			// Complete the task by interacting
			return PressedFire() && distance(m_pPlayer->GetCharacter()->GetMousePos(), m_pTaskMoveTo->m_Interaction.m_Position) < 48.f;
		});
	}
	// Check if the Type includes the PICKUP_ITEM or REQUIRED_ITEM flags
	else if(Type & (QuestBotInfo::TaskAction::PICKUP_ITEM | QuestBotInfo::TaskAction::REQUIRED_ITEM))
	{
		Handler([this]
		{
			// Complete the task by pressing "fire" button
			return PressedFire();
		});
	}

	// handle broadcast
	HandleBroadcastInformation();
}

void CEntityQuestAction::HandleBroadcastInformation() const
{
	// in case the quest ended in Handler
	if(!m_pTaskMoveTo)
		return;

	// var
	auto& pPickupItem = m_pTaskMoveTo->m_PickupItem;
	auto& pRequireItem = m_pTaskMoveTo->m_RequiredItem;
	const auto Type = m_pTaskMoveTo->m_Type;

	// defeat mob skip
	if(Type & QuestBotInfo::TaskAction::Types::DEFEAT_MOB)
		return;

	// text information
	std::string strBuffer{};
	if(pRequireItem.IsValid())
	{
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(pRequireItem.GetID());
		strBuffer += fmt_handle(m_ClientID, "- Required [{}x{}({})]", pPlayerItem->Info()->GetName(), pRequireItem.GetValue(), pPlayerItem->GetValue()) + "\n";
	}
	if(pPickupItem.IsValid())
	{
		CPlayerItem* pPlayerItem = m_pPlayer->GetItem(pPickupItem.GetID());
		strBuffer += fmt_handle(m_ClientID, "- Pick up [{}x{}({})]", pPlayerItem->Info()->GetName(), pPickupItem.GetValue(), pPlayerItem->GetValue()) + "\n";
	}

	// select by type
	if(Type & QuestBotInfo::TaskAction::Types::INTERACTIVE)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 10, "Please click with hammer on the highlighted area to interact with it.\n{}", strBuffer.c_str());
	}
	else if(Type & QuestBotInfo::TaskAction::Types::PICKUP_ITEM || Type & QuestBotInfo::TaskAction::Types::REQUIRED_ITEM)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 10, "Press hammer 'Fire', to interact.\n{}", strBuffer.c_str());
	}
}

void CEntityQuestAction::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	for(int i = 0; i < m_IDs.size(); i++)
	{
		CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_Type = WEAPON_HAMMER;
		pObj->m_X = m_Pos.x + random_float(-m_Radius / 1.5f, m_Radius / 1.5f);
		pObj->m_Y = m_Pos.y + random_float(-m_Radius / 1.5f, m_Radius / 1.5f);
		pObj->m_StartTick = Server()->Tick() - 3;
	}
}