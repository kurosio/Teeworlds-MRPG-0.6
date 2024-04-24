/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SkillData.h"

#include <game/server/gamecontext.h>

#include "Entities/HealthTurret/healer-health.h"
#include "Entities/AttackTeleport/attack-teleport.h"
#include "Entities/HealthTurret/hearth.h"
#include "Entities/SleepyGravity/sleepy-gravity.h"
#include "game/server/entities/botai/character_bot_ai.h"

CGS* CSkill::GS() const
{
	return (CGS*)Server()->GameServerPlayer(m_ClientID);
}

CPlayer* CSkill::GetPlayer() const
{
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		return GS()->m_apPlayers[m_ClientID];
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

	// mana check
	CCharacter* pChr = GetPlayer()->GetCharacter();
	const int ManaCost = maximum(1, translate_to_percent_rest(GetPlayer()->GetStartMana(), Info()->GetPercentageCost()));
	if(ManaCost > 0 && pChr->CheckFailMana(ManaCost))
		return false;

	const vec2 PlayerPosition = pChr->GetPos();
	const int ClientID = GetPlayer()->GetCID();
	if(m_ID == Skill::SkillHeartTurret)
	{
		// check and reset
		for(CHealthHealer* pHh = (CHealthHealer*)GS()->m_World.FindFirst(CGameWorld::ENTYPE_SKILLTURRETHEART); pHh; pHh = (CHealthHealer*)pHh->TypeNext())
		{
			if(pHh->m_pPlayer->GetCID() != ClientID)
				continue;

			pHh->Destroy();
			break;
		}

		// create healt turret
		const int PowerLevel = ManaCost;
		new CHealthHealer(&GS()->m_World, GetPlayer(), GetBonus(), PowerLevel, PlayerPosition);
		return true;
	}

	if(m_ID == Skill::SkillSleepyGravity)
	{
		// check and reset
		for(CSleepyGravity* pHh = (CSleepyGravity*)GS()->m_World.FindFirst(CGameWorld::ENTYPE_SLEEPYGRAVITY); pHh; pHh = (CSleepyGravity*)pHh->TypeNext())
		{
			if(pHh->m_pPlayer->GetCID() != ClientID)
				continue;

			pHh->Destroy();
			break;
		}

		// create sleepy
		const int PowerLevel = ManaCost;
		new CSleepyGravity(&GS()->m_World, GetPlayer(), GetBonus(), PowerLevel, PlayerPosition);
		return true;
	}

	if(m_ID == Skill::SkillAttackTeleport)
	{
		new CAttackTeleport(&GS()->m_World, PlayerPosition, pChr, GetBonus());
		return true;
	}

	if(m_ID == Skill::SkillCureI)
	{
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
			new CHearth(&GS()->m_World, PlayerPosition, pPlayer, PowerLevel, pPlayer->GetCharacter()->m_Core.m_Vel, true);
			GS()->CreateDeath(pPlayer->GetCharacter()->GetPos(), i);
		}

		GS()->CreateSound(PlayerPosition, SOUND_CTF_GRAB_PL);
	}

	if(m_ID == Skill::SkillBlessingGodWar)
	{
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

		GS()->CreateText(NULL, false, vec2(PlayerPosition.x, PlayerPosition.y - 96.0f), vec2(0, 0), 40, "RECOVERY AMMO");
		return true;
	}

	if(m_ID == Skill::SkillProvoke)
	{
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
			GS()->CreateFlyingPoint(PlayerPosition, pPlayer->GetCharacter()->m_Core.m_Vel, i);
		}

		// some effects
		GS()->CreateSound(PlayerPosition, SOUND_NINJA_FIRE);
		GS()->CreateText(NULL, false, vec2(PlayerPosition.x, PlayerPosition.y - 96.0f), vec2(0, 0), 40, "PROVOKE");
		if(MissedProvoked)
			GS()->Chat(ClientID, "Some were not provoked due to a stronger provocation.");
	}

	return false;
}

bool CSkill::Upgrade()
{
	if(!GetPlayer() || !GetPlayer()->IsAuthed() || m_Level >= Info()->GetMaxLevel())
		return false;

	if(!GetPlayer()->Account()->SpendCurrency(Info()->GetPriceSP(), itSkillPoint))
		return false;

	const int ClientID = GetPlayer()->GetCID();
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