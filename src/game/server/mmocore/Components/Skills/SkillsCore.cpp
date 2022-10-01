/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SkillsCore.h"

#include <game/server/gamecontext.h>

void CSkillsCore::OnInit()
{
	auto InitSkills = Sqlpool.Prepare<DB::SELECT>("*", "tw_skills_list");
	InitSkills->AtExecute([](IServer*, ResultPtr pRes)
	{
		while (pRes->next())
		{
			CSkillDataInfo SkillInfo;
			str_copy(SkillInfo.m_aName, pRes->getString("Name").c_str(), sizeof(SkillInfo.m_aName));
			str_copy(SkillInfo.m_aDesc, pRes->getString("Description").c_str(), sizeof(SkillInfo.m_aDesc));
			str_copy(SkillInfo.m_aBonusName, pRes->getString("BonusName").c_str(), sizeof(SkillInfo.m_aBonusName));
			SkillInfo.m_BonusDefault = pRes->getInt("BonusValue");
			SkillInfo.m_ManaPercentageCost = pRes->getInt("ManaPercentageCost");
			SkillInfo.m_PriceSP = pRes->getInt("PriceSP");
			SkillInfo.m_MaxLevel = pRes->getInt("MaxLevel");
			SkillInfo.m_Passive = pRes->getBoolean("Passive");
			SkillInfo.m_Type = pRes->getInt("Type");
			
			const int SkillID = pRes->getInt("ID");
			CSkillDataInfo::ms_aSkillsData[SkillID] = SkillInfo;
		}
	});
}

void CSkillsCore::OnInitAccount(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		CSkillData Skill;
		Skill.SetSkillOwner(pPlayer);
		Skill.m_SkillID = pRes->getInt("SkillID");
		Skill.m_Level = pRes->getInt("Level");
		Skill.m_SelectedEmoticion = pRes->getInt("UsedByEmoticon");

		const int SkillID = pRes->getInt("SkillID");
		CSkillData::ms_aSkills[ClientID][SkillID] = Skill;
	}
}

void CSkillsCore::OnResetClient(int ClientID)
{
	CSkillData::ms_aSkills.erase(ClientID);
}

bool CSkillsCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	if (ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if (!pChr || !pChr->IsAlive())
			return false;

		if (pChr->GetHelper()->BoolIndex(TILE_LEARN_SKILL))
		{
			const int ClientID = pPlayer->GetCID();
			GS()->AVH(ClientID, TAB_INFO_SKILL, "Skill Learn Information");
			GS()->AVM(ClientID, "null", NOPE, TAB_INFO_SKILL, "Here you can learn passive and active skills");
			GS()->AVM(ClientID, "null", NOPE, TAB_INFO_SKILL, "You can bind active skill any button using the console");
			GS()->AV(ClientID, "null");
			GS()->ShowVotesItemValueInformation(pPlayer, itSkillPoint);
			GS()->AV(ClientID, "null");

			ShowMailSkillList(pPlayer, SKILL_TYPE_TANK);
			ShowMailSkillList(pPlayer, SKILL_TYPE_DPS);
			ShowMailSkillList(pPlayer, SKILL_TYPE_HEALER);
			ShowMailSkillList(pPlayer, SKILL_TYPE_IMPROVEMENTS);
			return true;
		}
		return false;
	}
	return false;
}

bool CSkillsCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();
	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_LEARN_SKILL))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_LEARN_SKILL))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = false;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	return false;
}

bool CSkillsCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if (PPSTR(CMD, "SKILLLEARN") == 0)
	{
		const int SkillID = VoteID;
		if (pPlayer->GetSkill(SkillID)->Upgrade())
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if (PPSTR(CMD, "SKILLCHANGEEMOTICION") == 0)
	{
		const int SkillID = VoteID;
		pPlayer->GetSkill(SkillID)->SelectNextControlEmote();
		GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	return false;
}

void CSkillsCore::ShowMailSkillList(CPlayer *pPlayer, int Type)
{
	const int ClientID = pPlayer->GetCID();
	const char* pSkillTypeName[NUM_SKILL_TYPES] = { "Improving", "Healing", "Attacking", "Defensive" };
	GS()->AVL(ClientID, "null", "{STR} skill's", pSkillTypeName[Type]);
	for (const auto& pSkillData : CSkillDataInfo::ms_aSkillsData)
	{
		if(pSkillData.second.m_Type == Type)
			SkillSelected(pPlayer, pSkillData.first);
	}
	GS()->AV(ClientID, "null");
}

void CSkillsCore::SkillSelected(CPlayer *pPlayer, int SkillID)
{
	CSkillData* pSkill = pPlayer->GetSkill(SkillID);
	const int ClientID = pPlayer->GetCID();
	const int HideID = NUM_TAB_MENU + SkillID;
	const bool IsPassive = pSkill->Info()->IsPassive();
	const bool IsMaxLevel = pSkill->m_Level >= pSkill->Info()->GetMaxLevel();
	
	GS()->AVH(ClientID, HideID, "{STR} - {INT}SP ({INT}/{INT})", pSkill->Info()->GetName(), pSkill->Info()->m_PriceSP, pSkill->GetLevel(), pSkill->Info()->m_MaxLevel);
	if(!IsMaxLevel)
	{
		const int NewBonus = pSkill->GetBonus() + pSkill->Info()->GetBonusDefault();
		GS()->AVM(ClientID, "null", NOPE, HideID, "Next level {INT} {STR}", NewBonus, pSkill->Info()->GetBonusName());
	}
	else
	{
		const int ActiveBonus = pSkill->GetBonus();
		GS()->AVM(ClientID, "null", NOPE, HideID, "Max level {INT} {STR}", ActiveBonus, pSkill->Info()->GetBonusName());
	}
	GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pSkill->Info()->GetDesc());

	if(!IsPassive)
	{
		GS()->AVM(ClientID, "null", NOPE, HideID, "Mana required {INT}%", pSkill->Info()->GetManaPercentageCost());
		if(pSkill->IsLearned())
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "F1 Bind: (bind 'key' say \"/useskill {INT}\")", SkillID);
			GS()->AVM(ClientID, "SKILLCHANGEEMOTICION", SkillID, HideID, "Used on {STR}", pSkill->GetControlEmoteStateName());
		}
	}

	if(!IsMaxLevel)
		GS()->AVM(ClientID, "SKILLLEARN", SkillID, HideID, "Learn {STR}", pSkill->Info()->GetName());
}

void CSkillsCore::ParseEmoticionSkill(CPlayer *pPlayer, int EmoticionID)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayer->GetCharacter())
		return;

	const int ClientID = pPlayer->GetCID();
	for (auto& pSkillPlayer : CSkillData::ms_aSkills[ClientID])
	{
		CSkillData* pSkill = pPlayer->GetSkill(pSkillPlayer.first);
		if (pSkill->m_SelectedEmoticion == EmoticionID)
			pSkill->Use();
	}
}
