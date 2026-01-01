/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_manager.h"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <game/server/core/components/crafting/craft_manager.h>
#include <game/server/core/components/inventory/item_data.h>
#include <game/server/gamecontext.h>

namespace
{
	struct SAchievementLevelInfo
	{
		int m_Level {};
		int m_TotalLevels {};
	};

	using AchievementGroupKey = std::pair<AchievementType, int>;

	std::unordered_map<const CAchievementInfo*, SAchievementLevelInfo> BuildLevelInfoByAchievement(
		const std::deque<CAchievement*>& Achievements,
		AchievementType Type)
	{
		std::map<AchievementGroupKey, std::vector<const CAchievementInfo*>> groupedByCriteria;
		for(const auto* pAchievement : Achievements)
		{
			const auto* pInfo = pAchievement->Info();
			if(pInfo->GetType() != Type)
				continue;

			groupedByCriteria[{pInfo->GetType(), pInfo->GetCriteria()}].push_back(pInfo);
		}

		std::unordered_map<const CAchievementInfo*, SAchievementLevelInfo> levelInfoByAchievement;
		for(auto& [key, grouped] : groupedByCriteria)
		{
			std::ranges::sort(grouped, [](const CAchievementInfo* pLeft, const CAchievementInfo* pRight)
			{
				if(pLeft->GetRequired() == pRight->GetRequired())
					return pLeft->GetID() < pRight->GetID();
				return pLeft->GetRequired() < pRight->GetRequired();
			});

			const int totalLevels = (int)grouped.size();
			for(int i = 0; i < totalLevels; ++i)
			{
				levelInfoByAchievement[grouped[i]] = { i + 1, totalLevels };
			}
		}

		return levelInfoByAchievement;
	}

	std::unordered_set<const CAchievementInfo*> BuildVisibleAchievements(
		const std::deque<CAchievement*>& Achievements,
		AchievementType Type)
	{
		std::map<AchievementGroupKey, std::vector<const CAchievement*>> groupedByCriteria;
		for(const auto* pAchievement : Achievements)
		{
			const auto* pInfo = pAchievement->Info();
			if(pInfo->GetType() != Type)
				continue;

			groupedByCriteria[{pInfo->GetType(), pInfo->GetCriteria()}].push_back(pAchievement);
		}

		std::unordered_set<const CAchievementInfo*> visible;
		for(auto& [key, grouped] : groupedByCriteria)
		{
			std::ranges::sort(grouped, [](const CAchievement* pLeft, const CAchievement* pRight)
			{
				const auto* pLeftInfo = pLeft->Info();
				const auto* pRightInfo = pRight->Info();
				if(pLeftInfo->GetRequired() == pRightInfo->GetRequired())
					return pLeftInfo->GetID() < pRightInfo->GetID();
				return pLeftInfo->GetRequired() < pRightInfo->GetRequired();
			});

			int lastCompletedIndex = -1;
			for(int i = 0; i < (int)grouped.size(); ++i)
			{
				if(grouped[i]->IsCompleted())
					lastCompletedIndex = i;
			}

			const int maxVisibleIndex = std::min(lastCompletedIndex + 1, (int)grouped.size() - 1);
			for(int i = 0; i <= maxVisibleIndex; ++i)
				visible.insert(grouped[i]->Info());
		}

		return visible;
	}

	void AddRewardInfo(VoteWrapper& Wrapper, const CAchievementInfo* pInfo)
	{
		std::vector<std::string> rewardEntries;
		rewardEntries.reserve(4);

		const auto& dataJson = pInfo->GetRewardData();
		if(!dataJson.empty())
		{
			if(const int Experience = dataJson.value("exp", 0); Experience > 0)
				rewardEntries.push_back("Exp " + std::to_string(Experience));

			const CItemsContainer items = dataJson.value("items", CItemsContainer {});
			for(const auto& item : items)
			{
				if(!item.IsValid())
					continue;
				rewardEntries.push_back(std::string(item.Info()->GetName()) + " x" + std::to_string(item.GetValue()));
			}
		}

		if(const int Points = pInfo->GetPoint(); Points > 0)
			rewardEntries.push_back("Points " + std::to_string(Points) + "p");

		if(rewardEntries.empty())
			return;

		std::string rewardText;
		for(size_t i = 0; i < rewardEntries.size(); ++i)
		{
			if(i > 0)
				rewardText += ", ";
			rewardText += rewardEntries[i];
		}

		Wrapper.Add("Rewards: {}", rewardText);
	}
}

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
		std::string Name = pResult->getString("Name");
		std::string Reward = pResult->getString("Reward");
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

		if(const auto TypeID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowTypeList(pPlayer, (AchievementType)TypeID.value());
			VoteWrapper::AddEmptyline(ClientID);
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

void CAchievementManager::ShowMenu(CPlayer* pPlayer) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const auto TotalAchievements = GetCount();
	const auto TotalCompleted = GetCompletedCount(ClientID);
	const auto Percentage = round_to_int(translate_to_percent(TotalAchievements, TotalCompleted));
	const auto progressBar = mystd::string::progressBar(100, Percentage, 7, "\u25B0", "\u25B1");


	// info
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Achievements (Information)");
	VInfo.Add("You can complete achievements and earn rewards.");
	VoteWrapper::AddEmptyline(ClientID);


	// main
	VoteWrapper VMain(ClientID, VWF_STYLE_STRICT | VWF_ALIGN_TITLE | VWF_SEPARATE, "\u2654 Main information");
	VMain.Add("Overall: {} of {} completed", TotalCompleted, TotalAchievements);
	VMain.Add("Progress: {} {}%", progressBar, Percentage);
	VMain.Add("Achievement points: {}p", pPlayer->GetItem(itAchievementPoint)->GetValue());
	VoteWrapper::AddEmptyline(ClientID);


	// types
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2059 Achievement groups");
	for(int i = (int)AchievementType::DefeatPVP; i <= (int)AchievementType::Leveling; i++)
	{
		auto Type = (AchievementType)i;
		auto Completed = GetCompletedCountByType(ClientID, Type);
		auto Total = GetCountByType(Type);
		VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECT, i, "{} ({} of {})", GetAchievementTypeName(Type), Completed, Total);
	}
}

void CAchievementManager::ShowTypeList(CPlayer* pPlayer, AchievementType Type) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const auto& achievements = CAchievement::Data()[ClientID];
	const auto levelInfoByAchievement = BuildLevelInfoByAchievement(achievements, Type);
	const auto visibleAchievements = BuildVisibleAchievements(achievements, Type);


	// info
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE | VWF_ALIGN_TITLE, "\u2324 Achievements (Information)");
	VInfo.Add("Select an achievement from the list to get conditions.");
	VoteWrapper::AddEmptyline(ClientID);


	// achievements list
	VoteWrapper(ClientID).Add("\u2263 {} achievements", GetAchievementTypeName(Type));
	for(const auto& pAchievement : achievements)
	{
		const auto* pInfo = pAchievement->Info();
		if(pInfo->GetType() != Type)
			continue;
		if(!visibleAchievements.contains(pInfo))
			continue;

		const auto progress = pAchievement->GetProgress();
		const auto required = pInfo->GetRequired();
		const bool completed = pAchievement->IsCompleted();
		const bool rewardExists = pInfo->RewardExists();
		const auto levelInfoIt = levelInfoByAchievement.find(pInfo);
		const int level = levelInfoIt != levelInfoByAchievement.end() ? levelInfoIt->second.m_Level : 0;
		const int totalLevels = levelInfoIt != levelInfoByAchievement.end() ? levelInfoIt->second.m_TotalLevels : 0;
		const std::string levelSuffix = totalLevels > 1
			? " [Level " + std::to_string(level) + "/" + std::to_string(totalLevels) + "]"
			: "";

		VoteWrapper VAchievement(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "{} {}{}{}",
			completed ? "✔" : "○", pInfo->GetName(), levelSuffix, rewardExists ? " : Reward" : "");
		AddAchievementDetails(VAchievement, pInfo, progress, required, level, totalLevels);
	}
}

void CAchievementManager::AddAchievementDetails(VoteWrapper& VAchievement, const CAchievementInfo* pInfo, int Progress, int Required, int Level, int TotalLevels) const
{
	auto addProgressInfo = [&](VoteWrapper& wrapper, auto progress, auto required, std::string action, std::string needed = {})
	{
		const auto progressAchievement = translate_to_percent(Required, Progress);
		const auto progressBar = mystd::string::progressBar(100, (int)progressAchievement, 5, "\u25B0", "\u25B1");
		wrapper.Add("Progress: {} - {~.2}%", progressBar, progressAchievement);
		if(needed.empty())
			wrapper.Add("Goal: {} {}/{}", action, progress, required);
		else
			wrapper.Add("Goal: {} {}/{} {}", action, progress, required, needed);
	};

	if(TotalLevels > 1)
		VAchievement.Add("Level: {}/{}", Level, TotalLevels);

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
			addProgressInfo(VAchievement, Progress, Required, "Equip", pItem->GetName());
			break;
		}
		case AchievementType::CompleteQuest:
		{
			CQuestDescription* pQuestInfo = GS()->GetQuestInfo(pInfo->GetCriteria());
			addProgressInfo(VAchievement, Progress, Required, "Complete", pQuestInfo->GetName());
			break;
		}
		case AchievementType::Leveling:
		{
			const auto ProfessionName = std::string(GetProfessionName((ProfessionIdentifier)pInfo->GetCriteria()));
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

	AddRewardInfo(VAchievement, pInfo);
}

int CAchievementManager::GetCount() const
{
	return (int)CAchievementInfo::Data().size();
}

int CAchievementManager::GetCountByType(AchievementType Type) const
{
	return (int)std::ranges::count_if(CAchievementInfo::Data(), [Type](const CAchievementInfo* pAchievement)
	{
		return pAchievement->GetType() == Type;
	});
}

int CAchievementManager::GetCompletedCountByType(int ClientID, AchievementType Type) const
{
	const auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::ranges::count_if(pAchievements, [Type](const CAchievement* p)
	{
		return p->Info()->GetType() == Type && p->IsCompleted();
	});
}

int CAchievementManager::GetCompletedCount(int ClientID) const
{
	const auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::ranges::count_if(pAchievements, [](const CAchievement* p)
	{
		return p->IsCompleted();
	});
}
