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
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
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

bool CSkillManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	// Main menu skill list
	if(Menulist == MENU_SKILL_LIST)
	{
		const int ClientID = pPlayer->GetCID();

		// Information
		CVoteWrapper VInfo(ClientID, VWF_STYLE_STRICT, "Skill master information");
		VInfo.AddLine();
		VInfo.Add("Here you can learn passive and active skills");
		VInfo.Add("You can bind active skill any button using the console");
		VInfo.AddLine();
		VInfo.AddItemValue(itSkillPoint);
		VInfo.AddLine();

		// Skill list by class type
		int Skilltype = SKILL_TYPE_TANK;
		const char* pTypename = "Defensive skill's";
		const ClassGroup& Class = pPlayer->GetClass()->GetGroup();

		if(Class == ClassGroup::Dps)
		{
			Skilltype = SKILL_TYPE_DPS;
			pTypename = "Attacking skill's";
		}
		else if(Class == ClassGroup::Healer)
		{
			Skilltype = SKILL_TYPE_HEALER;
			pTypename = "Healing skill's";
		}

		ShowSkillList(pPlayer, pTypename, (SkillType)Skilltype);
		ShowSkillList(pPlayer, "Improving", SkillType::SKILL_TYPE_IMPROVEMENTS);
		return true;
	}

	// Skill selected detail information
	if(Menulist == MENU_SKILL_SELECTED)
	{
		// Set last menu skill list
		pPlayer->m_VotesData.SetLastMenuID(MENU_SKILL_LIST);

		// Show selected skill
		if(pPlayer->m_VotesData.GetMenuTemporaryInteger() >= 0)
			ShowSkill(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());

		// Add backpage
		CVoteWrapper::AddBackpage(pPlayer->GetCID());
		return true;
	}

	return false;
}

void CSkillManager::ShowSkillList(CPlayer* pPlayer, const char* pTitle, SkillType Type) const
{
	const int ClientID = pPlayer->GetCID();

	// add empty line
	CVoteWrapper::AddEmptyline(ClientID);

	// iterate skill's for list
	CVoteWrapper VSkills(ClientID, VWF_SEPARATE_OPEN, pTitle);
	for(const auto& [ID, Skill] : CSkillDescription::Data())
	{
		if(Skill.m_Type == Type)
		{
			CSkill* pSkill = pPlayer->GetSkill(ID);
			VSkills.AddMenu(MENU_SKILL_SELECTED, ID, "{} - {}SP {}", Skill.GetName(), Skill.GetPriceSP(), pSkill->GetStringLevelStatus().c_str());
		}
	}

	// add line
	CVoteWrapper::AddLine(ClientID);
}

void CSkillManager::ShowSkill(CPlayer* pPlayer, SkillIdentifier ID) const
{
	const int ClientID = pPlayer->GetCID();
	CSkill* pSkill = pPlayer->GetSkill(ID);
	CSkillDescription* pInfo = pSkill->Info();
	const bool IsLearned = pSkill->IsLearned();
	const bool IsPassive = pInfo->IsPassive();
	const bool IsMaximumLevel = pSkill->GetLevel() >= pInfo->GetMaxLevel();

	// skill want learn information
	CVoteWrapper VSkillWant(ClientID, VWF_STYLE_STRICT_BOLD, "Do you want learn?");
	VSkillWant.Add(Instance::Localize(ClientID, pInfo->GetDescription()));
	VSkillWant.AddIf(IsLearned, "{} {} (each level +{})", pSkill->GetBonus(), pInfo->GetBoostName(), pInfo->GetBoostDefault());
	VSkillWant.AddIf(!IsPassive, "Mana required {}%", pInfo->GetPercentageCost());
	VSkillWant.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	// required sp for learn
	CVoteWrapper VRequired(ClientID, VWF_OPEN | VWF_STYLE_STRICT, "Required");
	VRequired.ReinitNumeralDepthStyles(
		{
			{ DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD }
		}
	);
	CPlayerItem* pPlayerItem = pPlayer->GetItem(itSkillPoint);
	bool MarkHas = pPlayerItem->GetValue() >= pInfo->GetPriceSP();
	VRequired.MarkList().Add("{} {}x{} ({})", MarkHas ? "\u2714" : "\u2718", pPlayerItem->Info()->GetName(), pInfo->GetPriceSP(), pPlayerItem->GetValue());
	VRequired.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	// settings and information about usage
	if(!IsPassive && IsLearned)
	{
		CVoteWrapper VUsage(ClientID, VWF_OPEN | VWF_STYLE_STRICT, "Usage");
		VUsage.Add("F1 Bind: (bind 'key' say \"/useskill {}\")", ID);
		VUsage.AddOption("SKILL_CHANGE_USAGE_EMOTICION", ID, "Used on {}", pSkill->GetSelectedEmoticonName());
		VUsage.AddLine();
		CVoteWrapper::AddEmptyline(ClientID);
	}

	// show status and button buy
	if(IsMaximumLevel)
		CVoteWrapper(ClientID).Add("- Already been improved to maximum level");
	else if(!MarkHas)
		CVoteWrapper(ClientID).Add("- Not enough skill point's to learn");
	else
		CVoteWrapper(ClientID).AddOption("SKILL_LEARN", ID, "\u2726 Learn ({} of {} level)", pSkill->GetLevel(), pInfo->GetMaxLevel());

	// add emptyline
	CVoteWrapper::AddEmptyline(ClientID);
}

bool CSkillManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_SKILL_ZONE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_SKILL_LIST);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_SKILL_ZONE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}
	return false;
}

bool CSkillManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// learn skill function
	if (PPSTR(CMD, "SKILL_LEARN") == 0)
	{
		const int SkillID = VoteID;
		if(pPlayer->GetSkill(SkillID)->Upgrade())
			pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// change emoticion function
	if (PPSTR(CMD, "SKILL_CHANGE_USAGE_EMOTICION") == 0)
	{
		const int SkillID = VoteID;
		pPlayer->GetSkill(SkillID)->SelectNextControlEmote();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}
	return false;
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
