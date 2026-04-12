#include "fishing_rod.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>
#include <game/server/core/components/events/mini_events_manager.h>
#include <generated/server_data.h>

CEntityFishingRod::CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position, vec2 Force, bool AutoMode)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Position, 0, ClientID)
{
	m_EndRodPoint = Position;
	m_Rope.Init(NUM_ROPE_POINTS, Position, Force);
	m_Fishing.m_State = FishingNow::WAITING;
	m_Fishing.m_HookingTime = SERVER_TICK_SPEED * (5 + rand() % 14);
	m_FloatInWater = false;
	m_AutoMode = AutoMode;
	m_LastAutoPullTick = 0;

	AddSnappingGroupIds(ROD, NUM_ROD_POINTS);
	AddSnappingGroupIds(ROPE, NUM_ROPE_POINTS);
	GameWorld()->InsertEntity(this);
}

CEntityFishingRod::~CEntityFishingRod()
{
	if(auto* pChar = GS()->GetPlayerChar(m_ClientID))
		pChar->m_pFishingRod = nullptr;
}

vec2 CEntityFishingRod::CalculateRodPoint(bool FacingRight, size_t Segment) const
{
	static constexpr std::array<std::pair<vec2, vec2>, NUM_ROD_POINTS> s_aRodPositions = { {
		{ vec2(0, 0),		vec2(100, -60) },
		{ vec2(100, -60),	vec2(140, -50) },
		{ vec2(140, -50),	vec2(160, -40) }
	} };

	if(Segment >= s_aRodPositions.size())
		return m_Pos;

	const auto& To = s_aRodPositions[Segment].second;
	return FacingRight ? vec2(m_Pos.x + To.x, m_Pos.y + To.y) : vec2(m_Pos.x - To.x, m_Pos.y + To.y);
}

void CEntityFishingRod::UpdateRodEndPoint(const CCharacter* pChar)
{
	if(!pChar)
		return;

	const bool FacingRight = pChar->m_LatestInput.m_TargetX > 0.f;
	m_EndRodPoint = CalculateRodPoint(FacingRight, NUM_ROD_POINTS - 1);
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
	const auto EquippedFishrodItemIdOpt = pPlayer->GetEquippedSlotItemID(ItemType::EquipFishrod);
	if(!EquippedFishrodItemIdOpt.has_value())
	{
		GS()->Chat(m_ClientID, "To start fishing, equip a fishing rod.");
		MarkForDestroy();
		return;
	}

	const auto* pRodInfo = GS()->GetItemInfo(*EquippedFishrodItemIdOpt);
	const char* pRodName = pRodInfo->GetName();

	// update
	m_Pos = pChar->m_Core.m_Pos;
	UpdateRodEndPoint(pChar);
	m_Rope.UpdatePhysics(GS()->Collision(), 3.0f, 16.f, 64.f);
	m_Rope.m_vPoints[0] = m_EndRodPoint;

	// view cam effect
	const auto& lastPoint = m_Rope.m_vPoints.back();
	pPlayer->LockedView().ViewLock(lastPoint, true);

	// fishing only water
	const auto& TestBox = vec2(lastPoint.x, lastPoint.y + 18.f);
	if((GS()->Collision()->GetCollisionFlagsAt(TestBox) & CCollision::COLFLAG_WATER) == 0)
	{
		// pulling unground water
		if(m_Fishing.m_State == FishingNow::PULLING)
			m_Fishing.m_State = FishingNow::AWAY;
		m_FloatInWater = false;
		return;
	}

	// mark float in water and send sound
	if(!m_FloatInWater)
	{
		GS()->CreateSound(TestBox, SOUND_SFX_WATER);
		m_FloatInWater = true;
	}

	// check node
	const auto switchNumber = GS()->Collision()->GetSwitchTileNumber(TestBox).value_or(-1);
	auto* pNode = GS()->Collision()->GetFishNode(switchNumber);
	if(!pNode)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "There are no fish in this area.");
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
	FishingTick(pPlayer, pFisherman, pNode, pRodName);
}

void CEntityFishingRod::FishingTick(CPlayer* pPlayer, CProfession* pFisherman, GatheringNode* pNode, const char* pRodName)
{
	m_Fishing.m_HookingTime--;

	// left time for fishing
	if(m_Fishing.m_HookingTime <= 0)
	{
		if(m_Fishing.m_State == FishingNow::HOOKING)
		{
			const auto HookingNextTime = pPlayer->GetItem(itFishBait)->IsEquipped() ? 10 : 15;
			m_Fishing.m_State = FishingNow::WAITING;
			m_Fishing.m_HookingTime = SERVER_TICK_SPEED * (3 + HookingNextTime);
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
		{
			const auto lastPoint = m_Rope.m_vPoints.back();
			m_Fishing.m_State = FishingNow::HOOKING;
			m_LastAutoPullTick = Server()->Tick();
			GS()->CreateSound(lastPoint, SOUND_SFX_WATER);
		}

		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "Waiting for fish: '{}' | Rod: {} | Mode: {}",
			pNode->Name, pRodName, m_AutoMode ? "Auto" : "Manual");
		return;
	}

	// hooking state
	if(m_Fishing.m_State == FishingNow::HOOKING)
	{
		// effect hooking
		if(m_Fishing.m_HookingTime % Server()->TickSpeed() == 0)
		{
			const auto lastPoint = m_Rope.m_vPoints.back();
			GS()->CreateDeath(lastPoint, m_ClientID);
			m_Rope.SetForce(vec2(0.f, 5.f));
		}

		// input key fire
		Server()->Input()->BlockInputGroup(m_ClientID, BLOCK_INPUT_FIRE);
		const bool ManualPull = !m_AutoMode && Server()->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE);
		const bool AutoPull = m_AutoMode && (Server()->Tick() - m_LastAutoPullTick) >= (Server()->TickSpeed() - 1);
		if(ManualPull || AutoPull)
		{
			m_Fishing.m_State = FishingNow::PULLING;
			m_Fishing.m_Health = pNode->Health;
			m_Fishing.m_HookingTime = Server()->TickSpeed() * 3;
			m_LastAutoPullTick = Server()->Tick();
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
			const auto lastPoint = m_Rope.m_vPoints.back();
			m_Fishing.m_FromPoint = lastPoint;
			m_Fishing.m_InterpolatedX = lastPoint.x;
		}

		// input key fire
		Server()->Input()->BlockInputGroup(m_ClientID, BLOCK_INPUT_FIRE);
		const bool ManualPull = !m_AutoMode && Server()->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE);
		const bool AutoPull = m_AutoMode && (Server()->Tick() - m_LastAutoPullTick) >= (Server()->TickSpeed() - 1);
		if(ManualPull || AutoPull)
		{
			const auto Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeIdentifier::Patience));
			m_Fishing.m_Health = maximum(m_Fishing.m_Health - Damage, 0);
			m_LastAutoPullTick = Server()->Tick();

			const auto totalDamage = pNode->Health - m_Fishing.m_Health;
			float percentDmg = translate_to_percent(pNode->Health, totalDamage);
			m_Fishing.m_InterpolatedX = (*m_Fishing.m_FromPoint).x + ((m_EndRodPoint.x - (*m_Fishing.m_FromPoint).x) * (percentDmg / 100.f));

			constexpr int IGNORE_POINTS = 3;
			float percentHP = translate_to_percent(pNode->Health, m_Fishing.m_Health);
			float percentPoints = translate_to_percent((size_t)NUM_ROPE_POINTS, m_Rope.m_vPoints.size());
			while(percentPoints > percentHP && m_Rope.m_vPoints.size() > IGNORE_POINTS)
			{
				m_Rope.m_vPoints.erase(m_Rope.m_vPoints.begin() + 1);
				percentPoints = translate_to_percent((size_t)NUM_ROPE_POINTS, m_Rope.m_vPoints.size());
			}
		}

		// smooth moving
		if(!m_Rope.m_vPoints.empty())
		{
			auto& lastPointRef = m_Rope.m_vPoints.back();
			if(lastPointRef.x != m_Fishing.m_InterpolatedX)
				lastPointRef.x += (m_Fishing.m_InterpolatedX - lastPointRef.x) * 0.1f;
		}

		// success
		if(m_Fishing.m_Health <= 0)
		{
			auto& lastPointRef = m_Rope.m_vPoints.back();
			int Value = 1 + rand() % 2;
			const auto ItemID = pNode->m_vItems.getRandomElement();
			auto* pPlayerItem = pPlayer->GetItem(ItemID);
			GS()->Core()->MiniEventsManager()->ApplyBonus(MiniEventType::FishingDrop, &Value);
			pFisherman->AddExperience(pNode->Level * 2);
			pPlayerItem->Add(Value);

			// create design drop pickup
			const auto DesignPos = vec2(m_EndRodPoint.x, m_EndRodPoint.y - 24.f);
			GS()->EntityManager()->DesignRandomDrop(2 + rand() % 2, 8.0f, DesignPos, Server()->TickSpeed(), POWERUP_HEALTH, 0, CmaskOne(pPlayer->GetCID()));
			lastPointRef = m_EndRodPoint;
			m_Fishing.m_State = FishingNow::SUCCESS;
		}

		// information
		auto health = m_Fishing.m_Health;
		auto maxHP = pNode->Health;
		const auto Sec = m_Fishing.m_HookingTime / Server()->TickSpeed();
		const auto ProgressBar = mystd::string::progressBar(Server()->TickSpeed() * 3, m_Fishing.m_HookingTime, 10, "\u25B0", "\u25B1");
		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 50, "'{}' (HP {}/{}) [{} | {}s] | Rod: {}",
			pNode->Name, health, maxHP, ProgressBar, Sec, pRodName);
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
	const auto* pvRodIds = FindSnappingGroupIds(ROD);
	if(!pvRodIds)
		return;

	const auto curTick = Server()->Tick();
	const bool facingRight = (pChar->m_LatestInput.m_TargetX > 0.f);

	// draw fishingrod segments
	const auto numSegments = minimum((*pvRodIds).size(), (size_t)NUM_ROD_POINTS);
	for(size_t i = 0; i < numSegments; ++i)
	{
		const auto From = (i == 0) ? m_Pos : CalculateRodPoint(facingRight, i - 1);
		const auto To = CalculateRodPoint(facingRight, i);
		GS()->SnapLaser(SnappingClient, (*pvRodIds)[i], From, To, curTick - 2, LASERTYPE_SHOTGUN);
	}

	// draw rope
	const auto* pvRopeIds = FindSnappingGroupIds(ROPE);
	if(!pvRopeIds)
		return;

	const size_t ropePointCount = m_Rope.m_vPoints.size();
	if(ropePointCount >= 2)
	{
		for(size_t i = 0; i < ropePointCount - 1; ++i)
		{
			const auto& From = m_Rope.m_vPoints[i];
			const auto& To = m_Rope.m_vPoints[i + 1];
			GS()->SnapLaser(SnappingClient, (*pvRopeIds)[i], From, To, curTick - 6, LASERTYPE_DRAGGER);
		}

		// draw rod float
		GS()->SnapPickup(SnappingClient, GetID(), m_Rope.m_vPoints.back(), POWERUP_HEALTH, 0);
	}
}
