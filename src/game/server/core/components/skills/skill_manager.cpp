/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "skill_manager.h"

#include <game/server/gamecontext.h>
#include "skill_data.h"

void CSkillManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_skills_list");
	while (pRes->next())
	{
		const auto ID = pRes->getInt("ID");
		const auto Name = pRes->getString("Name");
		const auto Description = pRes->getString("Description");
		const auto BoostName = pRes->getString("BoostName");
		const auto BoostValue = pRes->getInt("BoostValue");
		const auto PercentageCost = pRes->getInt("PercentageCost");
		const auto PriceSP = pRes->getInt("PriceSP");
		const auto MaxLevel = pRes->getInt("MaxLevel");
		const auto Passive = pRes->getBoolean("Passive");
		const auto ProfID = (ProfessionIdentifier)pRes->getInt("ProfessionID");

		CSkillDescription(ID).Init(Name, Description, BoostName, BoostValue, ProfID, PercentageCost, PriceSP, MaxLevel, Passive);
	}
}

void CSkillManager::OnPlayerLogin(CPlayer *pPlayer)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '{}'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		const auto ID = pRes->getInt("SkillID");
		const auto Level = pRes->getInt("Level");
		const auto SelectedEmoticion = pRes->getInt("UsedByEmoticon");

		// init by server
		CSkill::CreateElement(pPlayer->GetCID(), ID)->Init(Level, SelectedEmoticion);
	}
}

void CSkillManager::OnClientReset(int ClientID)
{
	mystd::freeContainer(CSkill::Data()[ClientID]);
}

bool CSkillManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	// menu skill list
	if(Menulist == MENU_SKILL_LIST)
	{
		const int ClientID = pPlayer->GetCID();

		// Information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE|VWF_STYLE_STRICT, "Skill master information");
		VInfo.AddLine();
		VInfo.Add("Here you can learn passive and active skills");
		VInfo.Add("You can bind active skill any button using the console");
		VInfo.AddLine();
		VInfo.AddItemValue(itSkillPoint);
		VoteWrapper::AddEmptyline(ClientID);

		// Skill list by class type
		const auto ProfID = pPlayer->Account()->GetActiveProfessionID();

		if(ProfID != ProfessionIdentifier::None)
		{
			const auto Title = std::string(GetProfessionName(ProfID)) + " skill's";
			ShowSkillList(pPlayer, Title.c_str(), ProfID);
			VoteWrapper::AddEmptyline(ClientID);
		}

		ShowSkillList(pPlayer, "Improving skill's", ProfessionIdentifier::None);
		return true;
	}

	// Skill selected detail information
	if(Menulist == MENU_SKILL_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SKILL_LIST);

		if(const auto SkillID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowSkill(pPlayer, SkillID.value());
		}

		// Add backpage
		VoteWrapper::AddBackpage(pPlayer->GetCID());
		return true;
	}

	return false;
}

void CSkillManager::ShowSkillList(CPlayer* pPlayer, const char* pTitle, ProfessionIdentifier ProfID) const
{
	const int ClientID = pPlayer->GetCID();

	// iterate skill's for list
	VoteWrapper VSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, pTitle);
	for(const auto& [ID, Skill] : CSkillDescription::Data())
	{
		if(Skill.m_ProfessionID != ProfID)
			continue;

		const auto* pSkill = pPlayer->GetSkill(ID);
		VSkills.AddMenu(MENU_SKILL_SELECT, ID, "{} - {}SP {}", Skill.GetName(), Skill.GetPriceSP(), pSkill->GetStringLevelStatus().c_str());
	}
}

void CSkillManager::ShowSkill(CPlayer* pPlayer, int SkillID) const
{
	const auto ClientID = pPlayer->GetCID();
	const auto* pSkill = pPlayer->GetSkill(SkillID);
	const auto* pInfo = pSkill->Info();
	const auto* pPlayerSkillPoints = pPlayer->GetItem(itSkillPoint);
	const auto IsLearned = pSkill->IsLearned();
	const auto IsPassive = pInfo->IsPassive();
	const auto IsMaximumLevel = pSkill->GetLevel() >= pInfo->GetMaxLevel();
	const auto MarkHas = pPlayerSkillPoints->GetValue() >= pInfo->GetPriceSP();

	// skill want learn information
	VoteWrapper VSkillWant(ClientID, VWF_STYLE_STRICT_BOLD, "Do you want learn?");
	VSkillWant.Add(Instance::Localize(ClientID, pInfo->GetDescription()));
	if(IsLearned)
	{
		VSkillWant.Add("{} {} (each level +{})", pSkill->GetBonus(), pInfo->GetBoostName(), pInfo->GetBoostDefault());
	}
	if(!IsPassive)
	{
		VSkillWant.Add("Mana required {}%", pInfo->GetPercentageCost());
	}
	VSkillWant.AddLine();
	VoteWrapper::AddEmptyline(ClientID);

	// required sp for learn
	VoteWrapper VRequired(ClientID, VWF_OPEN | VWF_STYLE_STRICT, "Required");
	VRequired.ReinitNumeralDepthStyles({{ DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD }});
	VRequired.MarkList().Add("{} {} x{} ({})", MarkHas ? "\u2714" : "\u2718",
		pPlayerSkillPoints->Info()->GetName(), pInfo->GetPriceSP(), pPlayerSkillPoints->GetValue());
	VRequired.AddLine();
	VoteWrapper::AddEmptyline(ClientID);

	// settings and information about usage
	if(!IsPassive && IsLearned)
	{
		VoteWrapper VUsage(ClientID, VWF_OPEN | VWF_STYLE_STRICT, "Usage");
		VUsage.Add("F1 Bind: (bind 'key' say \"/use_skill {}\")", SkillID);
		VUsage.AddOption("SKILL_CHANGE_USAGE_EMOTICION", SkillID, "Used on {}", pSkill->GetSelectedEmoticonName());
		VUsage.AddLine();
		VoteWrapper::AddEmptyline(ClientID);
	}

	// show status and button buy
	if(IsMaximumLevel)
	{
		VoteWrapper(ClientID).Add("- Already been improved to maximum level");
	}
	else if(!MarkHas)
	{
		VoteWrapper(ClientID).Add("- Not enough skill point's to learn");
	}
	else
	{
		VoteWrapper(ClientID).AddOption("SKILL_LEARN", SkillID, "\u2726 Learn ({} of {} level)", pSkill->GetLevel(), pInfo->GetMaxLevel());
	}

	// add emptyline
	VoteWrapper::AddEmptyline(ClientID);
}

void CSkillManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_SKILL_ZONE, MENU_SKILL_LIST, {}, {});
}

bool CSkillManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	// learn skill function
	if (PPSTR(pCmd, "SKILL_LEARN") == 0)
	{
		const int SkillID = Extra1;
		if(pPlayer->GetSkill(SkillID)->Upgrade())
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// change emoticion function
	if (PPSTR(pCmd, "SKILL_CHANGE_USAGE_EMOTICION") == 0)
	{
		const int SkillID = Extra1;
		pPlayer->GetSkill(SkillID)->SelectNextControlEmote();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}
	return false;
}

void CSkillManager::UseSkillsByEmoticion(CPlayer *pPlayer, int EmoticionID)
{
	if(pPlayer && pPlayer->IsAuthed() && pPlayer->GetCharacter())
	{
		const int ClientID = pPlayer->GetCID();
		for(auto& p : CSkill::Data()[ClientID])
		{
			if(p->m_SelectedEmoticion == EmoticionID)
				p->Use();
		}
	}
}
