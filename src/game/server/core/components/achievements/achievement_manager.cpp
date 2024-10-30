/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_manager.h"

#include <game/server/core/components/crafting/craft_manager.h>
#include <game/server/gamecontext.h>

void CAchievementManager::OnPreInit()
{
	ResultPtr pResult = Database->Execute<DB::SELECT>("*", TW_ACHIEVEMENTS);
	while(pResult->next())
	{
		// initialize variables
		int ID = pResult->getInt("ID");
		int Type = pResult->getInt("Type");
		int Criteria = pResult->getInt("Criteria");
		int Required = pResult->getInt("Required");
		std::string Name = pResult->getString("Name").c_str();
		std::string Reward = pResult->getString("Reward").c_str();
		int AchievementPoint = pResult->getInt("AchievementPoint");

		// create element
		auto* pAchievement = CAchievementInfo::CreateElement(ID);
		pAchievement->Init(Name, (AchievementType)Type, Criteria, Required, Reward, AchievementPoint);
	}

	// sort achievements by name
	std::ranges::sort(CAchievementInfo::Data(), [](const CAchievementInfo* p1, const CAchievementInfo* p2)
	{
		return std::string_view(p1->GetName()) < std::string_view(p2->GetName());
	});
}

void CAchievementManager::OnClientReset(int ClientID)
{
	mystd::freeContainer(CAchievement::Data()[ClientID]);
}

bool CAchievementManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// menu achievements
	if(Menulist == MENU_ACHIEVEMENTS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu achievements group selected
	if(Menulist == MENU_ACHIEVEMENTS_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_ACHIEVEMENTS);

		if(const auto GroupID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowGroupMenu(pPlayer, GroupID.value());
			VoteWrapper::AddEmptyline(ClientID);
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

void CAchievementManager::ShowMenu(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Achievements (Information)");
	VInfo.Add("You can complete achievements and earn rewards.");
	VoteWrapper::AddEmptyline(ClientID);

	// main
	int TotalAchievements = GetCount();
	int TotalCompleted = GetCompletedCount(ClientID);
	int Percentage = translate_to_percent(TotalAchievements, TotalCompleted);
	VoteWrapper VMain(ClientID, VWF_STYLE_STRICT | VWF_ALIGN_TITLE | VWF_SEPARATE, "\u2654 Main information");
	VMain.Add("{} of {} completed (progress {}%)", TotalCompleted, TotalAchievements, Percentage);
	VMain.Add("Achievement points: {}p", pPlayer->GetItem(itAchievementPoint)->GetValue());
	VoteWrapper::AddEmptyline(ClientID);

	// show group
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2059 Achievements groups");
	int CompletedGeneral = GetCompletedCountByGroup(ClientID, ACHIEVEMENT_GROUP_GENERAL);
	int TotalGeneral = GetCountByGroup(ACHIEVEMENT_GROUP_GENERAL);
	VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECT, ACHIEVEMENT_GROUP_GENERAL, "General ({} of {})", CompletedGeneral, TotalGeneral);

	int CompletedBattle = GetCompletedCountByGroup(ClientID, ACHIEVEMENT_GROUP_BATTLE);
	int TotalBattle = GetCountByGroup(ACHIEVEMENT_GROUP_BATTLE);
	VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECT, ACHIEVEMENT_GROUP_BATTLE, "Battle ({} of {})", CompletedBattle, TotalBattle);

	int CompletedItems = GetCompletedCountByGroup(ClientID, ACHIEVEMENT_GROUP_ITEMS);
	int TotalItems = GetCountByGroup(ACHIEVEMENT_GROUP_ITEMS);
	VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECT, ACHIEVEMENT_GROUP_ITEMS, "Items ({} of {})", CompletedItems, TotalItems);
}

void CAchievementManager::ShowGroupMenu(CPlayer* pPlayer, int groupID) const
{
	const int ClientID = pPlayer->GetCID();
	const char* apGroupName[] = { "General", "Battle", "Items" };

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE | VWF_ALIGN_TITLE, "\u2324 Achievements (Information)");
	VInfo.Add("Select an achievement from the list to get conditions.");
	VoteWrapper::AddEmptyline(ClientID);

	// achievements list
	const auto& achievements = CAchievement::Data()[ClientID];
	VoteWrapper(ClientID).Add("\u2263 {} achievements", apGroupName[groupID]);
	for(auto& pAchievement : achievements)
	{
		const auto* pInfo = pAchievement->Info();
		if(pInfo->GetGroup() != groupID)
			continue;

		const auto progress = pAchievement->GetProgress();
		const auto required = pInfo->GetRequired();
		const bool completed = pAchievement->IsCompleted();
		const bool rewardExists = pInfo->RewardExists();

		VoteWrapper VAchievement(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "{} ({}AP) {}{}",
			completed ? "✔" : "×",
			pInfo->GetPoint(),
			pInfo->GetName(),
			rewardExists ? " : Reward" : "");

		AddAchievementDetails(VAchievement, pInfo, progress, required);
	}
}

void CAchievementManager::AddAchievementDetails(VoteWrapper& VAchievement, const CAchievementInfo* pInfo, int Progress, int Required) const
{
	auto addProgressInfo = [&](VoteWrapper& wrapper, auto progress, auto required, std::string action, std::string needed = {})
	{
		const auto progressAchievement = translate_to_percent(Required, Progress);
		const auto progressBar = mystd::string::progressBar(100, (int)progressAchievement, 10, "\u25B0", "\u25B1");

		wrapper.Add("Progress: {} - {~.2}%", progressBar, progressAchievement);
		if(needed.empty())
			wrapper.Add("{} {}/{}", action, progress, required);
		else
			wrapper.Add("{} {}/{} {}", action, progress, required, needed);
	};

	switch(const auto Type = pInfo->GetType())
	{
		case AchievementType::ReceiveItem:
		case AchievementType::HaveItem:
		{
			const auto* pItem = GS()->GetItemInfo(pInfo->GetCriteria());
			const auto Action = Type == AchievementType::ReceiveItem ? "Receive" : "Have";
			addProgressInfo(VAchievement, Progress, Required, Action, pItem->GetName());
			break;
		}
		case AchievementType::TotalDamage:
		{
			addProgressInfo(VAchievement, Progress, Required, "Total damage");
			break;
		}
		case AchievementType::Death:
		{
			addProgressInfo(VAchievement, Progress, Required, "Deaths");
			break;
		}
		case AchievementType::DefeatMob:
		{
			const auto botID = pInfo->GetCriteria();
			const auto botName = DataBotInfo::ms_aDataBot[botID].m_aNameBot;
			addProgressInfo(VAchievement, Progress, Required, "Defeat", botName);
			break;
		}
		case AchievementType::DefeatPVE:
		{
			addProgressInfo(VAchievement, Progress, Required, "Defeat", "enemies");
			break;
		}
		case AchievementType::DefeatPVP:
		{
			addProgressInfo(VAchievement, Progress, Required, "Defeat", "players");
			break;
		}
		case AchievementType::Equip:
		{
			CItemDescription* pItem = GS()->GetItemInfo(pInfo->GetCriteria());
			VAchievement.Add("Equip {}", pItem->GetName());
			break;
		}
		case AchievementType::UnlockWorld:
		{
			int worldID = pInfo->GetCriteria();
			VAchievement.Add("Unlock {}", Server()->GetWorldName(worldID));
			break;
		}
		case AchievementType::Leveling:
		{
			const auto ProfessionName = std::string(GetProfessionName((Professions)pInfo->GetCriteria()));
			const auto ActionStr = "Reach " + ProfessionName;
			addProgressInfo(VAchievement, Progress, Required, ActionStr, "levels");
			break;
		}
		case AchievementType::CraftItem:
		{
			if(CCraftItem* pItem = Core()->CraftManager()->GetCraftByID(pInfo->GetCriteria()))
			{
				addProgressInfo(VAchievement, Progress, Required, "Craft", pItem->GetItem()->Info()->GetName());
			} break;
		}
	}
}

void CAchievementManager::UpdateAchievement(CPlayer* pPlayer, AchievementType Type, int Criteria, int Progress, int ProgressType) const
{
	if(!pPlayer || pPlayer->IsBot())
		return;

	// initialize variables
	bool Updated = false;
	auto& pAchievements = CAchievement::Data()[pPlayer->GetCID()];

	// search for the achievement
	for(const auto& pAchievement : pAchievements)
	{
		const auto achievementType = pAchievement->Info()->GetType();
		const auto achievementCriteria = pAchievement->Info()->GetCriteria();

		if(achievementType != Type || pAchievement->IsCompleted())
			continue;

		if(achievementCriteria <= 0 || achievementCriteria == Criteria)
		{
			if(pAchievement->UpdateProgress(Criteria, Progress, ProgressType))
				Updated = true;
		}
	}

	// update the achievement progress in the database
	if(Updated)
	{
		GS()->Core()->SaveAccount(pPlayer, SAVE_ACHIEVEMENTS);
	}
}

int CAchievementManager::GetCount() const
{
	return (int)CAchievementInfo::Data().size();
}

int CAchievementManager::GetCountByGroup(int Group) const
{
	return (int)std::ranges::count_if(CAchievementInfo::Data(), [Group](const CAchievementInfo* pAchievement)
	{
		return pAchievement->GetGroup() == Group;
	});
}

int CAchievementManager::GetCompletedCountByGroup(int ClientID, int Group) const
{
	auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::ranges::count_if(pAchievements, [Group](const CAchievement* p)
	{
		return p->Info()->GetGroup() == Group && p->IsCompleted();
	});
}

int CAchievementManager::GetCompletedCount(int ClientID) const
{
	auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::ranges::count_if(pAchievements, [](const CAchievement* p)
	{
		return p->IsCompleted();
	});
}
