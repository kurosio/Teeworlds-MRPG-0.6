/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "skill_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/entities/botai/character_bot_ai.h>
#include <game/server/core/entities/event/entitiy_group.h>

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
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		return GS()->GetPlayer(m_ClientID);
	}
	return nullptr;
}

std::string CSkill::GetStringLevelStatus() const
{
	if(!IsLearned())
		return "(not learned)";
	if(m_Level < Info()->GetMaxLevel())
		return "(" + std::to_string(m_Level) + " of " + std::to_string(Info()->GetMaxLevel()) + ")";
	return "(max)";
}

void CSkill::SelectNextControlEmote()
{
	if(!GetPlayer() || !GetPlayer()->IsAuthed())
		return;

	m_SelectedEmoticion++;
	if(m_SelectedEmoticion >= NUM_EMOTICONS)
		m_SelectedEmoticion = -1;

	Database->Execute<DB::UPDATE>("tw_accounts_skills", "UsedByEmoticon = '%d' WHERE SkillID = '%d' AND UserID = '%d'", m_SelectedEmoticion, m_ID, GetPlayer()->Account()->GetID());
}

bool CSkill::Use()
{
	if(!GetPlayer() || !GetPlayer()->IsAuthed() || !GetPlayer()->GetCharacter() || m_Level <= 0)
		return false;

	// initialize variables
	const int ClientID = GetPlayer()->GetCID();
	const int ManaCost = maximum(1, translate_to_percent_rest(GetPlayer()->GetStartMana(), Info()->GetPercentageCost()));
	CCharacter* pChr = GetPlayer()->GetCharacter();
	const vec2 PlayerPosition = pChr->GetPos();

	if(m_ID == SkillAttackTeleport)
	{
		// check mana
		if(pChr->CheckFailMana(ManaCost))
			return false;

		// create attack teleport
		new CAttackTeleport(&GS()->m_World, PlayerPosition, GetPlayer(), GetBonus());
		return true;
	}

	if(m_ID == SkillCureI)
	{
		// check mana
		if(pChr->CheckFailMana(ManaCost))
			return false;

		// cure near players
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			// check player
			CPlayer* pPlayer = GS()->GetPlayer(i, true, true);
			if(!pPlayer || !GS()->IsPlayerEqualWorld(i))
				continue;

			// check distance
			if(distance(PlayerPosition, pPlayer->GetCharacter()->GetPos()) > 800)
				continue;

			// dissalow heal for pvp clients
			if(pPlayer->GetCharacter()->IsAllowedPVP(ClientID) && i != ClientID)
				continue;

			// create healt
			const int PowerLevel = maximum(ManaCost + translate_to_percent_rest(ManaCost, minimum(GetBonus(), 100)), 1);
			new CHeartHealer(&GS()->m_World, PlayerPosition, pPlayer, PowerLevel, pPlayer->GetCharacter()->m_Core.m_Vel, true);
			GS()->CreateDeath(pPlayer->GetCharacter()->GetPos(), i);
		}

		GS()->CreateSound(PlayerPosition, SOUND_CTF_GRAB_PL);
		return true;
	}

	if(m_ID == SkillBlessingGodWar)
	{
		// check mana
		if(pChr->CheckFailMana(ManaCost))
			return false;

		// blessing near players
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPlayer = GS()->GetPlayer(i, true, true);
			if(!pPlayer || !GS()->IsPlayerEqualWorld(i) || distance(PlayerPosition, pPlayer->GetCharacter()->GetPos()) > 800
				|| (pPlayer->GetCharacter()->IsAllowedPVP(ClientID) && i != ClientID))
				continue;

			const int RealAmmo = 10 + pPlayer->GetAttributeSize(AttributeIdentifier::Ammo);
			const int RestoreAmmo = translate_to_percent_rest(RealAmmo, minimum(GetBonus(), 100));
			for(int j = WEAPON_GUN; j <= WEAPON_LASER; j++)
			{
				pPlayer->GetCharacter()->GiveWeapon(j, RestoreAmmo);
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
		if(pChr->CheckFailMana(ManaCost))
			return false;

		// provoke mobs
		bool MissedProvoked = false;
		for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
		{
			// check player
			CPlayerBot* pPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i, false, true));
			if(!pPlayer || !GS()->IsPlayerEqualWorld(i))
				continue;

			// check distance
			if(distance(PlayerPosition, pPlayer->GetCharacter()->GetPos()) > 800)
				continue;

			// check allowed pvp
			if(!pPlayer->GetCharacter()->IsAllowedPVP(ClientID))
				continue;

			// check target upper agression
			CCharacterBotAI* pCharacterBotAI = dynamic_cast<CCharacterBotAI*>(pPlayer->GetCharacter());
			if(CPlayer* pPlayerAgr = GS()->GetPlayer(pCharacterBotAI->AI()->GetTarget()->GetCID(), false, true))
			{
				if(pPlayerAgr->GetStartHealth() > GetPlayer()->GetStartHealth())
				{
					MissedProvoked = true;
					continue;
				}
			}

			// set agression
			pCharacterBotAI->AI()->GetTarget()->Set(ClientID, GetBonus());
			GS()->CreatePlayerSpawn(pPlayer->GetCharacter()->GetPos());
			pPlayer->GetCharacter()->SetEmote(EMOTE_ANGRY, 10, true);
			GS()->EntityManager()->FlyingPoint(PlayerPosition, i, pPlayer->GetCharacter()->m_Core.m_Vel);
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
		if(pChr->CheckFailMana(ManaCost))
			return false;

		if(auto groupPtr = m_pEntitySkill.lock())
			groupPtr->Clear();

		GS()->EntityManager()->GravityDisruption(ClientID, PlayerPosition, minimum(200.f + GetBonus(), 400.f), 10 * Server()->TickSpeed(), ManaCost, &m_pEntitySkill);
		return true;
	}

	// Skill heart turret
	if(m_ID == SkillHeartTurret)
	{
		// check mana
		if(pChr->CheckFailMana(ManaCost))
			return false;

		if(auto groupPtr = m_pEntitySkill.lock())
			groupPtr->Clear();

		GS()->EntityManager()->HealthTurret(ClientID, PlayerPosition, ManaCost, (10 + GetBonus()) * Server()->TickSpeed(), 2 * Server()->TickSpeed(), &m_pEntitySkill);
		return true;
	}

	if(m_ID == SkillEnergyShield)
	{
		// disable energy shield
		if(auto groupPtr = m_pEntitySkill.lock())
		{
			GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "The energy shield has been disabled!");
			groupPtr->Clear();
			return true;
		}

		// check mana
		if(pChr->CheckFailMana(ManaCost))
			return false;

		// enable shield
		const int StartHealth = maximum(1, translate_to_percent_rest(GetPlayer()->GetStartHealth(), GetBonus()));
		GS()->EntityManager()->EnergyShield(ClientID, PlayerPosition, StartHealth, &m_pEntitySkill);
		GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "The energy shield has been enabled! Health: {}!", StartHealth);
		return true;
	}

	return false;
}

bool CSkill::Upgrade()
{
	// check player exists
	if(!GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	// check for maximal leveling
	const int ClientID = GetPlayer()->GetCID();
	if(m_Level >= Info()->GetMaxLevel())
	{
		GS()->Chat(ClientID, "You've already reached the maximum level");
		return false;
	}

	// try spend skill points
	if(!GetPlayer()->Account()->SpendCurrency(Info()->GetPriceSP(), itSkillPoint))
		return false;

	// update level
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE SkillID = '%d' AND UserID = '%d'", m_ID, GetPlayer()->Account()->GetID());
	if(pRes->next())
	{
		m_Level++;
		Database->Execute<DB::UPDATE>("tw_accounts_skills", "Level = '%d' WHERE SkillID = '%d' AND UserID = '%d'", m_Level, m_ID, GetPlayer()->Account()->GetID());
		GS()->Chat(ClientID, "Increased the skill [{} level to {}]", Info()->GetName(), m_Level);
	}
	else
	{
		m_Level = 1;
		m_SelectedEmoticion = -1;
		Database->Execute<DB::INSERT>("tw_accounts_skills", "(SkillID, UserID, Level) VALUES ('%d', '%d', '1');", m_ID, GetPlayer()->Account()->GetID());
		GS()->Chat(ClientID, "Learned a new skill [{}]", Info()->GetName());
	}

	return true;
}