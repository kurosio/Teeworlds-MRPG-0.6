#include "skill_manager.h"
#include "skill_db.h"

#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>
#include "skill_data.h"

void CSkillManager::OnPreInit()
{
	auto Skills = SkillDb::ListSkills();
	while(Skills.Next())
	{
		const auto ID = Skills.GetInt("ID");
		const auto Name = Skills.GetString("Name");
		const auto Description = Skills.GetString("Description");
		const auto ManaCostPct = Skills.GetInt("ManaCostPct");
		const auto PriceSP = Skills.GetInt("PriceSP");
		const auto Passive = Skills.GetBoolean("Passive");
		const auto ProfID = (ProfessionIdentifier)Skills.GetInt("ProfessionID");

		CSkillDescription::CreateElement(ID)->Init(Name, Description, ProfID, ManaCostPct, PriceSP, Passive);
	}

	CSkillTree::LoadFromDB();
}

void CSkillManager::OnRegisterAccountLoadStages(CPlayer* pPlayer)
{
	auto* pAccount = pPlayer->Account();
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pAccount->GetID();
	pAccount->AddLoadStage("skills", [ClientID, AccountID](CAccountData::CLoadDone Done)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		SkillDb::LoadAccountSkills(AccountID).Then([pContext, AccountID, Done = std::move(Done)](Db::CResultSet&& Skills) mutable
		{
			if(!pContext->GetPlayer() || !Skills.Ok())
			{
				Done(false);
				return;
			}

			std::vector<std::pair<int, int>> vSkills;
			while(Skills.Next())
				vSkills.emplace_back(Skills.GetInt("SkillID"), Skills.GetInt("UsedByEmoticon"));

			// Load every tree choice in one request, not one blocking query per skill.
			SkillDb::LoadAccountTreeSelections(AccountID).Then([pContext, Done = std::move(Done), vSkills = std::move(vSkills)](Db::CResultSet&& Choices) mutable
			{
				auto* pPlayer = pContext->GetPlayer();
				if(!pPlayer || !Choices.Ok())
				{
					Done(false);
					return;
				}

				std::unordered_map<int, std::unordered_map<int, int>> TreeSelections;
				while(Choices.Next())
				{
					const int SkillID = Choices.GetInt("SkillID");
					const int Level = Choices.GetInt("LevelIndex");
					const int Option = Choices.GetInt("OptionIndex");
					if(SkillID > 0 && Level > 0 && Option > 0)
						TreeSelections[SkillID][Level] = Option;
				}

				for(const auto& [SkillID, SelectedEmoticon] : vSkills)
				{
					auto* pSkill = CSkill::CreateElement(pPlayer->GetCID(), SkillID);
					pSkill->Init(true, SelectedEmoticon);
					if(auto It = TreeSelections.find(SkillID); It != TreeSelections.end())
						pSkill->m_TreeSelections = std::move(It->second);
				}
				Done(true);
			});
		});
	});
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
		pPlayer->m_VotesData.SetLastMenuID(MENU_SKILLS);

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

void CSkillManager::ShowSkill(CPlayer * pPlayer, int SkillID)
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
	VoteWrapper::AddEmptyline(ClientID);

	// Manage
	VoteWrapper VManage(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Manage");
	VManage.AddItemValue(itSkillPoint);
	if(Learned)
	{
		if(!Passive)
		{
			VManage.AddLine();
			VManage.Add("F1 Bind: (bind 'key' say \"/use_skill {}\")", SkillID);
			VManage.AddOption("SKILL_CHANGE_USAGE_EMOTICON", SkillID, "Used on {}", pSkill->GetSelectedEmoticonName());
		}
		if(Progress > 0)
		{
			const int resetCost = pSkill->GetResetCostSP();
			VManage.AddLine();
			VManage.Add("Warning: Does not return SP to the player.", resetCost);
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
				for(const auto& [OptionIdx, Opt] : pLevel->Options)
				{
					const int cost = pSkill->GetTreeNodePriceSP(Level, OptionIdx);
					const auto Modifer = format_skill_modifier(Opt.ModType, Opt.ModValue);
					V.AddOption("SKILL_SET_UPGRADE", MakeAnyList(SkillID, Level, OptionIdx), "\u2726 {}{} ({} SP) | {}", Opt.Name, Modifer, cost, Opt.Description);
				}
				V.AddLine();
				continue;
			}

			// unlocked level options
			for(const auto& [OptionIdx, Opt] : pLevel->Options)
			{
				const char* pSymbol = Selected == OptionIdx ? "\u2714" : "\u2718";
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
		for(auto& SkillDataPair : CSkill::Data()[ClientID])
		{
			auto& SkillData = SkillDataPair.second;
			if(SkillData.m_SelectedEmoticon == EmoticonID)
				SkillData.Use();
		}
	}
}
