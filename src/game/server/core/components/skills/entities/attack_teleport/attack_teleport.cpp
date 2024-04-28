/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "attack_teleport.h"

#include <game/server/gamecontext.h>

CAttackTeleport::CAttackTeleport(CGameWorld *pGameWorld, vec2 Pos, CPlayer* pPlayer, int SkillBonus)
: CEntity(pGameWorld, CGameWorld::ENTYPE_NOCTIS_TELEPORT, Pos, 28.0f)
{
	m_pPlayer = pPlayer;
	m_Direction = (m_pPlayer ? vec2(m_pPlayer->m_pLastInput->m_TargetX, m_pPlayer->m_pLastInput->m_TargetY) : vec2(0, 0));
	m_SkillBonus = SkillBonus;
	m_LifeSpan = Server()->TickSpeed();
	GameWorld()->InsertEntity(this);
}

CAttackTeleport::~CAttackTeleport()
{
	GS()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);
}

void CAttackTeleport::Tick()
{
	// check player and character to exist
	if(!m_pPlayer || !m_pPlayer->GetCharacter())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// life span 
	m_LifeSpan--;

	// variables
	vec2 To = m_Pos + normalize(m_Direction) * 20.0f;
	vec2 Size = vec2(GetProximityRadius(), GetProximityRadius());
	CCharacter* pOwnerChar = m_pPlayer->GetCharacter();
	CCharacter *pSearchChar = (CCharacter*)GS()->m_World.ClosestEntity(To, GetProximityRadius(), CGameWorld::ENTTYPE_CHARACTER, nullptr);

	// check collide
	const bool IsCollide = (GS()->Collision()->TestBox(m_Pos, Size) || GS()->Collision()->TestBox(To, Size)
		|| GS()->m_World.IntersectClosestDoorEntity(m_Pos, GetProximityRadius()) || GS()->m_World.IntersectClosestDoorEntity(To, GetProximityRadius()));
	const bool IsAllowedPVP = (pSearchChar && pSearchChar->IsAlive() && pSearchChar != pOwnerChar && pSearchChar->IsAllowedPVP(pOwnerChar->GetPlayer()->GetCID()));

	// functional
	if(!m_LifeSpan || IsCollide || IsAllowedPVP)
	{
		// sound effect
		GS()->CreateSound(pOwnerChar->GetPos(), SOUND_NINJA_FIRE);

		// damage for players
		const int ClientID = pOwnerChar->GetPlayer()->GetCID();
		const int MaximalDamageSize = translate_to_percent_rest(pOwnerChar->GetPlayer()->GetAttributeSize(AttributeIdentifier::DMG), clamp(m_SkillBonus, 5, 50));
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pSearchPlayer = GS()->GetPlayer(i, false, true);

			// checking allowed pvp 
			if(ClientID == i || !pSearchPlayer || !pSearchPlayer->GetCharacter()->IsAllowedPVP(ClientID))
				continue;

			// checking collision
			if(distance(m_Pos, pSearchPlayer->GetCharacter()->GetPos()) > 320
				|| GS()->Collision()->IntersectLineWithInvisible(m_Pos, pSearchPlayer->GetCharacter()->GetPos(), 0, 0))
				continue;

			// take damage
			vec2 SearchPos = pSearchPlayer->GetCharacter()->GetPos();
			vec2 Diff = SearchPos - m_Pos;
			vec2 Force = normalize(Diff) * 16.0f;
			pSearchPlayer->GetCharacter()->TakeDamage(Force * 12.0f, MaximalDamageSize, ClientID, WEAPON_NINJA);
			GS()->CreateExplosion(SearchPos, pOwnerChar->GetPlayer()->GetCID(), WEAPON_GRENADE, 0);
			GS()->CreateSound(SearchPos, SOUND_NINJA_HIT);
		}

		// change to new position
		pOwnerChar->ChangePosition(m_Pos);

		// destroy entity
		GameWorld()->DestroyEntity(this);
		return;
	}

	// move object
	m_PosTo = m_Pos;
	m_Pos += normalize(m_Direction) * 20.0f;
}

void CAttackTeleport::Snap(int SnappingClient)
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