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
		const auto ManaCostPct = pRes->getInt("ManaCostPct");
		const auto PriceSP = pRes->getInt("PriceSP");
		const auto Passive = pRes->getBoolean("Passive");
		const auto ProfID = (ProfessionIdentifier)pRes->getInt("ProfessionID");

		CSkillDescription::CreateElement(ID)->Init(Name, Description, ProfID, ManaCostPct, PriceSP, Passive);
	}

	CSkillTree::LoadFromDB();
}

void CSkillManager::OnPlayerLogin(CPlayer *pPlayer)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '{}'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		const auto ID = pRes->getInt("SkillID");
		const auto SelectedEmoticon = pRes->getInt("UsedByEmoticon");
		CSkill::CreateElement(pPlayer->GetCID(), ID)->Init(true, SelectedEmoticon);
	}
}

void CSkillManager::OnClientReset(int ClientID)
{
	mystd::freeContainer(CSkill::Data()[ClientID]);
}

bool CSkillManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	// Skill selected detail information
	if(Menulist == MENU_SKILL_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_UPGRADES);

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

void CSkillManager::ShowSkill(CPlayer* pPlayer, int SkillID)
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const auto* pSkill = pPlayer->GetSkill(SkillID);
	const auto* pInfo = pSkill->Info();
	const bool Learned = pSkill->IsLearned();
	const bool Passive = pInfo->IsPassive();
	const int BaseCost = pInfo->GetPriceSP();
	const auto* pTree = CSkillTree::Get(SkillID);
	const int TreeMax = pTree ? pTree->GetMaxLevels() : 0;
	const int Progress = pSkill->GetTreeProgress();

	// Header
	VoteWrapper VHeader(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, pInfo->GetName());
	VHeader.Add(pInfo->GetDescription());
	if(!Passive && Learned)
		VHeader.Add("Mana required {}%", pInfo->GetManaCostPct());
	VHeader.AddItemValue(itSkillPoint);
	VoteWrapper::AddEmptyline(ClientID);

	// Manage
	VoteWrapper VManage(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Manage");
	if(Learned)
	{
		if(!Passive && Learned)
		{
			VManage.Add("F1 Bind: (bind 'key' say \"/use_skill {}\")", SkillID);
			VManage.AddOption("SKILL_CHANGE_USAGE_EMOTICON", SkillID, "Used on {}", pSkill->GetSelectedEmoticonName());
		}
		if(Progress > 0)
		{
			const int resetCost = pSkill->GetResetCostSP();
			VManage.AddOption("SKILL_TREE_RESET", SkillID, "Reset tree (cost: {} SP)", resetCost);
		}
	}
	else
	{
		VManage.AddOption("SKILL_LEARN", SkillID, "\u2726 Learn base (cost: {} SP)", BaseCost);
	}
	VoteWrapper::AddEmptyline(ClientID);

	// Skill tree
	if(Learned && pTree && TreeMax > 0)
	{
		VoteWrapper V(ClientID, VWF_OPEN | VWF_STYLE_STRICT, "Skill Tree ({} of {})", Progress, TreeMax);
		for(int Level = 1; Level <= TreeMax; ++Level)
		{
			const auto* pLevel = pTree->GetLevel(Level);
			if(!pLevel)
				continue;

			const int Selected = pSkill->GetSelectedOption(Level);
			const bool Completed = (Selected != 0);
			const bool Available = (!Completed && Level == (Progress + 1));
			const bool Locked = (!Completed && !Available);
			V.Add("{}LV.", Level);

			// is locked for now level
			if(Locked)
			{
				V.Add("\u2718 Hidden level");
				V.AddLine();
				continue;
			}

			// is available for now level
			if(Available)
			{
				for(const auto& Opt : pLevel->Options)
				{
					const int cost = pSkill->GetTreeNodePriceSP(Level, Opt.OptionIndex);
					const auto Modifer = format_skill_modifier(Opt.ModType, Opt.ModValue);
					V.AddOption("SKILL_SET_UPGRADE", MakeAnyList(SkillID, Level, Opt.OptionIndex), "\u2726 {}{} ({} SP) | {}", Opt.Name, Modifer, cost, Opt.Description);
				}
				V.AddLine();
				continue;
			}

			// unlocked level options
			for(const auto& Opt : pLevel->Options)
			{
				const char* pSymbol = Selected == Opt.OptionIndex ? "\u2714" : "\u2718";
				const auto Modifer = format_skill_modifier(Opt.ModType, Opt.ModValue);
				V.Add("{} {}{} | {}", pSymbol, Opt.Name, Modifer, Opt.Description);
			}
			V.AddLine();
		}
		VoteWrapper::AddEmptyline(ClientID);
	}
}

bool CSkillManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char* pReason)
{
	// Learn base
	if(PPSTR(pCmd, "SKILL_LEARN") == 0)
	{
		const int SkillID = GetIfExists<int>(Extras, 0, NOPE);
		if(pPlayer->GetSkill(SkillID)->Upgrade())
			pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(pCmd, "SKILL_SET_UPGRADE") == 0)
	{
		const int SkillID = GetIfExists<int>(Extras, 0, NOPE);
		const int LevelIndex = GetIfExists<int>(Extras, 1, NOPE);
		const int OptionIndex = GetIfExists<int>(Extras, 2, NOPE);
		if(pPlayer->GetSkill(SkillID)->SetTreeOption(LevelIndex, OptionIndex))
			pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(pCmd, "SKILL_TREE_RESET") == 0)
	{
		const int SkillID = GetIfExists<int>(Extras, 0, NOPE);
		if(pPlayer->GetSkill(SkillID)->ResetTree())
			pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(pCmd, "SKILL_CHANGE_USAGE_EMOTICON") == 0)
	{
		const int SkillID = GetIfExists<int>(Extras, 0, NOPE);
		pPlayer->GetSkill(SkillID)->SelectNextControlEmote();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	return false;
}


void CSkillManager::UseSkillsByEmoticon(CPlayer *pPlayer, int EmoticonID)
{
	if(pPlayer && pPlayer->IsAuthed() && pPlayer->GetCharacter())
	{
		const int ClientID = pPlayer->GetCID();
		for(auto& p : CSkill::Data()[ClientID])
		{
			if(p->m_SelectedEmoticon == EmoticonID)
				p->Use();
		}
	}
}
