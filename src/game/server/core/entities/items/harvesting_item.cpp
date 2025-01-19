/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "harvesting_item.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Accounts/AccountMiningManager.h>
#include <game/server/core/components/Accounts/AccountFarmingManager.h>
#include <game/server/core/components/houses/house_manager.h>

CEntityHarvestingItem::CEntityHarvestingItem(CGameWorld *pGameWorld, int ItemID, vec2 Pos, int Type, int HouseID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_HERVESTING_ITEM, Pos, PickupPhysSize), m_ItemID(ItemID)
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
	if(const auto optHarvestingContext = GetItemInfo()->GetHarvestingContext())
	{
		m_SpawnTick = Server()->Tick() + (Server()->TickSpeed() * Sec);
		m_Damage = optHarvestingContext->Health;
	}
}

void CEntityHarvestingItem::Process(int ClientID)
{
	// check valid player
	auto* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// check valid harvesting data
	const auto optHarvestingContext = GetItemInfo()->GetHarvestingContext();
	dbg_assert(optHarvestingContext.has_value(), "harvesting context is not valid");

	// check count damage
	if(m_Damage >= optHarvestingContext->Health)
		return;

	// not allowed un owner house job
	const auto* pHouse = GS()->Core()->HouseManager()->GetHouse(m_HouseID);
	if(pHouse && !pHouse->HasOwner())
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, 100, "It is forbidden to collect farming.");
		return;
	}

	// implement
	auto* pPlayerItem = pPlayer->GetItem(GetItemInfo()->GetID());
	if(m_Type == HARVESTINGITEM_TYPE_MINING)
	{
		auto* pMinerProfession = pPlayer->Account()->GetProfession(ProfessionIdentifier::Miner);
		if(!pMinerProfession)
			return;

		if(TakeDamage(AttributeIdentifier::Efficiency, pPlayer, pPlayerItem, ItemType::EquipPickaxe, pMinerProfession->GetLevel()))
		{
			const auto Value = 1 + rand() % 2;
			pMinerProfession->AddExperience(Value);
			pPlayerItem->Add(1 + rand() % 2);
			SetSpawn(20);
		}
	}
	else if(m_Type == HARVESTINGITEM_TYPE_FARMING)
	{
		auto* pFarmerProfession = pPlayer->Account()->GetProfession(ProfessionIdentifier::Farmer);
		if(!pFarmerProfession)
			return;

		if(TakeDamage(AttributeIdentifier::Extraction, pPlayer, pPlayerItem, ItemType::EquipRake, pFarmerProfession->GetLevel()))
		{
			const auto Value = 1 + rand() % 2;
			pFarmerProfession->AddExperience(Value);
			pPlayerItem->Add(1 + rand() % 2);
			SetSpawn(20);
		}
	}
}

bool CEntityHarvestingItem::TakeDamage(AttributeIdentifier Attribute, CPlayer* pPlayer, const CPlayerItem* pWorkedItem, ItemType EquipID, int SelfLevel)
{
	// check valid harvesting data
	const auto optHarvestingContext = GetItemInfo()->GetHarvestingContext();
	if(!optHarvestingContext.has_value())
		return false;

	// initialize variables
	const auto optEquipItemID = pPlayer->GetEquippedItemID(EquipID);
	const int ClientID = pPlayer->GetCID();
	const int& Level = optHarvestingContext->Level;
	const int& Health = optHarvestingContext->Health;

	// check level
	if(SelfLevel < Level)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, 100, "Your level low. {} {} Level", pWorkedItem->Info()->GetName(), Level);
		return false;
	}

	// with equipped item
	if(optEquipItemID.has_value())
	{
		// initializae variables
		auto* pEquippedItem = pPlayer->GetItem(optEquipItemID.value());
		const int Durability = pEquippedItem->GetDurability();
		const bool RequiredRepair = Durability <= 0;

		// check required repair
		if(!RequiredRepair && rand() % 10 == 0)
		{
			pEquippedItem->SetDurability(Durability - 1);
		}

		// damage
		m_Damage += 3 + pPlayer->GetTotalAttributeValue(Attribute);
		GS()->CreateSound(m_Pos, 20, CmaskOne(ClientID));

		// send message
		const auto Poffix = RequiredRepair ? "broken" : "";
		GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "{} [{}/{}P] : {} ({}/100%){}",
			pWorkedItem->Info()->GetName(), minimum(m_Damage, Health), Health, pEquippedItem->Info()->GetName(), Durability, Poffix);
	}
	else
	{
		// damage
		m_Damage += 1;
		GS()->CreateSound(m_Pos, 20, CmaskOne(ClientID));

		// send message
		GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "{} [{}/{}P] : Hand (\u221e/\u221e)",
			pWorkedItem->Info()->GetName(), minimum(m_Damage, Health), Health);
	}

	// check health
	return m_Damage >= Health;
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
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
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

