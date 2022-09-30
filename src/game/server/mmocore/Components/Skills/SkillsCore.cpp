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
			const int SkillID = pRes->getInt("ID");
			str_copy(CSkillDataInfo::ms_aSkillsData[SkillID].m_aName, pRes->getString("Name").c_str(), sizeof(CSkillDataInfo::ms_aSkillsData[SkillID].m_aName));
			str_copy(CSkillDataInfo::ms_aSkillsData[SkillID].m_aDesc, pRes->getString("Description").c_str(), sizeof(CSkillDataInfo::ms_aSkillsData[SkillID].m_aDesc));
			str_copy(CSkillDataInfo::ms_aSkillsData[SkillID].m_aBonusName, pRes->getString("BonusName").c_str(), sizeof(CSkillDataInfo::ms_aSkillsData[SkillID].m_aBonusName));
			CSkillDataInfo::ms_aSkillsData[SkillID].m_BonusValue = pRes->getInt("BonusValue");
			CSkillDataInfo::ms_aSkillsData[SkillID].m_ManaPercentageCost = pRes->getInt("ManaPercentageCost");
			CSkillDataInfo::ms_aSkillsData[SkillID].m_PriceSP = pRes->getInt("PriceSP");
			CSkillDataInfo::ms_aSkillsData[SkillID].m_MaxLevel = pRes->getInt("MaxLevel");
			CSkillDataInfo::ms_aSkillsData[SkillID].m_Passive = pRes->getBoolean("Passive");
			CSkillDataInfo::ms_aSkillsData[SkillID].m_Type = pRes->getInt("Type");
		}
	});
}

void CSkillsCore::OnInitAccount(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		const int SkillID = pRes->getInt("SkillID");
		CSkillData::ms_aSkills[ClientID][SkillID].SetSkillOwner(pPlayer);
		CSkillData::ms_aSkills[ClientID][SkillID].m_SkillID = SkillID;
		CSkillData::ms_aSkills[ClientID][SkillID].m_Level = pRes->getInt("Level");
		CSkillData::ms_aSkills[ClientID][SkillID].m_SelectedEmoticion = pRes->getInt("UsedByEmoticon");
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
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_LEARN_SKILL))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = false;
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
		if (pPlayer->GetSkill(SkillID).Upgrade())
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if (PPSTR(CMD, "SKILLCHANGEEMOTICION") == 0)
	{
		const int SkillID = VoteID;
		pPlayer->GetSkill(SkillID).SelectNextControlEmote();
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
	CSkillData& pSkill = pPlayer->GetSkill(SkillID);
	const int ClientID = pPlayer->GetCID();
	const int HideID = NUM_TAB_MENU + SkillID;
	const bool IsPassive = pSkill.Info().m_Passive;
	const bool IsMaxLevel = pSkill.m_Level >= pSkill.Info().m_MaxLevel;
	
	GS()->AVH(ClientID, HideID, "{STR} - {INT}SP ({INT}/{INT})", pSkill.Info().m_aName, pSkill.Info().m_PriceSP, pSkill.m_Level, pSkill.Info().m_MaxLevel);
	if(!IsMaxLevel)
	{
		const int NewBonus = pSkill.GetBonus() + pSkill.Info().m_BonusValue;
		GS()->AVM(ClientID, "null", NOPE, HideID, "Next level {INT} {STR}", NewBonus, pSkill.Info().m_aBonusName);
	}
	else
	{
		const int ActiveBonus = pSkill.GetBonus();
		GS()->AVM(ClientID, "null", NOPE, HideID, "Max level {INT} {STR}", ActiveBonus, pSkill.Info().m_aBonusName);
	}
	GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pSkill.Info().m_aDesc);

	if(!IsPassive)
	{
		GS()->AVM(ClientID, "null", NOPE, HideID, "Mana required {INT}%", pSkill.Info().m_ManaPercentageCost);
		if(pSkill.IsLearned())
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "F1 Bind: (bind 'key' say \"/useskill {INT}\")", SkillID);
			GS()->AVM(ClientID, "SKILLCHANGEEMOTICION", SkillID, HideID, "Used on {STR}", pSkill.GetControlEmoteStateName());
		}
	}

	if(!IsMaxLevel)
		GS()->AVM(ClientID, "SKILLLEARN", SkillID, HideID, "Learn {STR}", pSkill.Info().m_aName);
}

void CSkillsCore::ParseEmoticionSkill(CPlayer *pPlayer, int EmoticionID)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayer->GetCharacter())
		return;

	const int ClientID = pPlayer->GetCID();
	for (auto& pSkillPlayer : CSkillData::ms_aSkills[ClientID])
	{
		CSkillData& pSkill = pPlayer->GetSkill(pSkillPlayer.first);
		if (pSkill.m_SelectedEmoticion == EmoticionID)
			pSkill.Use();
	}
}
