#include "eidolon_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/gamecontext.h>

#include <scenarios/impl/scenario_eidolon.h>
#include <scenarios/managers/scenario_player_manager.h>

CEidolonAI::CEidolonAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter) {}

bool CEidolonAI::CanDamage(CPlayer* pFrom)
{
	if(!pFrom)
		return false;

	const auto* pOwner = m_pPlayer->GetEidolonOwner();
	const auto* pOwnerCharacter = pOwner ? pOwner->GetCharacter() : nullptr;
	if(!pOwnerCharacter || pFrom == pOwner || !pFrom->GetCharacter())
		return false;

	// Use the owner's PvP, group, guild, scenario, bot and safe-zone rules.
	return pOwnerCharacter->IsAllowedPVP(pFrom->GetCID());
}

void CEidolonAI::OnSpawn()
{
	GS()->ScenarioPlayerManager()->RegisterScenario<CEidolonScenario>(m_ClientID);
	m_pCharacter->m_Core.m_Solo = true;
}

void CEidolonAI::OnDie(int Killer, int Weapon)
{
	// World deaths are used to remove an eidolon when its owner changes
	// equipment, dies or leaves the world. Self deaths include death tiles.
	if(Weapon == WEAPON_WORLD)
		return;

	auto* pOwner = m_pPlayer->GetEidolonOwner();
	const int ItemID = m_pPlayer->m_EidolonItemID;
	if (!pOwner || ItemID <= NOPE)
		return;

	if (auto* pItem = pOwner->GetItem(ItemID); pItem && pItem->HasItem() && pItem->GetDurability() > 0)
	{
		pItem->SetDurability(0);
		GS()->Chat(pOwner->GetCID(), "Your eidolon has fallen. Repair its item or respawn to summon it again.");
	}

	pOwner->GetSharedData().m_PendingEidolonItemBreaks.insert(ItemID);
	m_pPlayer->SetEidolonDead();
}

void CEidolonAI::OnTargetRules(float Radius)
{
	auto* pOwner = m_pPlayer->GetEidolonOwner();
	if(!pOwner || !pOwner->GetCharacter())
		return;

	// find from players
	const auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidate)
	{
		const bool DamageDisabled = pCandidate->GetCharacter()->m_Core.m_DamageDisabled;
		const bool AllowedPVP = pOwner->GetCID() != pCandidate->GetCID() && m_pCharacter->IsAllowedPVP(pCandidate->GetCID());
		return !DamageDisabled && AllowedPVP;
	});

	// try find from bots
	if(!pPlayer)
	{
		pPlayer = SearchPlayerBotCondition(Radius, [&](CPlayerBot* pCandidate)
		{
			const bool DamageDisabled = pOwner->GetCharacter()->m_Core.m_DamageDisabled;
			const bool AllowedPVP = m_pCharacter->IsAllowedPVP(pCandidate->GetCID())
				&& pCandidate->GetCharacter()->IsAllowedPVP(m_ClientID);
			return !DamageDisabled && AllowedPVP;
		});
	}

	if(pPlayer)
	{
		m_Target.Set(pPlayer->GetCID(), 100);
	}
}

void CEidolonAI::Process()
{
	m_pCharacter->SetSafeFlags(SAFEFLAG_COLLISION_DISABLED);

	const auto* pOwner = m_pPlayer->GetEidolonOwner();
	if(!pOwner || !pOwner->GetCharacter())
		return;

	const auto* pOwnerChar = pOwner->GetCharacter();
	const float Distance = distance(pOwnerChar->GetPos(), m_pCharacter->GetPos());
	if(Distance > 400.f && !m_Target.IsEmpty())
	{
		m_Target.Reset();
	}
	else
	{
		m_pCharacter->UpdateTarget(400.f);
	}

	if(const auto* pTargetChar = GS()->GetPlayerChar(m_Target.GetCID()))
	{
		m_pPlayer->m_TargetPos = pTargetChar->GetPos();
		m_pCharacter->SelectWeaponAtRandomInterval();
		m_pCharacter->Fire();
		m_pCharacter->Move();
		return;
	}

	if(Distance < 128.0f)
	{
		if(pOwnerChar->m_Core.m_HookState != HOOK_GRABBED)
		{
			m_pCharacter->ResetHook();
		}

		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_pCharacter->m_Input.m_TargetY = rand() % 4 - rand() % 8;
		}

		m_pPlayer->m_TargetPos = {};
		m_pCharacter->m_Input.m_TargetX = (m_pCharacter->m_Input.m_Direction * 10 + 1);
		m_pCharacter->m_Input.m_Direction = 0;
	}
	else
	{
		m_pPlayer->m_TargetPos = pOwnerChar->GetPos();
		m_pCharacter->Move();
	}
}

void CEidolonAI::OnSnapDDNetCharacter(int SnappingClient, CNetObj_DDNetCharacter* pDDNetCharacter)
{
	CPlayer* pOwner = m_pPlayer->GetEidolonOwner();
	if(!pOwner)
		return;

	if(pOwner->GetCID() != SnappingClient)
	{
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SOLO;
	}
	pDDNetCharacter->m_Flags |= CHARACTERFLAG_COLLISION_DISABLED;
}
