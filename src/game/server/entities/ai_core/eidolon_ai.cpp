#include "get_valid_ai_util.h"
#include "eidolon_ai.h"
#include "mob_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/gamecontext.h>

#include <game/server/core/scenarios/scenario_eidolon.h>
#include <game/server/core/tools/scenario_player_manager.h>

CEidolonAI::CEidolonAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter) {}

bool CEidolonAI::CanDamage(CPlayer* pFrom)
{
	if(pFrom->IsBot())
	{
		auto* pFromBot = static_cast<CPlayerBot*>(pFrom);
		if(auto* pMobAI = GetValidAI<CMobAI>(pFromBot))
			return !pMobAI->IsNeutral();
	}

	auto* pOwner = m_pPlayer->GetEidolonOwner();
	if(pOwner != pFrom)
	{
		if(pOwner)
		{
			auto* pOwnerLastAttacker = pOwner->GetCharacter()->GetLastPlayerAttacker(2);
			if(pOwnerLastAttacker &&
				pOwnerLastAttacker->GetCID() == pFrom->GetCID())
				return true;

			auto* pFromChar = pFrom->GetCharacter();
			if(pFromChar)
			{
				auto pFromLastAttacker = pFromChar->GetLastPlayerAttacker(2);
				if(pFromLastAttacker &&
					(pFromLastAttacker->GetCID() == pOwner->GetCID() ||
						pFromLastAttacker->GetCID() == m_pPlayer->GetCID()))
					return true;
			}
		}

		return false;
	}

	return false;
}

void CEidolonAI::OnSpawn()
{
	GS()->ScenarioPlayerManager()->RegisterScenario<CEidolonScenario>(m_ClientID);
	m_pCharacter->m_Core.m_Solo = true;
}

void CEidolonAI::OnDie(int Killer, int Weapon)
{
	auto* pOwner = m_pPlayer->GetEidolonOwner();
	if(!pOwner || !pOwner->GetCharacter())
		return;

	if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
	{
		auto EquippedEidolonItemIdOpt = pOwner->GetEquippedSlotItemID(ItemType::EquipEidolon);
		if(EquippedEidolonItemIdOpt.has_value())
		{
			auto* pPlayerItem = pOwner->GetItem(EquippedEidolonItemIdOpt.value());
			if(pPlayerItem && pPlayerItem->GetDurability() > 0)
			{
				//pPlayerItem->SetDurability(0); TODO crashed by event listener inventory durability item it's updated from character all item's and these item's destroyed eidolon character.
				pOwner->GS()->Chat(pOwner->GetCID(), "Your eidolon item 'durability is 0'.");
			}
		}
	}
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
