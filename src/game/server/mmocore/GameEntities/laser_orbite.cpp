/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser_orbite.h"

#include <game/server/gamecontext.h>

CLaserOrbite::CLaserOrbite(CGameWorld* pGameWorld, int ClientID, vec2* pAttachedPos, int Amount, EntLaserOrbiteType Type, float Speed, float Radius)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, vec2(0.f, 0.f)), m_MoveType(Type), m_ClientID(ClientID), m_MoveSpeed(Speed), m_Radius(Radius), m_pAttachedPos(pAttachedPos)
{
	GameWorld()->InsertEntity(this);
	m_IDs.set_size(Amount);
	for(int i = 0; i < m_IDs.size(); i++)
		m_IDs[i] = Server()->SnapNewID();

	if(const CPlayer* pPlayer = GS()->GetPlayer(ClientID, false, true))
		m_pAttachedPos = &pPlayer->GetCharacter()->m_Core.m_Pos;
}

void CLaserOrbite::Reset()
{
	for(int i = 0; i < m_IDs.size(); i++)
		Server()->SnapFreeID(m_IDs[i]);
	m_IDs.clear();
	GS()->m_World.DestroyEntity(this);
}

void CLaserOrbite::Tick()
{
	if(m_pAttachedPos == nullptr)
	{
		Reset();
		return;
	}

	m_Pos = *m_pAttachedPos;
}

vec2 CLaserOrbite::UtilityOrbitePos(int PosID) const
{
	float AngleStart = 2.0f * pi;
	float AngleStep = 2.0f * pi / (float)m_IDs.size();
	if(m_MoveType == EntLaserOrbiteType::MOVE_LEFT)
		AngleStart = -(AngleStart * (float)Server()->Tick() / (float)Server()->TickSpeed()) * m_MoveSpeed;
	else if(m_MoveType == EntLaserOrbiteType::MOVE_RIGHT)
		AngleStart = (AngleStart * (float)Server()->Tick() / (float)Server()->TickSpeed()) * m_MoveSpeed;

	return { m_Radius * cos(AngleStart + AngleStep * (float)PosID), m_Radius * sin(AngleStart + AngleStep * (float)PosID) };
}

void CLaserOrbite::Snap(int SnappingClient)
{
	if(!m_pAttachedPos || NetworkClipped(SnappingClient, *m_pAttachedPos))
		return;

	if(const CPlayer* pPlayer = GS()->GetPlayer(m_ClientID); pPlayer && pPlayer->IsVisibleForClient(SnappingClient) != 2)
		return;

	vec2 LastPosition = *m_pAttachedPos + UtilityOrbitePos(m_IDs.size() - 1);
	for(int i = 0; i < m_IDs.size(); i++)
	{
		vec2 PosStart = *m_pAttachedPos + UtilityOrbitePos(i);

		CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs[i], sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_FromX = (int)PosStart.x;
		pObj->m_FromY = (int)PosStart.y;
		pObj->m_X = (int)LastPosition.x;
		pObj->m_Y = (int)LastPosition.y;
		pObj->m_StartTick = Server()->Tick() - 3;

		LastPosition = PosStart;
	}
}

