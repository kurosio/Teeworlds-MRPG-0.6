/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "jobitems.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Accounts/AccountMinerManager.h>
#include <game/server/core/components/Accounts/AccountPlantManager.h>
#include <game/server/core/components/houses/house_manager.h>

// 1 - miner / 2 - plant
CJobItems::CJobItems(CGameWorld *pGameWorld, int ItemID, int Level, vec2 Pos, int Type, int Health, int HouseID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_JOBITEMS, Pos, PickupPhysSize)
{
	m_ItemID = ItemID;
	m_Level = Level;
	m_Type = Type;
	m_Health = Health;
	m_DamageDealt = 0;
	m_HouseID = HouseID;
	SpawnPositions();

	CJobItems::Reset();
	GameWorld()->InsertEntity(this);
}

void CJobItems::SpawnPositions()
{
	vec2 SwapPos = vec2(m_Pos.x, m_Pos.y-20);
	if (GS()->Collision()->GetCollisionAt(SwapPos.x, SwapPos.y) > 0)
		m_Pos = SwapPos;
	SwapPos = vec2(m_Pos.x, m_Pos.y+16);
	if (GS()->Collision()->GetCollisionAt(SwapPos.x, SwapPos.y) > 0)
		m_Pos = SwapPos;
	SwapPos = vec2(m_Pos.x-18, m_Pos.y);
	if (GS()->Collision()->GetCollisionAt(SwapPos.x, SwapPos.y) > 0)
		m_Pos = SwapPos;
	SwapPos = vec2(m_Pos.x+18, m_Pos.y);
	if (GS()->Collision()->GetCollisionAt(SwapPos.x, SwapPos.y) > 0)
		m_Pos = SwapPos;
}

void CJobItems::SetSpawn(int Sec)
{
	m_SpawnTick = Server()->Tick() + (Server()->TickSpeed()*Sec);
	m_DamageDealt = m_Health;
}

void CJobItems::Work(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || m_DamageDealt >= m_Health)
		return;

	// not allowed un owner house job
	CHouseData* pHouse = GS()->Core()->HouseManager()->GetHouse(m_HouseID);
	if(pHouse && !pHouse->HasOwner())
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "It is forbidden to pick plants without the owner!");
		return;
	}

	CPlayerItem* pPlayerItem = pPlayer->GetItem(m_ItemID);
	if(m_Type == JOB_ITEM_MINING)
		MiningWork(ClientID, pPlayer, *pPlayerItem);
	else if(m_Type == JOB_ITEM_FARMING)
		FarmingWork(ClientID, pPlayer, *pPlayerItem);
}

bool CJobItems::Interaction(const char* pTool, AttributeIdentifier AttributeDmg, CPlayer* pPlayer, const CPlayerItem* pWorkedItem, ItemFunctional EquipID, int JobLevel)
{
	const int ClientID = pPlayer->GetCID();
	const int EquipItem = pPlayer->GetEquippedItemID(EquipID);

	// check equipped
	if(EquipItem <= 0)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Need equip {}!", Server()->Localization()->Localize(pPlayer->GetLanguage(), pTool));
		return false;
	}

	// check level
	if(JobLevel < m_Level)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Your level low. {} {} Level", pWorkedItem->Info()->GetName(), m_Level);
		return false;
	}

	// check durability
	CPlayerItem* pPlayerItem = pPlayer->GetItem(EquipItem);
	const int Durability = pPlayerItem->GetDurability();
	if(Durability <= 0)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Need repair \"{}\"!", pPlayerItem->Info()->GetName());
		return false;
	}

	// lower the durability
	if(rand() % 10 == 0)
		pPlayerItem->SetDurability(Durability - 1);

	// damage
	m_DamageDealt += 3 + pPlayer->GetAttributeSize(AttributeDmg);
	GS()->CreateSound(m_Pos, 20, CmaskOne(ClientID));

	// information
	GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "{} [{}/{}P] : {} ({}/100%)",
		pWorkedItem->Info()->GetName(), (m_DamageDealt > m_Health ? m_Health : m_DamageDealt), m_Health, pPlayerItem->Info()->GetName(), Durability);

	return m_DamageDealt >= m_Health;
}

void CJobItems::MiningWork(int ClientID, CPlayer* pPlayer, CPlayerItem& pWorkedItem)
{
	if(Interaction("Pickaxe", AttributeIdentifier::Efficiency, pPlayer, &pWorkedItem, EQUIP_PICKAXE, pPlayer->Account()->m_MiningData(JOB_LEVEL, 0).m_Value))
	{
		GS()->Core()->AccountMinerManager()->Work(pPlayer, m_Level);
		pWorkedItem.Add(1+ rand()%2);
		SetSpawn(20);
	}
}

void CJobItems::FarmingWork(int ClientID, CPlayer* pPlayer, CPlayerItem& pWorkedItem)
{
	if(Interaction("Rake", AttributeIdentifier::Extraction, pPlayer, &pWorkedItem, EQUIP_RAKE, pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value))
	{
		GS()->Core()->AccountPlantManager()->Work(pPlayer, m_Level);
		pWorkedItem.Add(1 + rand() % 2);
		SetSpawn(20);		
	}
}

int CJobItems::GetPickupType() const
{
	switch(m_Type)
	{
		default:
		case JOB_ITEM_FARMING: return (int)POWERUP_HEALTH;
		case JOB_ITEM_MINING: return (int)POWERUP_ARMOR;
	}
}

void CJobItems::Reset()
{
	m_SpawnTick = -1;
	m_DamageDealt = 0;
}

void CJobItems::Tick()
{
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
			Reset();
	}
}

void CJobItems::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CJobItems::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient, m_SpawnTick == -1))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = GetPickupType();
}
