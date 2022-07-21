/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "jobitems.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Accounts/AccountMinerCore.h>
#include <game/server/mmocore/Components/Accounts/AccountPlantCore.h>
#include <game/server/mmocore/Components/Houses/HouseCore.h>

// 1 - miner / 2 - plant
CJobItems::CJobItems(CGameWorld *pGameWorld, int ItemID, int Level, vec2 Pos, int Type, int Health, int HouseID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_JOBITEMS, Pos, PickupPhysSize)
{
	m_ItemID = ItemID;
	m_Level = Level;
	m_Type = Type;
	m_Health = Health;
	m_TotalDamage = 0;
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
	m_TotalDamage = m_Health;
}

void CJobItems::Work(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true, true);
	if(!pPlayer || m_TotalDamage >= m_Health)
		return;

	// not allowed un owner house job
	if(m_HouseID > 0 && !GS()->Mmo()->House()->IsHouseHasOwner(m_HouseID))
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "It is forbidden to pick plants without the owner!");
		return;
	}

	CItemData& pWorkedItem = pPlayer->GetItem(m_ItemID);
	if(m_Type == JOB_ITEM_MINING)
		MiningWork(ClientID, pPlayer, pWorkedItem);
	else if(m_Type == JOB_ITEM_FARMING)
		FarmingWork(ClientID, pPlayer, pWorkedItem);
}

void CJobItems::MiningWork(int ClientID, CPlayer* pPlayer, CItemData& pWorkedItem)
{
	const int EquipItem = pPlayer->GetEquippedItemID(EQUIP_MINER);
	if(EquipItem <= 0)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Need equip Pickaxe!");
		return;
	}

	if(pPlayer->Acc().m_aMining[JOB_LEVEL].m_Value < m_Level)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Your level low. {STR} {INT} Level", pWorkedItem.Info().GetName(), m_Level);
		return;
	}

	CItemData& pEquippedPickaxe = pPlayer->GetItem(EquipItem);
	const int Durability = pEquippedPickaxe.m_Durability;
	if(Durability <= 0)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Need repair pickaxe!");
		return;
	}

	if(rand() % 10 == 0)
		pEquippedPickaxe.SetDurability(Durability - 1);

	m_TotalDamage += 3 + pPlayer->GetItemsAttributeCount(StEfficiency);
	GS()->CreateSound(m_Pos, 20, CmaskOne(ClientID));

	GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "{STR} [{INT}/{INT}P] : {STR} ({INT}/100%)",
		pWorkedItem.Info().GetName(), (m_TotalDamage > m_Health ? m_Health : m_TotalDamage), m_Health,
		pEquippedPickaxe.Info().GetName(), Durability);

	if(m_TotalDamage >= m_Health)
	{
		GS()->Mmo()->MinerAcc()->Work(pPlayer, m_Level);
		pWorkedItem.Add(pPlayer->Acc().m_aMining[JOB_UPGR_QUANTITY].m_Value);
		SetSpawn(20);
	}
}

void CJobItems::FarmingWork(int ClientID, CPlayer* pPlayer, CItemData& pWorkedItem)
{
	if(pPlayer->Acc().m_aFarming[JOB_LEVEL].m_Value < m_Level)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Your level low. {STR} {INT} Level", pWorkedItem.Info().GetName(), m_Level);
		return;
	}

	m_TotalDamage += 10;
	GS()->CreateSound(m_Pos, 20, CmaskOne(ClientID));

	GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "{STR} [{INT}/{INT}P]",
		pWorkedItem.Info().GetName(), (m_TotalDamage > m_Health ? m_Health : m_TotalDamage), m_Health);

	if(m_TotalDamage >= m_Health)
	{
		GS()->Mmo()->PlantsAcc()->Work(pPlayer, m_Level);
		pWorkedItem.Add(pPlayer->Acc().m_aFarming[JOB_UPGR_QUANTITY].m_Value);
		SetSpawn(20);
	}
}

int CJobItems::SwitchToObject(bool MmoItem) const
{
	switch(m_Type)
	{
		default:
		case JOB_ITEM_FARMING: return (MmoItem ? (int)MMO_PICKUP_PLANT : (int)PICKUP_HEALTH);
		case JOB_ITEM_MINING: return (MmoItem ? (int)MMO_PICKUP_ORE : (int)PICKUP_ARMOR);
	}
}

void CJobItems::Reset()
{
	m_SpawnTick = -1;
	m_TotalDamage = 0;
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
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	if(SwitchToObject(true) > -1 && GS()->IsMmoClient(SnappingClient))
	{
		CNetObj_MmoPickup *pObj = static_cast<CNetObj_MmoPickup*>(Server()->SnapNewItem(NETOBJTYPE_MMOPICKUP, GetID(), sizeof(CNetObj_MmoPickup)));
		if(!pObj)
			return;

		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_Type = SwitchToObject(true);
		return;
	}

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = SwitchToObject(false);
}
