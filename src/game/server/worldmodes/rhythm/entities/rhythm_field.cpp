/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "rhythm_field.h"
#include "rhythm_arrow.h"

#include <generated/protocol.h>
#include <game/server/gamecontext.h>

CRhythmField::CRhythmField(CGameWorld *pGameWorld, vec2 Pos, float Bpm, float HitRadius) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Pos),
	m_Bpm(Bpm),
	m_BeatPeriod(0.0f),
	m_BeatIntervalTicks(0),
	m_SpawnIntervalTicks(0),
	m_NextSpawnTick(0),
	m_FieldScale(1.0f),
	m_SpawnOffset(SRhythmFieldConfig::s_SpawnOffset),
	m_ArrowTravelDistance(SRhythmFieldConfig::s_FieldHeight),
	m_AutoSpawn(true),
	m_HitZonePos(Pos),
	m_HitZoneRadius(HitRadius)
{
	m_HitLineLaserId = -1;
	EnsureSnapIds();
	UpdateBeatTiming();
	m_NextSpawnTick = Server()->Tick() + m_SpawnIntervalTicks;

	GameWorld()->InsertEntity(this);
}

CRhythmField::~CRhythmField()
{
	for(CRhythmArrow *pArrow : m_vArrows)
	{
		if(pArrow)
			pArrow->DetachField();
	}
	m_vArrows.clear();

	if(m_HitLineLaserId >= 0)
		Server()->SnapFreeID(m_HitLineLaserId);
}

void CRhythmField::Reset()
{
	for(CRhythmArrow *pArrow : m_vArrows)
	{
		if(pArrow)
		{
			pArrow->DetachField();
			pArrow->Reset();
		}
	}
	m_vArrows.clear();
	MarkForDestroy();
}

void CRhythmField::Tick()
{
	if(!m_AutoSpawn)
		return;

	if(Server()->Tick() < m_NextSpawnTick)
		return;

	while(Server()->Tick() >= m_NextSpawnTick)
	{
		SpawnArrow();
		m_NextSpawnTick += m_SpawnIntervalTicks;
	}
}

void CRhythmField::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	const float HalfWidth = SRhythmFieldConfig::s_LaneWidth * 1.5f;
	const vec2 HitFrom(m_HitZonePos.x - HalfWidth, m_HitZonePos.y);
	const vec2 HitTo(m_HitZonePos.x + HalfWidth, m_HitZonePos.y);
	GS()->SnapLaser(SnappingClient, m_HitLineLaserId, HitTo, HitFrom, Server()->Tick(), LASERTYPE_DOOR);
}

void CRhythmField::SetBpm(float Bpm)
{
	m_Bpm = Bpm;
	UpdateBeatTiming();
}

void CRhythmField::SetAutoSpawn(bool Auto)
{
	m_AutoSpawn = Auto;
}

void CRhythmField::SetHitZone(vec2 Pos)
{
	m_Pos = Pos;
	m_HitZonePos = Pos;
}

void CRhythmField::EnsureSnapIds()
{
	if(m_HitLineLaserId < 0)
		m_HitLineLaserId = Server()->SnapNewID();
}

void CRhythmField::RegisterArrow(CRhythmArrow *pArrow)
{
	if(!pArrow)
		return;
	m_vArrows.push_back(pArrow);
}

void CRhythmField::UnregisterArrow(CRhythmArrow *pArrow)
{
	if(!pArrow)
		return;
	auto Iter = std::find(m_vArrows.begin(), m_vArrows.end(), pArrow);
	if(Iter != m_vArrows.end())
		m_vArrows.erase(Iter);
}

void CRhythmField::UpdateBeatTiming()
{
	if(m_Bpm <= 0.0f)
		m_Bpm = 120.0f;

	m_FieldScale = 1.0f;
	m_ArrowTravelDistance = SRhythmFieldConfig::s_FieldHeight;
	m_SpawnOffset = SRhythmFieldConfig::s_SpawnOffset;

	m_BeatPeriod = 60.0f / m_Bpm;
	m_BeatIntervalTicks = std::max(1, (int)std::round(m_BeatPeriod * Server()->TickSpeed()));
	m_SpawnIntervalTicks = std::max(1, (int)std::round((60.0f / m_Bpm) * Server()->TickSpeed()));
}

void CRhythmField::SpawnLaneArrow(int LaneIndex, int HitTick, int HoldDurationTicks)
{
	const float HalfWidth = SRhythmFieldConfig::s_LaneWidth * 1.5f;
	const float X = m_HitZonePos.x - HalfWidth + SRhythmFieldConfig::s_LaneWidth * (LaneIndex + 0.5f);
	const vec2 Origin(X, m_HitZonePos.y - m_ArrowTravelDistance - m_SpawnOffset);
	const vec2 Direction(0.0f, 1.0f);

	SpawnArrow(Origin, Direction, HitTick, LaneIndex, HoldDurationTicks);
}

void CRhythmField::SpawnArrow(vec2 Origin, vec2 Direction, int HitTick, int LaneIndex, int HoldDurationTicks)
{
	const int TravelTicks = std::max(1, HitTick - Server()->Tick());
	const float Distance = std::max(1.0f, dot(m_HitZonePos - Origin, Direction));
	const float SpeedPerTick = Distance / (float)TravelTicks;
	const float WeaponSpeed = GS()->Tuning()->m_GunSpeed;
	const float VelScale = Distance * Server()->TickSpeed() / (WeaponSpeed * TravelTicks);
	const float TailLength = HoldDurationTicks > 0 ? SpeedPerTick * HoldDurationTicks : 0.0f;
	const float MissY = m_HitZonePos.y + SRhythmFieldConfig::s_MissOffset + TailLength;
	new CRhythmArrow(&GS()->m_World, this, Origin, Direction, SpeedPerTick, HitTick, LaneIndex, MissY, VelScale, TailLength);
}

void CRhythmField::SpawnArrow()
{
	const int SpawnTick = Server()->Tick();
	const int HitTick = SpawnTick + m_BeatIntervalTicks;

	SpawnLaneArrow(SRhythmFieldConfig::s_AutoSpawnLaneIndex, HitTick);
}

void CRhythmField::HideArrowForClient(int LaneIndex, int HitTick, int ClientId)
{
	for(CRhythmArrow *pArrow : m_vArrows)
	{
		if(!pArrow)
			continue;
		if(pArrow->HitTick() != HitTick || pArrow->LaneIndex() != LaneIndex)
			continue;
		pArrow->HideForClient(ClientId);
	}
}

bool CRhythmField::IsHiddenArrowForClient(int LaneIndex, int HitTick, int ClientId) const
{
	for(const CRhythmArrow *pArrow : m_vArrows)
	{
		if(!pArrow)
			continue;
		if(pArrow->HitTick() != HitTick || pArrow->LaneIndex() != LaneIndex)
			continue;
		return pArrow->IsHiddenForClient(ClientId);
	}
	return false;
}
