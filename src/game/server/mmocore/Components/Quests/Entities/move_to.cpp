/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "move_to.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Quests/QuestManager.h"

constexpr float s_Radius = 60.0f;

CEntityMoveTo::CEntityMoveTo(CGameWorld* pGameWorld, vec2 Pos, int ClientID, int QuestID, int CollectItemID, bool* pComplete, std::deque < CEntityMoveTo* >* apCollection)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVE_TO, Pos), m_ClientID(ClientID), m_QuestID(QuestID), m_CollectItemID(CollectItemID)
{
	m_pComplete = pComplete;
	m_apCollection = apCollection;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	GameWorld()->InsertEntity(this);

	GS()->CreateLaserOrbite(this, 4, EntLaserOrbiteType::DEFAULT, 0.0f, s_Radius, CmaskOne(ClientID));
}

void CEntityMoveTo::Destroy()
{
	if(m_pPlayer && m_pPlayer->GetCharacter())
	{
		GS()->Mmo()->Quest()->UpdateSteps(m_pPlayer);
	}

	for(auto it = m_apCollection->begin(); it != m_apCollection->end(); ++it)
	{
		if(mem_comp((*it), this, sizeof(CEntityMoveTo)) == 0)
		{
			m_apCollection->erase(it);
			break;
		}
	}
	GS()->m_World.DestroyEntity(this);
}

bool CEntityMoveTo::PickItem() const
{
	if(m_pPlayer->GetCharacter()->m_ReloadTimer)
	{
		CPlayerItem* pItem = m_pPlayer->GetItem(m_CollectItemID);
		pItem->Add(1);
		GS()->Chat(m_ClientID, "You pick {STR}!", pItem->Info()->GetName());

		m_pPlayer->GetCharacter()->m_ReloadTimer = 0;
		return true;
	}

	GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_INFORMATION, 10, "Press 'Fire' for pick Quest Item");
	return false;
}

void CEntityMoveTo::Tick()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !total_size_vec2(m_PosTo) || !m_pComplete || (*m_pComplete == true))
	{
		Destroy();
		return;
	}

	if(distance(m_pPlayer->m_ViewPos, m_Pos) < s_Radius)
	{
		auto FuncSuccess = [this]()
		{
			(*m_pComplete) = true;
			m_pPlayer->GetQuest(m_QuestID)->SaveSteps();
			GS()->CreateDeath(m_Pos, m_ClientID);
			Destroy();
		};

		// pickup item
		if(m_CollectItemID > 0)
		{
			if(PickItem())
				FuncSuccess();
			return;
		}

		// only move
		FuncSuccess();
	}
}

void CEntityMoveTo::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = POWERUP_ARMOR;
		pPickup->m_Subtype = 0;
	}
}