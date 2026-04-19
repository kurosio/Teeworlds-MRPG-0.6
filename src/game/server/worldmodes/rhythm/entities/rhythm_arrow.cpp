/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "rhythm_arrow.h"
#include "rhythm_field.h"

#include <base/math.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>

CRhythmArrow::CRhythmArrow(CGameWorld *pGameWorld, CRhythmField *pField, vec2 Origin, vec2 Direction, float SpeedPerTick, int HitTick, int LaneIndex, float MissY, float VelScale, float TailLength) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Origin),
	m_pField(pField),
	m_Origin(Origin),
	m_Direction(Direction),
	m_Phase(0.0f),
	m_Speed(SpeedPerTick),
	m_SpawnTick(Server()->Tick()),
	m_HitTick(HitTick),
	m_LaneIndex(LaneIndex),
	m_MissY(MissY),
	m_VelScale(VelScale),
	m_TailLength(TailLength),
	m_TailLaserId(-1)
{
	if(m_TailLength > 0.0f)
		m_TailLaserId = Server()->SnapNewID();
	if(m_pField)
		m_pField->RegisterArrow(this);

	GameWorld()->InsertEntity(this);
}

void CRhythmArrow::Reset()
{
	MarkForDestroy();
}

void CRhythmArrow::Tick()
{
	const int Tick = Server()->Tick();
	m_Phase = (Tick - m_SpawnTick) * m_Speed;
	m_Pos = m_Origin + m_Direction * m_Phase;

	if(m_Pos.y >= m_MissY)
		MarkForDestroy();
}

void CRhythmArrow::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	if(SnappingClient >= 0 && m_HiddenMask.test(SnappingClient))
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, POWERUP_ARMOR);
	if(m_TailLaserId >= 0 && m_TailLength > 0.0f)
	{
		const vec2 TailPos = m_Pos - m_Direction * m_TailLength;
		GS()->SnapLaser(SnappingClient, m_TailLaserId, m_Pos, TailPos, Server()->Tick(), LASERTYPE_SHOTGUN);
	}
}

void CRhythmArrow::DetachField()
{
	m_pField = nullptr;
}

void CRhythmArrow::HideForClient(int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	m_HiddenMask.set(ClientId);
}

bool CRhythmArrow::IsHiddenForClient(int ClientId) const
{
	return m_HiddenMask.test(ClientId);
}

CRhythmArrow::~CRhythmArrow()
{
	if(m_pField)
		m_pField->UnregisterArrow(this);
	if(m_TailLaserId >= 0)
		Server()->SnapFreeID(m_TailLaserId);
}
