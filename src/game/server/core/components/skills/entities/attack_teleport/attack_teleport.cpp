/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "attack_teleport.h"

#include <game/server/gamecontext.h>

CAttackTeleport::CAttackTeleport(CGameWorld *pGameWorld, vec2 Pos, CPlayer* pPlayer, int SkillBonus)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_SKILL, Pos, 28.0f)
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
	const auto ClientID = m_pPlayer->GetCID();
	const auto To = m_Pos + normalize(m_Direction) * 20.0f;
	const auto Size = vec2(m_Radius, m_Radius);
	auto* pOwnerChar = m_pPlayer->GetCharacter();
	auto* pSearchChar = (CCharacter*)GS()->m_World.ClosestEntity(To, m_Radius, CGameWorld::ENTTYPE_CHARACTER, pOwnerChar);

	// check collide
	const bool IsCollide = (GS()->Collision()->TestBox(m_Pos, Size) || GS()->Collision()->TestBox(To, Size)
		|| GS()->m_World.IntersectClosestDoorEntity(m_Pos, m_Radius) || GS()->m_World.IntersectClosestDoorEntity(To, m_Radius));
	const bool IsAllowedPVP = (pSearchChar && pSearchChar->IsAlive() && pSearchChar->IsAllowedPVP(ClientID));

	// first part
	if(!m_SecondPart)
	{
		if(!m_LifeSpan || IsCollide || IsAllowedPVP)
		{
			// sound effect
			GS()->CreateSound(pOwnerChar->GetPos(), SOUND_NINJA_FIRE);

			// damage for players
			const auto currentDmgSize = m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG);
			const int maxDmgSize = maximum(1, translate_to_percent_rest(currentDmgSize, clamp(m_SkillBonus, 1, 30)));

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				auto* pSearchPlayer = GS()->GetPlayer(i, false, true);
				if(ClientID == i || !pSearchPlayer || !pSearchPlayer->GetCharacter()->IsAllowedPVP(ClientID))
					continue;

				// checking collision
				if(distance(m_Pos, pSearchPlayer->GetCharacter()->GetPos()) > 320
					|| GS()->Collision()->IntersectLineWithInvisible(m_Pos, pSearchPlayer->GetCharacter()->GetPos(), 0, 0))
					continue;

				// take damage
				vec2 SearchPos = pSearchPlayer->GetCharacter()->GetPos();
				GS()->CreateSound(SearchPos, SOUND_NINJA_HIT);
				GS()->CreateExplosion(SearchPos, ClientID, WEAPON_GAME, maxDmgSize);
				pOwnerChar->IncreaseHealth(maxDmgSize);
				m_vMovingMap.push_back(pSearchPlayer);
			}

			// change to new position
			pOwnerChar->ChangePosition(m_Pos);
			m_SecondPartTimeleft = round_to_int((float)Server()->TickSpeed() * 1.5f);
			m_SecondPart = true;
		}

		// move object
		m_PosTo = m_Pos;
		m_Pos += normalize(m_Direction) * 20.0f;
	}
	// second part
	else
	{
		if(!m_vMovingMap.empty() && m_SecondPartTimeleft > 0)
		{
			// information about second part
			GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, Server()->TickSpeed(), "Press fire for attacks by skill: {} attacks!", m_SecondPartCombo);
			m_SecondPartTimeleft--;

			// is clicked fire
			if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE))
			{
				CPlayer* pNextPlayer = nullptr;
				CCharacter* pNextChar = nullptr;
				const auto currentDmgSize = m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG);
				const int maxDmgSize = maximum(1, translate_to_percent_rest(currentDmgSize, clamp(m_SkillBonus, 1, 30)));

				// try get next player
				while(!pNextPlayer && !pNextChar && !m_vMovingMap.empty())
				{
					auto randIt = m_vMovingMap.begin();
					std::advance(randIt, rand() % m_vMovingMap.size());
					pNextPlayer = (*randIt);

					if(!pNextPlayer || !pNextPlayer->GetCharacter()
						|| (pNextPlayer && GS()->Collision()->IntersectLineColFlag(m_pPlayer->m_ViewPos, pNextPlayer->m_ViewPos, nullptr, nullptr, CCollision::COLFLAG_DISALLOW_MOVE)))
					{
						m_vMovingMap.erase(randIt);
						continue;
					}

					pNextChar = pNextPlayer->GetCharacter();
				}

				// if exists next character
				if(pNextChar)
				{
					// take damage and some buffs
					const auto SearchPos = pNextChar->GetPos();
					const auto StunTime = 3;

					if(pNextPlayer->m_Effects.Add("Stun", StunTime * Server()->TickSpeed()))
					{
						GS()->Chat(pNextPlayer->GetCID(), "You have been stunned for '{} seconds'!", StunTime);
					}

					GS()->CreateExplosion(SearchPos, ClientID, WEAPON_GAME, maxDmgSize);
					pOwnerChar->IncreaseHealth(maxDmgSize);
					pOwnerChar->ChangePosition(pNextChar->GetPos());

					GS()->CreateSound(SearchPos, SOUND_NINJA_FIRE);
					GS()->CreateSound(SearchPos, SOUND_NINJA_HIT);
					m_Pos = m_PosTo = SearchPos;
					m_SecondPartCombo++;

					// add external explosion damage for other player's
					for(const auto& pPlayer : m_vMovingMap)
					{
						if(pPlayer && pPlayer->GetCharacter() && pNextPlayer != pPlayer)
						{
							const auto maxMovingMapDmg = maximum(1, round_to_int((float)maxDmgSize * 0.1f));
							pPlayer->GetCharacter()->TakeDamage({}, maxMovingMapDmg, m_ClientID, WEAPON_GAME);
							pOwnerChar->IncreaseHealth(maxMovingMapDmg);
							GS()->CreateDeath(pPlayer->m_ViewPos, pPlayer->GetCID());
						}
					}
				}
			}

			// skip destroy block
			return;
		}

		// destroy entity
		GameWorld()->DestroyEntity(this);
	}
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