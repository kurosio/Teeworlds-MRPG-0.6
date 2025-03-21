#include "fishing_rod.h"

#include <game/server/gamecontext.h>

CEntityFishingRod::CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position, vec2 Force)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, Position, 0, ClientID)
{
	m_LastPoint = Position;
	m_Rope.Init(NUM_ROPE_POINTS, Position, Force);

	AddGroupIds(ROD, NUM_ROD_POINTS);
	AddGroupIds(ROPE, NUM_ROPE_POINTS);
	GameWorld()->InsertEntity(this);
}

void CEntityFishingRod::Tick()
{
	// check valid
	auto* pChar = GS()->GetPlayerChar(m_ClientID);
	if(!pChar || m_Rope.m_vPoints.size() < 2)
	{
		MarkForDestroy();
		return;
	}

	// check equip fishing rod
	auto* pPlayer = pChar->GetPlayer();
	const auto EquippedItemID = pPlayer->GetEquippedItemID(ItemType::EquipFishrod);
	if(!EquippedItemID.has_value())
	{
		GS()->Chat(m_ClientID, "To start fishing, equip your fishing rod!");
		MarkForDestroy();
		return;
	}

	// update
	m_Pos = pChar->m_Core.m_Pos;
	m_Rope.UpdatePhysics(GS()->Collision(), 3.0f, 16.f, 64.f);
	m_Rope.m_vPoints[0] = m_LastPoint;

	// fishing
	const auto& TestBox = vec2(m_Rope.m_vPoints.back().x, m_Rope.m_vPoints.back().y + 16.f);
	if(GS()->Collision()->GetCollisionFlagsAt(TestBox) & CCollision::COLFLAG_WATER)
	{
		// check node
		auto switchNumber = GS()->Collision()->GetSwitchTileNumber(TestBox).value_or(-1);
		auto* pNode = GS()->Collision()->GetFishNode(switchNumber);
		if(!pNode)
		{
			GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "There are no fish in the area!");
			return;
		}

		// check leveling
		CProfession* pFisherman = pPlayer->Account()->GetProfession(ProfessionIdentifier::Fisherman);
		if(pFisherman->GetLevel() < pNode->Level)
		{
			GS()->Broadcast(m_ClientID, BroadcastPriority::GameWarning, 100, "Your level is too low for '{}'. Required level: {}.", pNode->Name, pNode->Level);
			return;
		}


		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "Started fishing: '{}' / {}",
			pNode->Name, GS()->GetItemInfo(*EquippedItemID)->GetName());
	}
}

void CEntityFishingRod::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	auto* pChar = GS()->GetPlayerChar(m_ClientID);
	if(!pChar)
		return;

	// initialize variables
	auto& rodIds = GetGroupIds(ROD);
	const auto curTick = Server()->Tick();
	const bool facingRight = (pChar->m_LatestInput.m_TargetX > 0.f);
	const std::array<std::pair<vec2, vec2>, 3> positions = {
	{
		{ vec2(0, 0),    vec2(100, -60) },
		{ vec2(100, -60),vec2(140, -50) },
		{ vec2(140, -50),vec2(160, -40) }
	} };

	// draw fishingrod segments
	const auto numSegments = std::min(rodIds.size(), positions.size());
	for(size_t i = 0; i < numSegments; ++i)
	{
		const auto& [first, second] = positions[i];
		const auto From = facingRight ? vec2(m_Pos.x + first.x, m_Pos.y + first.y) : vec2(m_Pos.x - first.x, m_Pos.y + first.y);
		const auto To = facingRight ? vec2(m_Pos.x + second.x, m_Pos.y + second.y) : vec2(m_Pos.x - second.x, m_Pos.y + second.y);
		GS()->SnapLaser(SnappingClient, rodIds[i], From, To, curTick, LASERTYPE_SHOTGUN);
		m_LastPoint = To;
	}

	// draw rope
	auto& ropeIds = GetGroupIds(ROPE);
	const size_t ropePointCount = m_Rope.m_vPoints.size();
	if(ropePointCount >= 2)
	{
		for(size_t i = 0; i < ropePointCount - 1; ++i)
		{
			const auto& From = m_Rope.m_vPoints[i];
			const auto& To = m_Rope.m_vPoints[i + 1];
			GS()->SnapLaser(SnappingClient, ropeIds[i], From, To, curTick - 5, LASERTYPE_DRAGGER);
		}

		// draw rod float
		GS()->SnapPickup(SnappingClient, GetID(), m_Rope.m_vPoints.back(), POWERUP_HEALTH, 0);
	}
}