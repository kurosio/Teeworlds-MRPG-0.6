#include "fishing_rod.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>

CEntityFishingRod::CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position, vec2 Force)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_FISHING_ROD, Position, 0, ClientID)
{
	m_EndRodPoint = Position;
	m_Rope.Init(NUM_ROPE_POINTS, Position, Force);
	m_Fishing.m_State = FishingNow::WAITING;
	m_Fishing.m_HookingTime = SERVER_TICK_SPEED * (5 + rand() % 14);

	AddSnappingGroupIds(ROD, NUM_ROD_POINTS);
	AddSnappingGroupIds(ROPE, NUM_ROPE_POINTS);
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

	// is success or big distance
	const auto distanceBetween = distance(m_Rope.m_vPoints.front(), m_Rope.m_vPoints.back());
	if(distanceBetween > 800.f || m_Fishing.m_State == FishingNow::SUCCESS)
	{
		MarkForDestroy();
		return;
	}

	// away fish
	if(m_Fishing.m_State == FishingNow::AWAY)
	{
		GS()->Chat(m_ClientID, "The fish got away!");
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
	m_Rope.m_vPoints[0] = m_EndRodPoint;

	// fishing only water
	const auto& TestBox = vec2(m_Rope.m_vPoints.back().x, m_Rope.m_vPoints.back().y + 18.f);
	if((GS()->Collision()->GetCollisionFlagsAt(TestBox) & CCollision::COLFLAG_WATER) == 0)
	{
		// pulling unground water
		if(m_Fishing.m_State == FishingNow::PULLING)
			m_Fishing.m_State = FishingNow::AWAY;
		return;
	}

	// check node
	const auto switchNumber = GS()->Collision()->GetSwitchTileNumber(TestBox).value_or(-1);
	auto* pNode = GS()->Collision()->GetFishNode(switchNumber);
	if(!pNode)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "There are no fish in the area!");
		return;
	}

	// check leveling
	auto* pFisherman = pPlayer->Account()->GetProfession(ProfessionIdentifier::Fisherman);
	if(pFisherman->GetLevel() < pNode->Level)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameWarning, 100, "Your level is too low for '{}'. Required level: {}.", pNode->Name, pNode->Level);
		return;
	}

	// fishing logic
	FishingTick(pPlayer, pFisherman, pNode, EquippedItemID);
}

void CEntityFishingRod::FishingTick(CPlayer* pPlayer, CProfession* pFisherman, GatheringNode* pNode, std::optional<int> EquippedItemID)
{
	m_Fishing.m_HookingTime--;

	// left time for fishing
	if(m_Fishing.m_HookingTime <= 0)
	{
		if(m_Fishing.m_State == FishingNow::HOOKING)
		{
			m_Fishing.m_State = FishingNow::WAITING;
			m_Fishing.m_HookingTime = SERVER_TICK_SPEED * (3 + rand() % 15);
		}
		else
		{
			m_Fishing.m_State = FishingNow::AWAY;
		}

		return;
	}

	// waiting state
	if(m_Fishing.m_State == FishingNow::WAITING)
	{
		if(m_Fishing.m_HookingTime <= (Server()->TickSpeed() * 3) + 1)
			m_Fishing.m_State = FishingNow::HOOKING;

		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "Waiting for the fish: '{}' : {}",
			pNode->Name, GS()->GetItemInfo(*EquippedItemID)->GetName());
		return;
	}

	// hooking state
	vec2& lastPoint = m_Rope.m_vPoints.back();
	if(m_Fishing.m_State == FishingNow::HOOKING)
	{
		// effect hooking
		if(m_Fishing.m_HookingTime % Server()->TickSpeed() == 0)
		{
			GS()->CreateDeath(lastPoint, m_ClientID);
			m_Rope.SetForce(vec2(0.f, 5.f));
		}

		// input key fire
		Server()->Input()->BlockInputGroup(m_ClientID, BLOCK_INPUT_FIRE);
		if(Server()->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE))
		{
			m_Fishing.m_State = FishingNow::PULLING;
			m_Fishing.m_Health = pNode->Health;
			m_Fishing.m_HookingTime = Server()->TickSpeed() * 3;
		}

		const auto Sec = m_Fishing.m_HookingTime / Server()->TickSpeed();
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "Something's biting. Hook the fish! [{}s]", Sec);
		return;
	}

	// pulling
	if(m_Fishing.m_State == FishingNow::PULLING)
	{
		// set end point
		if(!m_Fishing.m_FromPoint)
		{
			m_Fishing.m_FromPoint = lastPoint;
			m_Fishing.m_InterpolatedX = lastPoint.x;
		}

		// input key fire
		Server()->Input()->BlockInputGroup(m_ClientID, BLOCK_INPUT_FIRE);
		if(Server()->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE))
		{
			const auto Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeIdentifier::Patience));
			m_Fishing.m_Health = maximum(m_Fishing.m_Health - Damage, 0);

			const auto totalDamage = pNode->Health - m_Fishing.m_Health;
			float percentDmg = translate_to_percent(pNode->Health, totalDamage);
			m_Fishing.m_InterpolatedX = (*m_Fishing.m_FromPoint).x + ((m_EndRodPoint.x - (*m_Fishing.m_FromPoint).x) * (percentDmg / 100.f));

			constexpr int IGNORE_POINTS = 3;
			float percentHP = translate_to_percent(pNode->Health, m_Fishing.m_Health);
			float percentPoints = translate_to_percent((size_t)NUM_ROPE_POINTS, m_Rope.m_vPoints.size());
			while(percentPoints > percentHP)
			{
				if(m_Rope.m_vPoints.size() <= IGNORE_POINTS)
					break;
				m_Rope.m_vPoints.erase(m_Rope.m_vPoints.begin() + 1);
				m_Rope.m_vPoints.shrink_to_fit();
				percentPoints = translate_to_percent((size_t)NUM_ROPE_POINTS, m_Rope.m_vPoints.size());
			}
		}

		// smooth moving
		if(lastPoint.x != m_Fishing.m_InterpolatedX)
			lastPoint.x += (m_Fishing.m_InterpolatedX - lastPoint.x) * 0.1f;

		// success
		if(m_Fishing.m_Health <= 0)
		{
			const auto Value = 1 + rand() % 2;
			const auto ItemID = pNode->m_vItems.getRandomElement();
			auto* pPlayerItem = pPlayer->GetItem(ItemID);
			pFisherman->AddExperience(pNode->Level);
			pPlayerItem->Add(Value);

			// create design drop pickup
			const auto DesignPos = vec2(m_EndRodPoint.x, m_EndRodPoint.y - 24.f);
			GS()->EntityManager()->DesignRandomDrop(2 + rand() % 2, 8.0f, DesignPos, Server()->TickSpeed(), POWERUP_HEALTH, 0, CmaskOne(pPlayer->GetCID()));
			lastPoint = m_EndRodPoint;
			m_Fishing.m_State = FishingNow::SUCCESS;
		}

		// information
		auto health = m_Fishing.m_Health;
		auto maxHP = pNode->Health;
		const auto Sec = m_Fishing.m_HookingTime / Server()->TickSpeed();
		const auto ProgressBar = mystd::string::progressBar(Server()->TickSpeed() * 3, m_Fishing.m_HookingTime, 20, "\u25B0", "\u25B1");
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "'{}'(HP {}/{}) [{}|{}s] : {}",
			pNode->Name, health, maxHP, ProgressBar, Sec, GS()->GetItemInfo(*EquippedItemID)->GetName());
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
	auto& rodIds = GetSnappingGroupIds(ROD);
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
		GS()->SnapLaser(SnappingClient, rodIds[i], From, To, curTick - 2, LASERTYPE_SHOTGUN);
		m_EndRodPoint = To;
	}

	// draw rope
	auto& ropeIds = GetSnappingGroupIds(ROPE);
	const size_t ropePointCount = m_Rope.m_vPoints.size();
	if(ropePointCount >= 2)
	{
		for(size_t i = 0; i < ropePointCount - 1; ++i)
		{
			const auto& From = m_Rope.m_vPoints[i];
			const auto& To = m_Rope.m_vPoints[i + 1];
			GS()->SnapLaser(SnappingClient, ropeIds[i], From, To, curTick - 6, LASERTYPE_DRAGGER);
		}

		// draw rod float
		GS()->SnapPickup(SnappingClient, GetID(), m_Rope.m_vPoints.back(), POWERUP_HEALTH, 0);
	}
}