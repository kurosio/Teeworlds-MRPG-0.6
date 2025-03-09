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


	// info
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Achievements (Information)");
	VInfo.Add("You can complete achievements and earn rewards.");
	VoteWrapper::AddEmptyline(ClientID);


	// main
	VoteWrapper VMain(ClientID, VWF_STYLE_STRICT | VWF_ALIGN_TITLE | VWF_SEPARATE, "\u2654 Main information");
	VMain.Add("{} of {} completed (progress {}%)", TotalCompleted, TotalAchievements, Percentage);
	VMain.Add("Achievement points: {}p", pPlayer->GetItem(itAchievementPoint)->GetValue());
	VoteWrapper::AddEmptyline(ClientID);


	// types
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2059 Achievements groups");
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


	// info
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE | VWF_ALIGN_TITLE, "\u2324 Achievements (Information)");
	VInfo.Add("Select an achievement from the list to get conditions.");
	VoteWrapper::AddEmptyline(ClientID);


	// achievements list
	VoteWrapper(ClientID).Add("\u2263 {} achievements", GetAchievementTypeName(Type));
	for(auto& pAchievement : achievements)
	{
		const auto* pInfo = pAchievement->Info();
		if(pInfo->GetType() != Type)
			continue;

		const auto progress = pAchievement->GetProgress();
		const auto required = pInfo->GetRequired();
		const bool completed = pAchievement->IsCompleted();
		const bool rewardExists = pInfo->RewardExists();

		VoteWrapper VAchievement(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "{} {}{}",
			completed ? "✔" : "×", pInfo->GetName(), rewardExists ? " : Reward" : "");
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
		wrapper.Add("Achievement point: {}", pInfo->GetPoint());
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
	auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::ranges::count_if(pAchievements, [Type](const CAchievement* p)
	{
		return p->Info()->GetType() == Type && p->IsCompleted();
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
