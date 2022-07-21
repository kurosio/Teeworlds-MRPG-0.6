/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "noctis-teleport.h"

#include <game/server/gamecontext.h>

CNoctisTeleport::CNoctisTeleport(CGameWorld *pGameWorld, vec2 Pos, CCharacter* pPlayerChar, int SkillBonus)
: CEntity(pGameWorld, CGameWorld::ENTYPE_NOCTIS_TELEPORT, Pos, 28.0f)
{
	// transmitted arguments
	m_pPlayerChar = pPlayerChar;
	m_Direction = (m_pPlayerChar ? vec2(m_pPlayerChar->m_Core.m_Input.m_TargetX, m_pPlayerChar->m_Core.m_Input.m_TargetY) : vec2(0, 0));
	m_SkillBonus = SkillBonus;
	m_LifeSpan = Server()->TickSpeed();
	GameWorld()->InsertEntity(this);
}

void CNoctisTeleport::Reset()
{
	GS()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);
	GS()->m_World.DestroyEntity(this);
	return;
}

void CNoctisTeleport::Tick()
{
	m_LifeSpan--;
	if(!m_pPlayerChar || !m_pPlayerChar->IsAlive())
	{
		Reset();
		return;
	}

	// TODO: optimize
	vec2 To = m_Pos + normalize(m_Direction) * 20.0f;
	vec2 Size = vec2(GetProximityRadius(), GetProximityRadius());
	CCharacter *pSearchChar = (CCharacter*)GS()->m_World.ClosestEntity(To, GetProximityRadius(), CGameWorld::ENTTYPE_CHARACTER, nullptr);
	const bool IsCollide = (GS()->Collision()->TestBox(m_Pos, Size) || GS()->Collision()->TestBox(To, Size)
		|| GS()->m_World.IntersectClosestDoorEntity(m_Pos, GetProximityRadius()) || GS()->m_World.IntersectClosestDoorEntity(To, GetProximityRadius()));

	if(!m_LifeSpan || IsCollide || (pSearchChar && pSearchChar->IsAlive() && pSearchChar != m_pPlayerChar && pSearchChar->IsAllowedPVP(m_pPlayerChar->GetPlayer()->GetCID())))
	{
		GS()->CreateSound(m_pPlayerChar->GetPos(), SOUND_NINJA_FIRE);

		// change to new position
		vec2 OldPosition = m_Pos;
		m_pPlayerChar->ChangePosition(OldPosition);

		const int ClientID = m_pPlayerChar->GetPlayer()->GetCID();
		const int MaximalDamageSize = translate_to_percent_rest(m_pPlayerChar->GetPlayer()->GetAttributeCount(Stats::StStrength, true), clamp(m_SkillBonus, 5, 50));
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pSearchPlayer = GS()->GetPlayer(i, false, true);
			if(ClientID == i || !pSearchPlayer || !pSearchPlayer->GetCharacter()->IsAllowedPVP(ClientID)
				|| distance(OldPosition, pSearchPlayer->GetCharacter()->GetPos()) > 320
				|| GS()->Collision()->IntersectLineWithInvisible(OldPosition, pSearchPlayer->GetCharacter()->GetPos(), 0, 0))
				continue;

			// change position to player
			vec2 SearchPos = pSearchPlayer->GetCharacter()->GetPos();
			m_pPlayerChar->ChangePosition(SearchPos);

			// take damage
			vec2 Diff = SearchPos - OldPosition;
			vec2 Force = normalize(Diff) * 16.0f;
			pSearchPlayer->GetCharacter()->TakeDamage(Force * 12.0f, MaximalDamageSize, ClientID, WEAPON_NINJA);
			GS()->CreateExplosion(SearchPos, m_pPlayerChar->GetPlayer()->GetCID(), WEAPON_GRENADE, 0);
			GS()->CreateSound(SearchPos, SOUND_NINJA_HIT);
		}
		m_pPlayerChar->ChangePosition(OldPosition);

		// reset entity
		Reset();
		return;
	}

	m_PosTo = m_Pos;
	m_Pos += normalize(m_Direction) * 20.0f;
}

void CNoctisTeleport::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_PosTo.x;
	pObj->m_Y = (int)m_PosTo.y;
	pObj->m_FromX = (int)m_Pos.x;
	pObj->m_FromY = (int)m_Pos.y;
	pObj->m_StartTick = Server()->Tick();
}