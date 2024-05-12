/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "harvesting_item.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Accounts/AccountMiningManager.h>
#include <game/server/core/components/Accounts/AccountFarmingManager.h>
#include <game/server/core/components/houses/house_manager.h>

CEntityHarvestingItem::CEntityHarvestingItem(CGameWorld *pGameWorld, int ItemID, vec2 Pos, int Type, int HouseID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_JOBITEMS, Pos, PickupPhysSize), m_ItemID(ItemID)
{
	// initialize variables
	m_Type = Type;
	m_Damage = 0;
	m_HouseID = HouseID;
	SpawnPositions();
	CEntityHarvestingItem::Reset();

	// insert entity
	GameWorld()->InsertEntity(this);
}

void CEntityHarvestingItem::SpawnPositions()
{
	// initialize variables
	vec2 newPos {};
	int minimalIterate = std::numeric_limits<int>::max();
	vec2 Dir[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

	// find spawn position
	for (const auto& vdir : Dir)
	{
		vec2 checkPos = m_Pos;
		for(int s = 0; s < 32; s++)
		{
			if(s > minimalIterate)
				break;

			checkPos += vdir;
			if(GS()->Collision()->CheckPoint(checkPos))
			{
				newPos = checkPos;
				minimalIterate = s;
				break;
			}
		}
	}

	// update position
	m_Pos = newPos;
}

void CEntityHarvestingItem::SetSpawn(int Sec)
{
	m_SpawnTick = Server()->Tick() + (Server()->TickSpeed()*Sec);
	m_Damage = GetItemInfo()->GetHarvestingData().m_Health;
}

void CEntityHarvestingItem::Process(int ClientID)
{
	// check valid player
	auto* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// check count damage
	if(m_Damage >= GetItemInfo()->GetHarvestingData().m_Health)
		return;

	// not allowed un owner house job
	auto* pHouse = GS()->Core()->HouseManager()->GetHouse(m_HouseID);
	if(pHouse && !pHouse->HasOwner())
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "It is forbidden to collect farming.");
		return;
	}

	// implement
	auto* pPlayerItem = pPlayer->GetItem(GetItemInfo()->GetID());
	if(m_Type == HARVESTINGITEM_TYPE_MINING)
	{
		Mining(pPlayer, *pPlayerItem);
	}
	else if(m_Type == HARVESTINGITEM_TYPE_FARMING)
	{
		Farming(pPlayer, *pPlayerItem);
	}
}

bool CEntityHarvestingItem::Interaction(const char* pToolname, AttributeIdentifier Attribute, CPlayer* pPlayer, const CPlayerItem* pWorkedItem, ItemFunctional EquipID, int SelfLevel)
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int EquipItem = pPlayer->GetEquippedItemID(EquipID);
	const int& Level = GetItemInfo()->GetHarvestingData().m_Level;
	const int& Health = GetItemInfo()->GetHarvestingData().m_Health;

	// check equipped
	if(EquipItem <= 0)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Need equip {}!", 
			Server()->Localization()->Localize(pPlayer->GetLanguage(), pToolname));
		return false;
	}

	// check level
	if(SelfLevel < Level)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Your level low. {} {} Level", pWorkedItem->Info()->GetName(), Level);
		return false;
	}

	// check durability
	auto* pEquippedItem = pPlayer->GetItem(EquipItem);
	const int Durability = pEquippedItem->GetDurability();
	if(Durability <= 0)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_WARNING, 100, "Need repair \"{}\"!", pEquippedItem->Info()->GetName());
		return false;
	}

	// lower the durability
	if(rand() % 10 == 0)
		pEquippedItem->SetDurability(Durability - 1);

	// damage
	m_Damage += 3 + pPlayer->GetAttributeSize(Attribute);
	GS()->CreateSound(m_Pos, 20, CmaskOne(ClientID));

	// send message
	GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "{} [{}/{}P] : {} ({}/100%)",
		pWorkedItem->Info()->GetName(), minimum(m_Damage, Health), Health, pEquippedItem->Info()->GetName(), Durability);

	// check health
	return m_Damage >= Health;
}

void CEntityHarvestingItem::Mining(CPlayer* pPlayer, CPlayerItem& pWorkedItem)
{
	if(Interaction("Pickaxe", AttributeIdentifier::Efficiency, pPlayer, &pWorkedItem, EQUIP_PICKAXE, pPlayer->Account()->m_MiningData(JOB_LEVEL, 0).m_Value))
	{
		GS()->Core()->AccountMiningManager()->Process(pPlayer, GetItemInfo()->GetHarvestingData().m_Level);
		pWorkedItem.Add(1 + rand()%2);
		SetSpawn(20);
	}
}

void CEntityHarvestingItem::Farming(CPlayer* pPlayer, CPlayerItem& pWorkedItem)
{
	if(Interaction("Rake", AttributeIdentifier::Extraction, pPlayer, &pWorkedItem, EQUIP_RAKE, pPlayer->Account()->m_FarmingData(JOB_LEVEL, 0).m_Value))
	{
		GS()->Core()->AccountFarmingManager()->Procces(pPlayer, GetItemInfo()->GetHarvestingData().m_Level);
		pWorkedItem.Add(1 + rand() % 2);
		SetSpawn(20);		
	}
}

int CEntityHarvestingItem::GetPickupType() const
{
	switch(m_Type)
	{
		default:
		case HARVESTINGITEM_TYPE_FARMING: return (int)POWERUP_HEALTH;
		case HARVESTINGITEM_TYPE_MINING: return (int)POWERUP_ARMOR;
	}
}

void CEntityHarvestingItem::Reset()
{
	m_SpawnTick = -1;
	m_Damage = 0;
}

void CEntityHarvestingItem::Tick()
{
	// respawn
	if(m_SpawnTick > 0 && Server()->Tick() > m_SpawnTick)
		Reset();
}

void CEntityHarvestingItem::Snap(int SnappingClient)
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

CItemDescription* CEntityHarvestingItem::GetItemInfo() const
{
	return GS()->GetItemInfo(m_ItemID);
}

