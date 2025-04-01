#include "eidolon_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/gamecontext.h>

#include <game/server/core/scenarios/scenario_eidolon.h>

CEidolonAI::CEidolonAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter) {}

bool CEidolonAI::CanDamage(CPlayer* pFrom)
{
	auto* pOwner = m_pPlayer->GetEidolonOwner();

	if(m_pPlayer->GetEidolonOwner() != pFrom)
	{
		if(pOwner)
		{
			auto* pOwnerLastAttacker = pOwner->GetCharacter()->GetLastAttacker();
			if(pOwnerLastAttacker &&
				pOwnerLastAttacker->GetCID() == pFrom->GetCID())
				return true;

			auto* pFromChar = pFrom->GetCharacter();
			if(pFromChar)
			{
				auto pFromLastAttacker = pFromChar->GetLastAttacker();
				if(pFromLastAttacker &&
					(pFromLastAttacker->GetCID() == pOwner->GetCID() ||
						pFromLastAttacker->GetCID() == m_pPlayer->GetCID()))
					return true;
			}
		}

		return false;
	}

	if(pFrom->IsBot())
	{
		const auto* pFromBot = static_cast<CPlayerBot*>(pFrom);
		if(pFromBot && (pFromBot->GetBotType() == TYPE_BOT_MOB))
			return true;
	}

	return false;
}

void CEidolonAI::OnSpawn()
{
	m_pPlayer->Scenarios().Start(std::make_unique<CEidolonScenario>());
	m_pCharacter->m_Core.m_Solo = true;
}

void CEidolonAI::OnDie(int Killer, int Weapon)
{
	auto* pOwner = m_pPlayer->GetEidolonOwner();
	if(!pOwner || !pOwner->GetCharacter())
		return;

	if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
	{
		auto optItemID = pOwner->GetEquippedItemID(ItemType::EquipEidolon);
		if(optItemID.has_value())
		{
			auto* pPlayerItem = pOwner->GetItem(optItemID.value());
			if(pPlayerItem && pPlayerItem->GetDurability() > 0)
			{
				pPlayerItem->SetDurability(0);
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
		// const auto* pOwnerChar = pOwner->GetCharacter();
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

			return !DamageDisabled && pOwner->GetCharacter()->IsAllowedPVP(pCandidate->GetCID());
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
	m_pCharacter->ResetInput();

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
