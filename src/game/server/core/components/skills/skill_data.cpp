/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "skill_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/entities/character_bot.h>
#include <game/server/core/entities/group/entitiy_group.h>

#include "entities/attack_teleport/attack_teleport.h"
#include "entities/heart_healer.h"

const char* CSkillDescription::GetEmoticonName(int EmoticionID)
{
	switch(EmoticionID)
	{
		case EMOTICON_OOP: return "Emoticion Ooop";
		case EMOTICON_EXCLAMATION: return "Emoticion Exclamation";
		case EMOTICON_HEARTS: return "Emoticion Hearts";
		case EMOTICON_DROP: return "Emoticion Drop";
		case EMOTICON_DOTDOT: return "Emoticion ...";
		case EMOTICON_MUSIC: return "Emoticion Music";
		case EMOTICON_SORRY: return "Emoticion Sorry";
		case EMOTICON_GHOST: return "Emoticion Ghost";
		case EMOTICON_SUSHI: return "Emoticion Sushi";
		case EMOTICON_SPLATTEE: return "Emoticion Splatee";
		case EMOTICON_DEVILTEE: return "Emoticion Deviltee";
		case EMOTICON_ZOMG: return "Emoticion Zomg";
		case EMOTICON_ZZZ: return "Emoticion Zzz";
		case EMOTICON_WTF: return "Emoticion Wtf";
		case EMOTICON_EYES: return "Emoticion Eyes";
		case EMOTICON_QUESTION: return "Emoticion Question";
		default: return "Not selected";
	}
}

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

	m_SelectedEmoticion = (m_SelectedEmoticion + 1) % (NUM_EMOTICONS + 1) - 1;
	Database->Execute<DB::UPDATE>("tw_accounts_skills", "UsedByEmoticon = '{}' WHERE SkillID = '{}' AND UserID = '{}'", 
		m_SelectedEmoticion, m_ID, pPlayer->Account()->GetID());
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

	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int ManaCost = maximum(1, translate_to_percent_rest(pPlayer->GetMaxMana(), Info()->GetPercentageCost()));
	const vec2 PlayerPosition = pChar->GetPos();

	if(m_ID == SkillAttackTeleport)
	{
		// check mana
		if(pChar->CheckFailMana(ManaCost))
			return false;

		// create attack teleport
		new CAttackTeleport(&GS()->m_World, PlayerPosition, pPlayer, GetBonus());
		return true;
	}

	if(m_ID == SkillCureI)
	{
		// check mana
		if(pChar->CheckFailMana(ManaCost))
			return false;

		// cure near players
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			// check player
			auto* pSearchPl = GS()->GetPlayer(i, true, true);
			if(!pSearchPl || !GS()->IsPlayerInWorld(i))
				continue;

			// check distance
			if(distance(PlayerPosition, pSearchPl->GetCharacter()->GetPos()) > 800)
				continue;

			// dissalow heal for pvp clients
			if(pSearchPl->GetCharacter()->IsAllowedPVP(ClientID) && i != ClientID)
				continue;

			// create healt
			const int PowerLevel = maximum(ManaCost + translate_to_percent_rest(ManaCost, minimum(GetBonus(), 100)), 1);
			new CHeartHealer(&GS()->m_World, PlayerPosition, pSearchPl, PowerLevel, pSearchPl->GetCharacter()->m_Core.m_Vel, true);
			GS()->CreateDeath(pSearchPl->GetCharacter()->GetPos(), i);
		}

		GS()->CreateSound(PlayerPosition, SOUND_CTF_GRAB_PL);
		return true;
	}

	if(m_ID == SkillBlessingGodWar)
	{
		// check mana
		if(pChar->CheckFailMana(ManaCost))
			return false;

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

		GS()->EntityManager()->Text(PlayerPosition + vec2(0, -96), 40, "RECOVERY AMMO");
		return true;
	}

	if(m_ID == SkillProvoke)
	{
		// check mana
		if(pChar->CheckFailMana(ManaCost))
			return false;

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
			if(const auto* pTargetPl = GS()->GetPlayer(pSearchChar->AI()->GetTarget()->GetCID(), false, true))
			{
				if(pTargetPl->GetMaxHealth() > pPlayer->GetMaxHealth())
				{
					MissedProvoked = true;
					continue;
				}
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

	if(m_ID == SkillSleepyGravity)
	{
		// check mana
		//if(pChr->CheckFailMana(ManaCost))
		//	return false;

		if(auto groupPtr = m_pEntitySkill.lock())
			groupPtr->Clear();

		int Damage = 15;
		int Lifetime = 1000;
		float Speed = 2.0f;
		float Radius = 300.0f;
		//GS()->EntityManager()->Tornado(ClientID, PlayerPosition, Damage, Lifetime, Speed, Radius);


		//GS()->EntityManager()->HealingAura(ClientID, PlayerPosition, 320.f, 1000, 10);

		//GS()->EntityManager()->GravityDisruption(ClientID, PlayerPosition, minimum(200.f + GetBonus(), 400.f), 10 * Server()->TickSpeed(), ManaCost, &m_pEntitySkill);
		return true;
	}

	// Skill heart turret
	if(m_ID == SkillHeartTurret)
	{
		// check mana
		//if(pChr->CheckFailMana(ManaCost))
		//	return false;

		if(auto groupPtr = m_pEntitySkill.lock())
			groupPtr->Clear();

		//GS()->EntityManager()->FlameWall(ClientID, PlayerPosition, 200.f, 1000, 1, 0.3f);
		//GS()->EntityManager()->FrostNova(ClientID, PlayerPosition, 120.f, 12, 5);
		GS()->EntityManager()->HealingAura(ClientID, PlayerPosition, 320.f, 1000, 10);
		GS()->EntityManager()->Bow(ClientID, 5, 25, 160.f, 16);

		GS()->EntityManager()->HealthTurret(ClientID, PlayerPosition, ManaCost, (10 + GetBonus()) * Server()->TickSpeed(), 2 * Server()->TickSpeed(), &m_pEntitySkill);
		return true;
	}

	if(m_ID == SkillEnergyShield)
	{
		// disable energy shield
		if(const auto groupPtr = m_pEntitySkill.lock())
		{
			GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "The energy shield has been disabled!");
			groupPtr->Clear();
			return true;
		}

		// check mana
		//if(pChr->CheckFailMana(ManaCost))
		//	return false;

		GS()->EntityManager()->FlameWall(ClientID, PlayerPosition, 200.f, 1000, 1, 0.3f);

		// enable shield
		const int StartHealth = maximum(1, translate_to_percent_rest(pPlayer->GetMaxHealth(), GetBonus()));
		GS()->EntityManager()->EnergyShield(ClientID, PlayerPosition, StartHealth, &m_pEntitySkill);
		GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "The energy shield has been enabled! Health: {}!", StartHealth);
		return true;
	}

	return false;
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
		GS()->Chat(ClientID, "Increased the skill [{} level to {}]", Info()->GetName(), m_Level);
	}
	else
	{
		m_Level = 1;
		m_SelectedEmoticion = -1;
		Database->Execute<DB::INSERT>("tw_accounts_skills", "(SkillID, UserID, Level) VALUES ('{}', '{}', '1');", m_ID, pPlayer->Account()->GetID());
		GS()->Chat(ClientID, "Learned a new skill [{}]", Info()->GetName());
	}

	return true;
}