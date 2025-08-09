/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "skill_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>

#include <game/server/entities/character_bot.h>
#include <game/server/core/entities/group/entitiy_group.h>

#include "entities/attack_teleport/attack_teleport.h"
#include "entities/heart_healer.h"

CGS* CSkill::GS() const
{
	return (CGS*)Instance::Server()->GameServerPlayer(m_ClientID);
}

CPlayer* CSkill::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

std::string CSkill::GetStringLevelStatus() const
{
	// is not learned
	if(!IsLearned())
	{
		return "(not learned)";
	}

	// is not maximal level
	if(m_Level < Info()->GetMaxLevel())
	{
		return "(" + std::to_string(m_Level) + " of " + std::to_string(Info()->GetMaxLevel()) + ")";
	}

	// max level
	return "(max)";
}

void CSkill::SelectNextControlEmote()
{
	const auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	if(m_SelectedEmoticon == -1)
		m_SelectedEmoticon = 0;
	else
	{
		if(m_SelectedEmoticon == NUM_EMOTICONS)
			m_SelectedEmoticon = -1;
		else
			m_SelectedEmoticon++;
	}

	Database->Execute<DB::UPDATE>("tw_accounts_skills", "UsedByEmoticon = '{}' WHERE SkillID = '{}' AND UserID = '{}'",
		m_SelectedEmoticon, m_ID, pPlayer->Account()->GetID());
}

bool CSkill::Use()
{
	// check is learned
	if(m_Level <= 0)
		return false;

	// check player valid
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// check character valid
	auto* pChar = pPlayer->GetCharacter();
	if(!pChar)
		return false;

	// check profession skill
	const auto ProfID = Info()->GetProfessionID();
	if(ProfID != ProfessionIdentifier::None && pPlayer->Account()->GetActiveProfessionID() != ProfID)
	{
		const char* pProfName = GetProfessionName(ProfID);
		GS()->Chat(m_ClientID, "You can use this skill with '{} profession'.", pProfName);
		return false;
	}

	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int ManaCost = maximum(1, translate_to_percent_rest(pPlayer->GetMaxMana(), Info()->GetPercentageCost()));
	const vec2 PlayerPosition = pChar->GetPos();
	auto& pEntSkillPtr = m_apEntSkillPtrs[m_ID];


	// Cure I
	if(IsActivated(pChar, ManaCost, SKILL_CURE))
	{
		// cure near players
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			auto* pSearch = GS()->GetPlayer(i, true, true);
			if(!pSearch || !GS()->IsPlayerInWorld(i))
				continue;

			// check distance
			if(distance(PlayerPosition, pSearch->GetCharacter()->GetPos()) > 800)
				continue;

			// dissalow heal for pvp clients
			if(i != ClientID && pSearch->GetCharacter()->IsAllowedPVP(ClientID))
				continue;

			// create healt
			const auto PowerLevel = maximum(ManaCost + translate_to_percent_rest(ManaCost, minimum(GetBonus(), 100)), 1);
			new CHeartHealer(&GS()->m_World, PlayerPosition, pSearch, PowerLevel, pSearch->GetCharacter()->m_Core.m_Vel, true);
			GS()->CreateDeath(pSearch->GetCharacter()->GetPos(), i);
		}

		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	// Blessing god war
	if(IsActivated(pChar, ManaCost, SKILL_BLESSING_GOD_WAR))
	{
		// blessing near players
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			// check valid player
			auto* pSearchPl = GS()->GetPlayer(i);
			if(!pSearchPl || !GS()->IsPlayerInWorld(i))
				continue;

			// check valid character
			auto* pSearchChar = pSearchPl->GetCharacter();
			if(!pSearchChar)
				continue;

			// check valid distance
			if(distance(PlayerPosition, pSearchChar->GetPos()) > 800)
				continue;

			// check allow for pvp
			if(i != ClientID && pSearchChar->IsAllowedPVP(ClientID))
				continue;

			// restore ammo
			const int RealAmmo = 10 + pSearchPl->GetTotalAttributeValue(AttributeIdentifier::Ammo);
			const int RestoreAmmo = translate_to_percent_rest(RealAmmo, minimum(GetBonus(), 100));

			for(int j = WEAPON_GUN; j <= WEAPON_LASER; j++)
			{
				pSearchChar->GiveWeapon(j, RestoreAmmo);
				GS()->CreateDeath(PlayerPosition, i);
			}

			GS()->CreateSound(PlayerPosition, SOUND_CTF_GRAB_PL);
		}

		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		GS()->EntityManager()->Text(PlayerPosition + vec2(0, -96), 40, "RECOVERY AMMO");
		return true;
	}

	//*
	//* TANK
	//*
	if(IsActivated(pChar, ManaCost, SKILL_PROVOKE))
	{
		// provoke mobs
		bool MissedProvoked = false;
		for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
		{
			// check player valid
			auto* pSearchPl = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
			if(!pSearchPl || !GS()->IsPlayerInWorld(i))
				continue;

			// check character valid
			auto* pSearchChar = dynamic_cast<CCharacterBotAI*>(pSearchPl->GetCharacter());
			if(!pSearchChar)
				continue;

			// check distance
			if(distance(PlayerPosition, pSearchChar->GetPos()) > 800)
				continue;

			// check allowed pvp
			if(!pSearchChar->IsAllowedPVP(ClientID))
				continue;

			// check target upper agression
			if(const auto* pTargetChar = GS()->GetPlayerChar(pSearchChar->AI()->GetTarget()->GetCID()))
			{
				if(pTargetChar->GetPlayer()->GetMaxHealth() > pPlayer->GetMaxHealth())
				{
					MissedProvoked = true;
					continue;
				}

				// set target for client
				pSearchPl->m_TargetPos = pTargetChar->GetPos();
			}

			// set agression
			pSearchChar->AI()->GetTarget()->Set(ClientID, GetBonus());
			GS()->EntityManager()->FlyingPoint(PlayerPosition, i, pSearchPl->GetCharacter()->m_Core.m_Vel);
			GS()->CreatePlayerSpawn(pSearchPl->GetCharacter()->GetPos());
			pSearchPl->GetCharacter()->SetEmote(EMOTE_ANGRY, 10, true);
		}

		// some effects
		GS()->CreateSound(PlayerPosition, SOUND_NINJA_FIRE);
		GS()->EntityManager()->Text(PlayerPosition + vec2(0, -96), 40, "PROVOKE");
		if(MissedProvoked)
		{
			GS()->Chat(ClientID, "Some were not provoked due to a stronger provocation.");
		}

		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_SLEEPY_GRAVITY, SKILL_USAGE_RESET))
	{
		const auto UpgradedValue = minimum(200.f + GetBonus(), 400.f);
		const auto Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG));
		GS()->EntityManager()->GravityDisruption(ClientID, PlayerPosition, UpgradedValue, 10 * Server()->TickSpeed(), Damage, &pEntSkillPtr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}


	if(IsActivated(pChar, ManaCost, SKILL_LAST_STAND, SKILL_USAGE_TOGGLE))
	{
		const int PercentManaCost = maximum(Info()->GetPercentageCost() - GetBonus(), 15);
		GS()->EntityManager()->LastStand(ClientID, PlayerPosition, 96.f, PercentManaCost, &pEntSkillPtr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}


	//*
	//* DPS
	//*
	if(IsActivated(pChar, ManaCost, SKILL_ATTACK_TELEPORT))
	{
		new CAttackTeleport(&GS()->m_World, PlayerPosition, pPlayer, GetBonus());
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_FLAME_WALL, SKILL_USAGE_RESET))
	{
		const auto UpgradedValue = minimum(200.f + GetBonus(), 320.f);
		const auto Damage = maximum(1, translate_to_percent_rest(pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG), 5.0f));

		GS()->EntityManager()->FlameWall(ClientID, PlayerPosition, UpgradedValue, 10 * Server()->TickSpeed(), Damage, 0.3f);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}


	//*
	//* HEALER
	//*
	if(IsActivated(pChar, ManaCost, SKILL_MAGIC_BOW, SKILL_USAGE_TOGGLE))
	{
		const auto Shots = 1 + GetBonus();
		const auto Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG));
		GS()->EntityManager()->Bow(ClientID, Damage, Shots, 180.f, 8, &pEntSkillPtr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_HEART_TURRET, SKILL_USAGE_RESET))
	{
		// initialize
		constexpr int NumCastClicked = 5;
		const auto UpgradeValue = (10 + GetBonus()) * Server()->TickSpeed();
		auto FuncExecuteHealingRift = [this, UpgradeValue, ManaCost](int FinalClientID, vec2 FinalPosition, EntGroupWeakPtr* pFinalSkillTracker)
		{
			GS()->EntityManager()->HealthTurret(FinalClientID, FinalPosition, ManaCost, UpgradeValue, 2 * Server()->TickSpeed(), pFinalSkillTracker);
		};

		// start casting
		GS()->EntityManager()->StartUniversalCast(ClientID, PlayerPosition, NumCastClicked, FuncExecuteHealingRift, &pEntSkillPtr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_HEALING_AURA, SKILL_USAGE_NONE))
	{
		// initialize
		constexpr int NumCastClicked = 7;
		const auto UpgradeValue = minimum(320.f + GetBonus(), 400.f);
		auto FuncExecuteHealingAura = [this, UpgradeValue, ManaCost](int FinalClientID, vec2 FinalPosition, EntGroupWeakPtr* pFinalSkillTracker)
		{
			GS()->EntityManager()->HealingAura(FinalClientID, FinalPosition, UpgradeValue, 10 * Server()->TickSpeed(), ManaCost);
		};

		// start casting
		GS()->EntityManager()->StartUniversalCast(ClientID, PlayerPosition, NumCastClicked, FuncExecuteHealingAura, nullptr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_HEALING_RIFT, SKILL_USAGE_RESET))
	{
		// initialize
		constexpr int NumCastClicked = 16;
		constexpr int Lifetime = 15 * SERVER_TICK_SPEED;
		const auto HealingPerPulse = ManaCost;
		auto FuncExecuteHealingRift = [this, Lifetime, HealingPerPulse](int FinalClientID, vec2 FinalPosition, EntGroupWeakPtr* pFinalSkillTracker)
		{
			GS()->EntityManager()->HealingRift(FinalClientID, FinalPosition, 120.f, 320.f, Lifetime, 1.5f, 2, HealingPerPulse, 10, 6, pFinalSkillTracker);
		};

		// start casting
		GS()->EntityManager()->StartUniversalCast(ClientID, PlayerPosition, NumCastClicked, FuncExecuteHealingRift, &pEntSkillPtr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	return false;
}

bool CSkill::IsActivated(CCharacter* pChar, int Manacost, int SkillID, int SkillUsage) const
{
	if(m_ID != SkillID)
		return false;

	auto& skillEntityPtr = m_apEntSkillPtrs[m_ID];

	// reset skill when use
	if(SkillUsage == SKILL_USAGE_RESET)
	{
		if(!pChar->TryUseMana(Manacost))
			return false;

		if(const auto groupPtr = skillEntityPtr.lock())
			groupPtr->Clear();

		return true;
	}

	// toggle skill when use
	if(SkillUsage == SKILL_USAGE_TOGGLE)
	{
		if(const auto groupPtr = skillEntityPtr.lock())
		{
			GS()->Broadcast(m_ClientID, BroadcastPriority::GameWarning, 100, "The {} has been disabled!", Info()->GetName());
			groupPtr->Clear();
			return false;
		}

		if(!pChar->TryUseMana(Manacost))
			return false;

		GS()->Broadcast(m_ClientID, BroadcastPriority::GameWarning, 100, "The {} has been enabled!", Info()->GetName());
		return true;
	}

	return pChar->TryUseMana(Manacost);
}

bool CSkill::Upgrade()
{
	// check player exists
	const auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// check for maximal leveling
	const int ClientID = pPlayer->GetCID();
	if(m_Level >= Info()->GetMaxLevel())
	{
		GS()->Chat(ClientID, "You've already reached the maximum level");
		return false;
	}

	// try spend skill points
	if(!pPlayer->Account()->SpendCurrency(Info()->GetPriceSP(), itSkillPoint))
		return false;

	// update level
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE SkillID = '{}' AND UserID = '{}'", m_ID, pPlayer->Account()->GetID());
	if(pRes->next())
	{
		m_Level++;
		Database->Execute<DB::UPDATE>("tw_accounts_skills", "Level = '{}' WHERE SkillID = '{}' AND UserID = '{}'", m_Level, m_ID, pPlayer->Account()->GetID());
		GS()->Chat(ClientID, "Increased the skill ['{}' level to '{}'].", Info()->GetName(), m_Level);
	}
	else
	{
		m_Level = 1;
		m_SelectedEmoticon = -1;
		Database->Execute<DB::INSERT>("tw_accounts_skills", "(SkillID, UserID, Level) VALUES ('{}', '{}', '1');", m_ID, pPlayer->Account()->GetID());
		GS()->Chat(ClientID, "Learned a new skill ['{}']", Info()->GetName());
	}

	return true;
}
