/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_manager.h"
#include "achievement_db.h"

#include <algorithm>
#include <array>
#include <map>
#include <string_view>
#include <utility>

#include <components/bots/bot_data.h>
#include <components/crafting/craft_manager.h>
#include <components/inventory/item_data.h>
#include <components/quests/quest_data.h>
#include <game/server/gamecontext.h>

namespace
{
	constexpr int ACHIEVEMENT_TYPE_COUNT = (int)AchievementType::Leveling + 1;

	using TCounts = std::array<int, ACHIEVEMENT_TYPE_COUNT>;

	CAchievement* FindAchievement(int ClientID, int AchievementID)
	{
		const auto It = CAchievement::Data().find(ClientID);
		if(It == CAchievement::Data().end())
			return nullptr;

		const auto AchievementIt = It->second.find(AchievementID);
		return AchievementIt != It->second.end() ? AchievementIt->second : nullptr;
	}
}

void CAchievementManager::OnPreInit()
{
	auto Achievements = AchievementDb::ListAchievements();
	while(Achievements.Next())
	{
		auto* pInfo = CAchievementInfo::CreateElement(Achievements.GetInt("ID"));
		pInfo->Init(Achievements.GetString("Name"), (AchievementType)Achievements.GetInt("Type"),
			Achievements.GetInt("Criteria"), Achievements.GetInt("Required"),
			Achievements.GetString("Reward"), Achievements.GetInt("AchievementPoint"));
	}

	std::ranges::sort(CAchievementInfo::Data(), [](const CAchievementInfo* pLeft, const CAchievementInfo* pRight)
	{
		return std::string_view(pLeft->GetName()) < std::string_view(pRight->GetName());
	});

	BuildChains();
}

void CAchievementManager::OnPostInit()
{
	// Item descriptions are loaded by other components in OnPreInit()
	for(auto* pInfo : CAchievementInfo::Data())
		pInfo->BuildRewardText();
}

void CAchievementManager::BuildChains()
{
	std::map<std::pair<int, int>, std::vector<CAchievementInfo*>> Grouped;
	for(auto* pInfo : CAchievementInfo::Data())
		Grouped[{ (int)pInfo->GetType(), pInfo->GetCriteria() }].push_back(pInfo);

	m_vChains.clear();
	m_vChains.reserve(Grouped.size());
	for(auto& [Key, vInfos] : Grouped)
	{
		std::ranges::sort(vInfos, [](const CAchievementInfo* pLeft, const CAchievementInfo* pRight)
		{
			if(pLeft->GetRequired() != pRight->GetRequired())
				return pLeft->GetRequired() < pRight->GetRequired();
			return pLeft->GetID() < pRight->GetID();
		});

		SAchievementChain Chain;
		Chain.m_Type = (AchievementType)Key.first;
		Chain.m_vIDs.reserve(vInfos.size());
		for(size_t i = 0; i < vInfos.size(); ++i)
		{
			if(i + 1 == vInfos.size())
				vInfos[i]->SetChainTail();
			Chain.m_vIDs.push_back(vInfos[i]->GetID());
		}
		m_vChains.push_back(std::move(Chain));
	}
}

void CAchievementManager::OnClientReset(int ClientID)
{
	mystd::freeContainer(CAchievement::Data()[ClientID]);
}

bool CAchievementManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_ACHIEVEMENTS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

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
	const int ClientID = pPlayer->GetCID();
	TCounts TotalByType {};
	TCounts CompletedByType {};
	for(const auto* pInfo : CAchievementInfo::Data())
	{
		const int TypeIndex = (int)pInfo->GetType();
		if(TypeIndex > 0 && TypeIndex < ACHIEVEMENT_TYPE_COUNT)
			TotalByType[TypeIndex]++;
	}

	const auto ClientIt = CAchievement::Data().find(ClientID);
	if(ClientIt != CAchievement::Data().end())
	{
		for(const auto& Entry : ClientIt->second)
		{
			const int TypeIndex = (int)Entry.second->Info()->GetType();
			if(Entry.second->IsCompleted() && TypeIndex > 0 && TypeIndex < ACHIEVEMENT_TYPE_COUNT)
				CompletedByType[TypeIndex]++;
		}
	}

	int TotalAchievements = 0;
	int TotalCompleted = 0;
	for(int i = 0; i < ACHIEVEMENT_TYPE_COUNT; ++i)
	{
		TotalAchievements += TotalByType[i];
		TotalCompleted += CompletedByType[i];
	}

	const auto Percentage = round_to_int(translate_to_percent(TotalAchievements, TotalCompleted));
	const auto progressBar = mystd::string::progressBar(100, Percentage, 7, "\u25B0", "\u25B1");

	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Achievements (Information)");
	VInfo.Add("You can complete achievements and earn rewards.");
	VoteWrapper::AddEmptyline(ClientID);

	VoteWrapper VMain(ClientID, VWF_STYLE_STRICT | VWF_ALIGN_TITLE | VWF_SEPARATE, "\u2654 Main information");
	VMain.Add("Overall: {} of {} completed", TotalCompleted, TotalAchievements);
	VMain.Add("Progress: {} {}%", progressBar, Percentage);
	VMain.Add("Achievement points: {}p", pPlayer->GetItem(itAchievementPoint)->GetValue());
	VoteWrapper::AddEmptyline(ClientID);

	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2059 Achievement groups");
	for(int i = (int)AchievementType::DefeatPVP; i <= (int)AchievementType::Leveling; i++)
	{
		const auto Type = (AchievementType)i;
		VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECT, i, "{} ({} of {})",
			GetAchievementTypeName(Type), CompletedByType[i], TotalByType[i]);
	}
}

void CAchievementManager::ShowTypeList(CPlayer* pPlayer, AchievementType Type) const
{
	const int ClientID = pPlayer->GetCID();

	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE | VWF_ALIGN_TITLE, "\u2324 Achievements (Information)");
	VInfo.Add("Select an achievement from the list to get conditions.");
	VoteWrapper::AddEmptyline(ClientID);
	VoteWrapper(ClientID).Add("\u2263 {} achievements", GetAchievementTypeName(Type));

	for(const auto& Chain : m_vChains)
	{
		if(Chain.m_Type != Type)
			continue;

		const int TotalLevels = (int)Chain.m_vIDs.size();
		int LastCompletedIndex = -1;
		for(int i = 0; i < TotalLevels; ++i)
		{
			const auto* pAchievement = FindAchievement(ClientID, Chain.m_vIDs[i]);
			if(pAchievement && pAchievement->IsCompleted())
				LastCompletedIndex = i;
		}

		const int MaxVisibleIndex = std::min(LastCompletedIndex + 1, TotalLevels - 1);
		for(int i = 0; i <= MaxVisibleIndex; ++i)
		{
			const auto* pAchievement = FindAchievement(ClientID, Chain.m_vIDs[i]);
			if(!pAchievement)
				continue;

			const auto* pInfo = pAchievement->Info();
			const std::string LevelSuffix = TotalLevels > 1
				? " [Level " + std::to_string(i + 1) + "/" + std::to_string(TotalLevels) + "]"
				: "";

			VoteWrapper VAchievement(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "{} {}{~}",
				pAchievement->IsCompleted() ? "✔" : "○", pInfo->GetName(), LevelSuffix);
			AddAchievementDetails(VAchievement, pInfo, pAchievement->GetProgress(), i + 1, TotalLevels);
		}
	}
}

void CAchievementManager::AddAchievementDetails(VoteWrapper& Vote, const CAchievementInfo* pInfo, int Progress, int Level, int TotalLevels) const
{
	const auto Required = pInfo->GetRequired();
	auto AddProgressInfo = [&](const std::string& Action, const std::string& Needed = {})
	{
		const auto Percent = translate_to_percent(Required, Progress);
		const auto progressBar = mystd::string::progressBar(100, (int)Percent, 5, "\u25B0", "\u25B1");
		Vote.Add("Progress: {} - {~.2}%", progressBar, Percent);
		if(Needed.empty())
			Vote.Add("Goal: {} {}/{}", Action, Progress, Required);
		else
			Vote.Add("Goal: {} {}/{} {}", Action, Progress, Required, Needed);
	};

	if(TotalLevels > 1)
		Vote.Add("Level: {}/{}", Level, TotalLevels);

	std::string Action;
	std::string Needed;
	const auto Type = pInfo->GetType();
	const int Criteria = pInfo->GetCriteria();
	switch(Type)
	{
		case AchievementType::ReceiveItem:
		case AchievementType::HaveItem:
		{
			const auto It = CItemDescription::Data().find(Criteria);
			if(It != CItemDescription::Data().end())
			{
				Action = Type == AchievementType::ReceiveItem ? "Receive" : "Have";
				Needed = It->second.GetName();
			}
			break;
		}
		case AchievementType::TotalDamage:
			Action = "Total damage";
			break;
		case AchievementType::Death:
			Action = "Deaths";
			break;
		case AchievementType::DefeatMob:
		{
			const auto It = DataBotInfo::ms_aDataBot.find(Criteria);
			if(It != DataBotInfo::ms_aDataBot.end())
			{
				Action = "Defeat";
				Needed = It->second.m_aNameBot;
			}
			break;
		}
		case AchievementType::DefeatPVE:
			Action = "Defeat";
			Needed = "enemies";
			break;
		case AchievementType::DefeatPVP:
			Action = "Defeat";
			Needed = "players";
			break;
		case AchievementType::Equip:
		{
			const auto It = CItemDescription::Data().find(Criteria);
			if(It != CItemDescription::Data().end())
			{
				Action = "Equip";
				Needed = It->second.GetName();
			}
			break;
		}
		case AchievementType::CompleteQuest:
		{
			if(const auto* pQuest = GS()->GetQuestInfo(Criteria))
			{
				Action = "Complete";
				Needed = pQuest->GetName();
			}
			break;
		}
		case AchievementType::Leveling:
			Action = std::string("Reach ") + GetProfessionName((ProfessionIdentifier)Criteria);
			Needed = "levels";
			break;
		case AchievementType::CraftItem:
		{
			if(const auto* pCraft = Core()->CraftManager()->GetCraftByID(Criteria))
			{
				Action = "Craft";
				Needed = pCraft->GetItem()->Info()->GetName();
			}
			break;
		}
	}

	if(!Action.empty())
		AddProgressInfo(Action, Needed);

	if(!pInfo->GetRewardText().empty())
		Vote.Add("Rewards: {~}", pInfo->GetRewardText());
}
