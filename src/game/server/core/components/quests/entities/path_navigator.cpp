/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"
#include <game/server/gamecontext.h>

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/core/tools/path_finder.h>

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, int ClientID, bool StartByCreating, vec2 SearchPos, int WorldID, bool Projectile, int64_t Mask)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_NAVIGATOR, {}, 0, ClientID)
{
	m_Mask = Mask;
	m_StepPos = 0;
	m_Projectile = Projectile;
	m_StartByCreating = StartByCreating;

	const auto* pPlayer = GetOwner();
	const auto PosTo = GS()->Core()->WorldManager()->FindPosition(WorldID, SearchPos);
	if(PosTo && pPlayer)
	{
		m_Pos = pPlayer->m_ViewPos;
		m_PosTo = *PosTo;
	}
	else
	{
		MarkForDestroy();
	}

	GameWorld()->InsertEntity(this);
}

void CEntityPathNavigator::Tick()
{
	auto* pPlayer = GetOwner();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// if no path is available, request a new path
	vec2 PlayerPos = pPlayer->GetCharacter()->m_Core.m_Pos;
	if(m_PathHandle.vPath.empty())
	{
		if(m_TickLastIdle < Server()->Tick() && distance(PlayerPos, m_PosTo) > 240.f)
		{
			GS()->PathFinder()->RequestPath(m_PathHandle, PlayerPos, m_PosTo);
			m_StepPos = 0;
		}
		m_PathHandle.TryGetPath();
		return;
	}

	// perform movement if the countdown has expired
	if(m_TickCountDown < Server()->Tick())
		Move(pPlayer);
}

void CEntityPathNavigator::TickDeferred()
{
	auto* pPlayer = GetOwner();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// update last position
	vec2 PlayerPos = pPlayer->GetCharacter()->m_Core.m_Pos;
	if(m_LastPos != PlayerPos && m_TickLastIdle != -1 && !m_StartByCreating)
	{
		m_TickLastIdle = Server()->Tick() + (Server()->TickSpeed() * 3);
	}
	m_LastPos = PlayerPos;
}


void CEntityPathNavigator::Move(CPlayer* pPlayer)
{
	if(m_Projectile)
	{
		if(is_negative_vec(m_Pos))
		{
			m_Pos = m_PathHandle.vPath[m_StepPos];
			m_StepPos++;
		}

		// smooth movement
		if(m_StepPos < (int)m_PathHandle.vPath.size())
		{
			m_Pos += normalize(m_PathHandle.vPath[m_StepPos] - m_Pos) * 4.f;
		}

		// update timer by steps
		if(Server()->Tick() % (Server()->TickSpeed() / 8) == 0 && m_StepPos < (int)m_PathHandle.vPath.size())
		{
			m_Pos = m_PathHandle.vPath[m_StepPos];
			m_StepPos++;
		}

		return;
	}

	// update timer by steps
	if(Server()->Tick() % (Server()->TickSpeed() / 10) == 0)
	{
		m_Pos = m_PathHandle.vPath[m_StepPos];

		// create damage effect
		constexpr vec2 Corrector(32.f, 64.f);
		GS()->CreateDamage(m_Pos - Corrector, -1, 1, false, 0.f, m_Mask);
		m_StepPos++;

		// check if the entity should stop moving
		vec2 PlayerPos = pPlayer->GetCharacter()->m_Core.m_Pos;
		if(distance(PlayerPos, m_Pos) > 800.0f || m_StepPos >= (int)m_PathHandle.vPath.size())
		{
			m_TickCountDown = Server()->Tick() + (Server()->TickSpeed() * 2);
			m_StartByCreating = false;
			m_PathHandle.vPath.clear();
		}
	}
}

void CEntityPathNavigator::Snap(int SnappingClient)
{
	if(!m_Projectile || m_PathHandle.vPath.empty() || !CmaskIsSet(m_Mask, SnappingClient) || NetworkClipped(SnappingClient) || !GetOwnerChar())
		return;

	GS()->SnapProjectile(SnappingClient, GetID(), m_Pos, {}, Server()->Tick(), WEAPON_HAMMER, m_ClientID);
}