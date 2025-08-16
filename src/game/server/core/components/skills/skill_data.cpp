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

void CSkill::LoadTreeChoicesFromDB()
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// load from database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skill_tree", "WHERE UserID = '{}' AND SkillID = '{}'", pPlayer->Account()->GetID(), m_ID);
	m_TreeSelections.clear();
	while(pRes->next())
	{
		const int L = pRes->getInt("LevelIndex");
		const int O = pRes->getInt("OptionIndex");
		if(L > 0 && O > 0)
			m_TreeSelections[L] = O;
	}
}


int CSkill::GetTreeNodePriceSP(int LevelIndex, int OptionIndex) const
{
	const auto* pTree = CSkillTree::Get(m_ID);
	if(!pTree)
		return 0;

	const auto* pOpt = pTree->FindOption(LevelIndex, OptionIndex);
	return pOpt ? pOpt->PriceSP : 0;
}

bool CSkill::SetTreeOption(int LevelIndex, int OptionIndex)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer || !IsLearned())
		return false;

	const auto* pTree = CSkillTree::Get(m_ID);
	if(!pTree)
		return false;

	// check valid index
	if(LevelIndex <= 0 || LevelIndex > pTree->GetMaxLevels())
		return false;
	if(!pTree->HasOption(LevelIndex, OptionIndex))
		return false;

	// check is next level
	if(GetSelectedOption(LevelIndex) != 0)
		return false;
	if(LevelIndex != (GetTreeProgress() + 1))
		return false;

	// try spend sp
	const int CostSP = GetTreeNodePriceSP(LevelIndex, OptionIndex);
	if(CostSP > 0 && !pPlayer->Account()->SpendCurrency(CostSP, itSkillPoint))
		return false;

	// update
	Database->Execute<DB::INSERT>("tw_accounts_skill_tree",
		"(UserID, SkillID, LevelIndex, OptionIndex) VALUES ('{}','{}','{}','{}')",
		pPlayer->Account()->GetID(), m_ID, LevelIndex, OptionIndex);
	m_TreeSelections[LevelIndex] = OptionIndex;

	// message
	GS()->Chat(pPlayer->GetCID(), "Selected L{} -> Option {} for ['{}'] (cost: {} SP).",
		LevelIndex, OptionIndex, Info()->GetName(), CostSP);
	return true;
}


int CSkill::GetResetCostSP() const
{
	return Info()->GetPriceSP();
}

bool CSkill::ResetTree()
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer || !IsLearned())
		return false;

	// has tree progress
	if(GetTreeProgress() <= 0)
		return false;

	// try spend sp
	const int Cost = GetResetCostSP();
	if(Cost > 0 && !pPlayer->Account()->SpendCurrency(Cost, itSkillPoint))
		return false;

	// update & message
	Database->Execute<DB::REMOVE>("tw_accounts_skill_tree", "WHERE UserID = '{}' AND SkillID = '{}'", pPlayer->Account()->GetID(), m_ID);
	m_TreeSelections.clear();
	GS()->Chat(pPlayer->GetCID(), "Skill tree for ['{}'] was reset (cost: {} SP).", Info()->GetName(), Cost);
	return true;
}

int CSkill::GetMod(SkillMod Mod) const
{
	if(Mod == SkillMod::None)
		return 0;

	const auto* pTree = CSkillTree::Get(m_ID);
	if(!pTree)
		return 0;

	// calculate sum for mod
	int Sum = 0;
	for(const auto& [LevelIndex, OptionIndex] : m_TreeSelections)
	{
		const auto* pOpt = pTree->FindOption(LevelIndex, OptionIndex);
		if(pOpt && pOpt->ModType == Mod)
			Sum += pOpt->ModValue;
	}

	return Sum;
}

std::string CSkill::GetStringLevelStatus() const
{
	if(!IsLearned())
		return "(not learned)";

	const auto* pTree = CSkillTree::Get(m_ID);
	const int TreeMax = pTree ? pTree->GetMaxLevels() : 0;
	const int Progress = GetTreeProgress();

	if(TreeMax <= 0)
		return "(learned)";

	if(Progress < TreeMax)
		return "(" + std::to_string(Progress) + " of " + std::to_string(TreeMax) + ")";

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
	if(!m_Learned)
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
	const int ManaCost = maximum(1, translate_to_percent_rest(pPlayer->GetMaxMana(), Info()->GetManaCostPct()));
	const vec2 PlayerPosition = pChar->GetPos();

	// Cure I
	if(IsActivated(pChar, ManaCost, SKILL_CURE))
	{
		// initialize perks
		const auto Radius = 500.f + GetMod(SkillMod::Radius);
		const auto Heal = ManaCost + translate_to_percent_rest(ManaCost, (float)GetMod(SkillMod::BonusIncreasePct));
		const bool PerkCureAlly = GetMod(SkillMod::CureAlly) > 0;

		// is not perk cudre ally
		if(!PerkCureAlly)
		{
			pChar->IncreaseHealth(Heal);
			GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
			return true;
		}

		// cure near players
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			auto* pSearch = GS()->GetPlayer(i, true, true);
			if(!pSearch || !GS()->IsPlayerInWorld(i))
				continue;

			// check distance
			if(distance(PlayerPosition, pSearch->GetCharacter()->GetPos()) > Radius)
				continue;

			// dissalow heal for pvp clients
			if(i != ClientID && pSearch->GetCharacter()->IsAllowedPVP(ClientID))
				continue;

			// create healt
			new CHeartHealer(&GS()->m_World, PlayerPosition, pSearch, Heal, pSearch->GetCharacter()->m_Core.m_Vel, true);
			GS()->CreateDeath(pSearch->GetCharacter()->GetPos(), i);
		}

		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	// Blessing god war
	if(IsActivated(pChar, ManaCost, SKILL_BLESSING_GOD_WAR))
	{
		// initialize perks
		const auto Radius = 500.f + GetMod(SkillMod::Radius);
		const auto RestoreAmmoPct = round_to_int(10.f + (float)GetMod(SkillMod::BonusIncreasePct));

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
			if(distance(PlayerPosition, pSearchChar->GetPos()) > Radius)
				continue;

			// check allow for pvp
			if(i != ClientID && pSearchChar->IsAllowedPVP(ClientID))
				continue;

			// restore ammo
			const int RealAmmo = 10 + pSearchPl->GetTotalAttributeValue(AttributeIdentifier::Ammo);
			const int RestoreAmmo = translate_to_percent_rest(RealAmmo, clamp(RestoreAmmoPct, 30, 100));

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
		// initialize perks
		const auto Radius = 500.f + GetMod(SkillMod::Radius);
		const auto Agression = 100 + GetMod(SkillMod::BonusIncreaseValue);

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
			if(distance(PlayerPosition, pSearchChar->GetPos()) > Radius)
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
			pSearchChar->AI()->GetTarget()->Set(ClientID, Agression);
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
		// initialize perks
		const auto Radius = 160.f + GetMod(SkillMod::Radius);
		const int Lifetime = (8 + GetMod(SkillMod::Lifetime)) * SERVER_TICK_SPEED;
		const auto Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG));

		// create skill
		GS()->EntityManager()->GravityDisruption(ClientID, PlayerPosition, Radius, Lifetime, Damage, &m_pEntSkillPtrs);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}


	if(IsActivated(pChar, ManaCost, SKILL_LAST_STAND, SKILL_USAGE_TOGGLE))
	{
		// initialize perks
		const auto ManaCostPct = maximum(Info()->GetManaCostPct() - GetMod(SkillMod::ManaCostPct), 15);
		const auto Radius = 90.f + GetMod(SkillMod::Radius);

		// create skill
		GS()->EntityManager()->LastStand(ClientID, PlayerPosition, Radius, ManaCostPct, &m_pEntSkillPtrs);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}


	//*
	//* DPS
	//*
	if(IsActivated(pChar, ManaCost, SKILL_ATTACK_TELEPORT))
	{
		// initialize perks
		const auto DamagePct = GetMod(SkillMod::BonusIncreasePct);

		// create skill
		new CAttackTeleport(&GS()->m_World, PlayerPosition, pPlayer, DamagePct);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_FLAME_WALL, SKILL_USAGE_RESET))
	{
		// initialize perks
		const auto Radius = 180.f + GetMod(SkillMod::Radius);
		const int Lifetime = (8 + GetMod(SkillMod::Lifetime)) * SERVER_TICK_SPEED;
		const auto Damage = maximum(1, translate_to_percent_rest(pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG), 5.0f));

		// create skill
		GS()->EntityManager()->FlameWall(ClientID, PlayerPosition, Radius, Lifetime, Damage, 0.3f);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}


	//*
	//* HEALER
	//*
	if(IsActivated(pChar, ManaCost, SKILL_MAGIC_BOW, SKILL_USAGE_TOGGLE))
	{
		// initialize perks
		const auto NumShots = 1 + GetMod(SkillMod::BonusIncreaseValue);
		const auto Radius = 100.f + GetMod(SkillMod::Radius);
		const auto NumExplosion = round_to_int(Radius / 25.f);
		const auto Damage = maximum(1, pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG));

		// create skill
		GS()->EntityManager()->Bow(ClientID, Damage, NumShots, Radius, NumExplosion, &m_pEntSkillPtrs);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_HEART_TURRET, SKILL_USAGE_RESET))
	{
		// initialize perks
		const int NumCastClicked = 15 - GetMod(SkillMod::CastClick);
		const int Lifetime = (8 + GetMod(SkillMod::Lifetime)) * SERVER_TICK_SPEED;
		const auto HealPerTick = ManaCost + translate_to_percent_rest(ManaCost, (float)GetMod(SkillMod::BonusIncreasePct));

		// skill lambda
		auto FuncExecuteHealingRift = [this, Lifetime, HealPerTick](int CID, vec2 Pos, EntGroupWeakPtr* pSkillTracker)
		{
			GS()->EntityManager()->HealthTurret(CID, Pos, HealPerTick, Lifetime, 2 * Server()->TickSpeed(), pSkillTracker);
		};

		// start casting
		GS()->EntityManager()->StartUniversalCast(ClientID, PlayerPosition, NumCastClicked, FuncExecuteHealingRift, &m_pEntSkillPtrs);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_HEALING_AURA, SKILL_USAGE_NONE))
	{
		// initialize perks
		const int NumCastClicked = 15 - GetMod(SkillMod::CastClick);
		const float Radius = 240.f + (float)GetMod(SkillMod::Radius);
		const int Lifetime = (8 + GetMod(SkillMod::Lifetime)) * SERVER_TICK_SPEED;
		const auto HealPerTick = ManaCost + translate_to_percent_rest(ManaCost, (float)GetMod(SkillMod::BonusIncreasePct));

		// skill lambda
		auto FuncExecuteHealingAura = [this, Radius, Lifetime, HealPerTick](int CID, vec2 Pos, EntGroupWeakPtr* pSkillTracker)
		{
			GS()->EntityManager()->HealingAura(CID, Pos, Radius, Lifetime, HealPerTick);
		};

		// start casting
		GS()->EntityManager()->StartUniversalCast(ClientID, PlayerPosition, NumCastClicked, FuncExecuteHealingAura, nullptr);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	if(IsActivated(pChar, ManaCost, SKILL_HEALING_RIFT, SKILL_USAGE_RESET))
	{
		// initialize perks
		const int NumCastClicked = 25 - GetMod(SkillMod::CastClick);
		const float Radius = 240.f + (float)GetMod(SkillMod::Radius);
		const int Lifetime = (8 + GetMod(SkillMod::Lifetime)) * SERVER_TICK_SPEED;
		const auto HealPerPulse = ManaCost + translate_to_percent_rest(ManaCost, (float)GetMod(SkillMod::BonusIncreasePct));

		// skill lambda
		auto FuncExecuteHealingRift = [this, Lifetime, HealPerPulse, Radius](int CID, vec2 Pos, EntGroupWeakPtr* pSkillTracker)
		{
			GS()->EntityManager()->HealingRift(CID, Pos, 120.f, Radius, Lifetime, 1.5f, 2, HealPerPulse, 10, 6, pSkillTracker);
		};

		// start casting
		GS()->EntityManager()->StartUniversalCast(ClientID, PlayerPosition, NumCastClicked, FuncExecuteHealingRift, &m_pEntSkillPtrs);
		GS()->CreateSound(PlayerPosition, SOUND_SFX_SKILL);
		return true;
	}

	return false;
}

bool CSkill::IsActivated(CCharacter* pChar, int Manacost, int SkillID, int SkillUsage) const
{
	if(m_ID != SkillID)
		return false;

	// reset skill when use
	if(SkillUsage == SKILL_USAGE_RESET)
	{
		if(!pChar->TryUseMana(Manacost))
			return false;

		if(const auto groupPtr = m_pEntSkillPtrs.lock())
			groupPtr->Clear();

		return true;
	}

	// toggle skill when use
	if(SkillUsage == SKILL_USAGE_TOGGLE)
	{
		if(const auto groupPtr = m_pEntSkillPtrs.lock())
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
	if(m_Learned)
		return false;

	// try spend skill points
	if(!pPlayer->Account()->SpendCurrency(Info()->GetPriceSP(), itSkillPoint))
		return false;

	// update state learned
	m_Learned = true;
	m_SelectedEmoticon = -1;
	Database->Execute<DB::INSERT>("tw_accounts_skills", "(SkillID, UserID, Level) VALUES ('{}', '{}', '1');", m_ID, pPlayer->Account()->GetID());
	GS()->Chat(ClientID, "Learned a new skill ['{}']", Info()->GetName());
	return true;
}
