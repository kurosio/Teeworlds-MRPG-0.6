/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SkillManager.h"

#include <game/server/gamecontext.h>

void CSkillManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_skills_list");
	while (pRes->next())
	{
		std::string Name = pRes->getString("Name").c_str();
		std::string Description = pRes->getString("Description").c_str();
		std::string BoostName = pRes->getString("BoostName").c_str();
		int BoostValue = pRes->getInt("BoostValue");
		int PercentageCost = pRes->getInt("PercentageCost");
		int PriceSP = pRes->getInt("PriceSP");
		int MaxLevel = pRes->getInt("MaxLevel");
		bool Passive = pRes->getBoolean("Passive");
		SkillType Type = (SkillType)pRes->getInt("Type");

		SkillIdentifier ID = pRes->getInt("ID");
		CSkillDescription(ID).Init(Name, Description, BoostName, BoostValue, Type, PercentageCost, PriceSP, MaxLevel, Passive);
	}
}

void CSkillManager::OnInitAccount(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '%d'", pPlayer->Acc()->GetID());
	while(pRes->next())
	{
		int Level = pRes->getInt("Level");
		int SelectedEmoticion = pRes->getInt("UsedByEmoticon");

		SkillIdentifier ID = pRes->getInt("SkillID");
		CSkill(ID, ClientID).Init(Level, SelectedEmoticion);
	}
}

void CSkillManager::OnResetClient(int ClientID)
{
	CSkill::Data().erase(ClientID);
}

bool CSkillManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
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
			GS()->AddVoteItemValue(ClientID, itSkillPoint);
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

bool CSkillManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_LEARN_SKILL))
	{
		_DEF_TILE_ENTER_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_LEARN_SKILL))
	{
		_DEF_TILE_EXIT_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}
	return false;
}

bool CSkillManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if (PPSTR(CMD, "SKILLLEARN") == 0)
	{
		const int SkillID = VoteID;
		if (pPlayer->GetSkill(SkillID)->Upgrade())
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}

	if (PPSTR(CMD, "SKILLCHANGEEMOTICION") == 0)
	{
		const int SkillID = VoteID;
		pPlayer->GetSkill(SkillID)->SelectNextControlEmote();
		GS()->StrongUpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}
	return false;
}

void CSkillManager::ShowMailSkillList(CPlayer *pPlayer, SkillType Type) const
{
	const int ClientID = pPlayer->GetCID();
	const char* pSkillTypeName[NUM_SKILL_TYPES] = { "Improving", "Healing", "Attacking", "Defensive" };

	GS()->AVL(ClientID, "null", "{STR} skill's", pSkillTypeName[Type]);
	for (const auto& [ID, Skill] : CSkillDescription::Data())
	{
		if(Skill.m_Type == Type)
			ShowSkill(pPlayer, ID);
	}
	GS()->AV(ClientID, "null");
}

void CSkillManager::ShowSkill(CPlayer *pPlayer, SkillIdentifier ID) const
{
	const int ClientID = pPlayer->GetCID();
	CSkill* pSkill = pPlayer->GetSkill(ID);

	const int HideID = NUM_TAB_MENU + ID;
	const bool IsPassive = pSkill->Info()->IsPassive();
	const bool IsMaximumLevel = pSkill->GetLevel() >= pSkill->Info()->GetMaxLevel();

	GS()->AVH(ClientID, HideID, "{STR} - {INT}SP ({INT}/{INT})", pSkill->Info()->GetName(), pSkill->Info()->GetPriceSP(), pSkill->GetLevel(), pSkill->Info()->GetMaxLevel());
	GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pSkill->Info()->GetDescription());
	if(!IsMaximumLevel)
	{
		const int NewBonus = pSkill->GetBonus() + pSkill->Info()->GetBoostDefault();
		GS()->AVM(ClientID, "null", NOPE, HideID, "Next level {INT} {STR}", NewBonus, pSkill->Info()->GetBoostName());
	}
	else
	{
		const int ActiveBonus = pSkill->GetBonus();
		GS()->AVM(ClientID, "null", NOPE, HideID, "Max level {INT} {STR}", ActiveBonus, pSkill->Info()->GetBoostName());
	}

	if(!IsPassive)
	{
		GS()->AVM(ClientID, "null", NOPE, HideID, "Mana required {INT}%", pSkill->Info()->GetPercentageCost());
		if(pSkill->IsLearned())
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "F1 Bind: (bind 'key' say \"/useskill {INT}\")", ID);
			GS()->AVM(ClientID, "SKILLCHANGEEMOTICION", ID, HideID, "Used on {STR}", pSkill->GetSelectedEmoticonName());
		}
	}

	if(!IsMaximumLevel)
	{
		GS()->AVM(ClientID, "SKILLLEARN", ID, HideID, "Learn {STR}", pSkill->Info()->GetName());
	}
	GS()->AVM(ClientID, "null", NOPE, HideID, "\0");
}

void CSkillManager::ParseEmoticionSkill(CPlayer *pPlayer, int EmoticionID)
{
	if(pPlayer && pPlayer->IsAuthed() && pPlayer->GetCharacter())
	{
		const int ClientID = pPlayer->GetCID();
		for(auto& [ID, Skill] : CSkill::Data()[ClientID])
		{
			if (Skill.m_SelectedEmoticion == EmoticionID)
				Skill.Use();
		}
	}
}
