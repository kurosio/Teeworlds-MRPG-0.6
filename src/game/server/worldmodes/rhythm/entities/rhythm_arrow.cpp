/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "rhythm_arrow.h"
#include "rhythm_field.h"

#include <base/math.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>

CRhythmArrow::CRhythmArrow(CGameWorld *pGameWorld, CRhythmField *pField, vec2 Origin, vec2 Direction, float SpeedPerTick, int HitTick, int LaneIndex, float MissY) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Origin),
	m_pField(pField),
	m_Origin(Origin),
	m_Direction(Direction),
	m_Phase(0.0f),
	m_Speed(SpeedPerTick),
	m_SpawnTick(Server()->Tick()),
	m_HitTick(HitTick),
	m_LaneIndex(LaneIndex),
	m_MissY(MissY)
{
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
}
