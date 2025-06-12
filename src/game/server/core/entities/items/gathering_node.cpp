#include "gathering_node.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/houses/house_manager.h>

CEntityGatheringNode::CEntityGatheringNode(CGameWorld* pGameWorld, GatheringNode* pNode, vec2 Pos, int Type)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_GATHERING_NODE, Pos, PickupPhysSize)
{
	m_Type = Type;
	m_pNode = pNode;
	SpawnPositions();
	CEntityGatheringNode::Reset();

	GameWorld()->InsertEntity(this);
}

void CEntityGatheringNode::SpawnPositions()
{
	constexpr int MAX_PROBE_DISTANCE = 32;
	vec2 closestWallPos = m_Pos;
	float shortestDist = (float)(MAX_PROBE_DISTANCE * MAX_PROBE_DISTANCE) + 1.0f;

	const std::array<vec2, 4> aDir = {
		vec2(0,  1),
		vec2(0, -1),
		vec2(1,  0),
		vec2(-1, 0)
	};

	for(const auto& Dir : aDir)
	{
		vec2 CollisionPos;
		vec2 endPos = m_Pos + Dir * MAX_PROBE_DISTANCE;
		if(GS()->Collision()->IntersectLine(m_Pos, endPos, &CollisionPos, nullptr))
		{
			const float dist = distance(m_Pos, CollisionPos);
			if(dist < shortestDist)
			{
				shortestDist = dist;
				closestWallPos = CollisionPos;
			}
		}
	}

	m_Pos = closestWallPos;
}

void CEntityGatheringNode::SetSpawn(int Sec)
{
	m_SpawnTick = Server()->Tick() + (Server()->TickSpeed() * Sec);
}

bool CEntityGatheringNode::TakeDamage(CPlayer* pPlayer)
{
	if(m_CurrentHealth <= 0 || m_SpawnTick != -1)
		return false;

	// information about empty node
	const auto ClientID = pPlayer->GetCID();
	if(m_pNode->m_vItems.isEmpty())
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, 100, "This node has no plants for harvesting.");
		return false;
	}

	// initialize profession details
	int SoundId = -1;
	auto EquipID = ItemType::Unknown;
	auto AttributeID = AttributeIdentifier::Unknown;
	CProfession* pProfession = nullptr;

	switch(m_Type)
	{
		case GATHERING_NODE_ORE:
			EquipID = ItemType::EquipPickaxe;
			AttributeID = AttributeIdentifier::Efficiency;
			pProfession = pPlayer->Account()->GetProfession(ProfessionIdentifier::Miner);
			SoundId = SOUND_GAME_MINER;
			break;
		case GATHERING_NODE_PLANT:
			EquipID = ItemType::EquipRake;
			AttributeID = AttributeIdentifier::Extraction;
			pProfession = pPlayer->Account()->GetProfession(ProfessionIdentifier::Farmer);
			SoundId = SOUND_GAME_FARMER;
			break;
		default:
			return false;
	}

	if(EquipID == ItemType::Unknown || AttributeID == AttributeIdentifier::Unknown || !pProfession)
		return false;


	// check leveling
	if(pProfession->GetLevel() < m_pNode->Level)
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, 100, "Your level is too low for '{}'. Required level: {}.", m_pNode->Name, m_pNode->Level);
		return false;
	}

	// damage
	int Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeID));
	m_CurrentHealth -= Damage;
	GS()->CreateSound(m_Pos, SoundId);

	// working
	const auto EquippedToolItemIdOpt = pPlayer->GetEquippedSlotItemID(EquipID);
	if(EquippedToolItemIdOpt.has_value())
	{
		auto* pEquippedItem = pPlayer->GetItem(EquippedToolItemIdOpt.value());
		const int Durability = pEquippedItem->GetDurability();
		const bool RequiredRepair = Durability <= 0;

		// decrease durability
		if(Durability > 0 && rand() % 10 == 0)
			pEquippedItem->SetDurability(Durability - 1);

		// send message
		const auto Poffix = RequiredRepair ? "broken" : "";
		GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "{}({} of {} HP) : {}({}/100%){}",
			m_pNode->Name, minimum(m_CurrentHealth, m_pNode->Health), m_pNode->Health, pEquippedItem->Info()->GetName(), Durability, Poffix);
	}
	else
	{
		// send message
		GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "{}({} of {} HP) : Hand(\u221e/\u221e)",
			m_pNode->Name, minimum(m_CurrentHealth, m_pNode->Health), m_pNode->Health);
	}

	Die(pPlayer, pProfession);
	return true;
}

void CEntityGatheringNode::Die(CPlayer* pPlayer, CProfession* pProfession)
{
	if(m_CurrentHealth > 0 || m_pNode->m_vItems.isEmpty())
		return;

	const auto Value = 1 + rand() % 2;
	const auto ItemID = m_pNode->m_vItems.getRandomElement();
	auto* pPlayerItem = pPlayer->GetItem(ItemID);
	pProfession->AddExperience(m_pNode->Level);
	pPlayerItem->Add(Value);
	SetSpawn(20);

	// create design drop pickup
	const auto DesignPos = vec2(m_Pos.x, m_Pos.y - 24.f);
	GS()->EntityManager()->DesignRandomDrop(2 + rand() % 2, 8.0f, DesignPos, Server()->TickSpeed(), GetPickupType(), 0, CmaskOne(pPlayer->GetCID()));
}

int CEntityGatheringNode::GetPickupType() const
{
	switch(m_Type)
	{
		default:
		case GATHERING_NODE_PLANT: return (int)POWERUP_HEALTH;
		case GATHERING_NODE_ORE: return (int)POWERUP_ARMOR;
	}
}

void CEntityGatheringNode::Reset()
{
	m_SpawnTick = -1;
	m_CurrentHealth = m_pNode->Health;
}

void CEntityGatheringNode::Tick()
{
	if(m_SpawnTick > 0 && Server()->Tick() > m_SpawnTick)
	{
		Reset();
	}
}

void CEntityGatheringNode::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient) || (m_pNode && m_pNode->m_vItems.isEmpty()))
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, GetPickupType(), 0);
}