/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_manager.h"

#include <game/server/core/components/crafting/craft_manager.h>
#include <game/server/gamecontext.h>

void CAchievementManager::OnInit()
{
	ResultPtr pResult = Database->Execute<DB::SELECT>("*", TW_ACHIEVEMENTS);
	while(pResult->next())
	{
		// initialize variables
		int ID = pResult->getInt("ID");
		int Type = pResult->getInt("Type");
		std::string Name = pResult->getString("Name").c_str();
		std::string Criteria = pResult->getString("Criteria").c_str();
		std::string Reward = pResult->getString("Reward").c_str();
		int AchievementPoint = pResult->getInt("AchievementPoint");

		// create element
		auto* pAchievement = CAchievementInfo::CreateElement(ID);
		pAchievement->Init(Name, Type, Criteria, Reward, AchievementPoint);
	}

	// sort achievements by name
	std::sort(CAchievementInfo::Data().begin(), CAchievementInfo::Data().end(),
		[](const CAchievementInfo* p1, const CAchievementInfo* p2) { return std::string_view(p1->GetName()) < std::string_view(p2->GetName()); });
}

void CAchievementManager::OnResetClient(int ClientID)
{
	// free achievement data
	for(auto& p : CAchievement::Data()[ClientID])
		delete p;
	CAchievement::Data()[ClientID].clear();
}

bool CAchievementManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	return false;
}

bool CAchievementManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	int ClientID = pPlayer->GetCID();

	// menu achievements
	if(Menulist == MENU_ACHIEVEMENTS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu achievements group selected
	if(Menulist == MENU_ACHIEVEMENTS_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_ACHIEVEMENTS);
		ShowGroupMenu(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

void CAchievementManager::ShowMenu(CPlayer* pPlayer) const
{
	// check valid player
	if (!pPlayer)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Achievements (Information)");
	VInfo.Add("You can complete achievements and earn rewards.");
	VoteWrapper::AddEmptyline(ClientID);

	// main
	int TotalAchievements = GetCount();
	int TotalCompleted = GetCompletedCount(ClientID);
	int Percentage = translate_to_percent(TotalAchievements, TotalCompleted);
	VoteWrapper VMain(ClientID, VWF_STYLE_STRICT|VWF_SEPARATE, "\u2654 Main information");
	VMain.Add("{} of {} completed (progress {}%)", TotalCompleted, TotalAchievements, Percentage);
	VMain.Add("Achievement points: {}p", pPlayer->GetItem(itAchievementPoint)->GetValue());
	VoteWrapper::AddEmptyline(ClientID);

	// show group
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2059 Achievements groups");
	int CompletedGeneral = GetCompletedCountByGroup(ClientID, ACHIEVEMENT_GROUP_GENERAL);
	int TotalGeneral = GetCountByGroup(ACHIEVEMENT_GROUP_GENERAL);
	VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECTED, ACHIEVEMENT_GROUP_GENERAL, "General ({} of {})", CompletedGeneral, TotalGeneral);

	int CompletedBattle = GetCompletedCountByGroup(ClientID, ACHIEVEMENT_GROUP_BATTLE);
	int TotalBattle = GetCountByGroup(ACHIEVEMENT_GROUP_BATTLE);
	VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECTED, ACHIEVEMENT_GROUP_BATTLE, "Battle ({} of {})", CompletedBattle, TotalBattle);

	int CompletedItems = GetCompletedCountByGroup(ClientID, ACHIEVEMENT_GROUP_ITEMS);
	int TotalItems = GetCountByGroup(ACHIEVEMENT_GROUP_ITEMS);
	VGroup.AddMenu(MENU_ACHIEVEMENTS_SELECTED, ACHIEVEMENT_GROUP_ITEMS, "Items ({} of {})", CompletedItems, TotalItems);
}

void CAchievementManager::ShowGroupMenu(CPlayer* pPlayer, int Group) const
{
	// check valid player
	if(!pPlayer)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	const char* pGroupName[] = { "General", "Battle", "Items" };

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Achievements (Information)");
	VInfo.Add("Select an achievement from the list to get conditions.");
	VoteWrapper::AddEmptyline(ClientID);

	// show all the achievements by group
	VoteWrapper(ClientID).Add("\u2263 {} achievements", pGroupName[Group]);
	auto& pAchievements = CAchievement::Data()[ClientID];
	for(auto& pAchievement : pAchievements)
	{
		// initialize variables
		int Progress = pAchievement->GetProgress();
		int Required = pAchievement->Info()->GetMiscRequired();
		int Type = pAchievement->Info()->GetType();
		bool Completed = pAchievement->IsCompleted();
		bool RewardExists = pAchievement->Info()->RewardExists();

		// check group
		if(pAchievement->Info()->GetGroup() != Group)
			continue;

		// show achievement
		VoteWrapper VAchievement(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "{} ({}ap) {}{}", (Completed ? "✔" : "×"),
			pAchievement->Info()->GetAchievementPoint(), pAchievement->Info()->GetName(), (RewardExists ? " : Reward" : "\0"));
		if(Type == ACHIEVEMENT_RECEIVE_ITEM)
		{
			CItemDescription* pItem = GS()->GetItemInfo(pAchievement->Info()->GetMisc());
			VAchievement.Add("Receive {}/{} {}", Progress, Required, pItem->GetName());
		}
		else if(Type == ACHIEVEMENT_HAVE_ITEM)
		{
			CItemDescription* pItem = GS()->GetItemInfo(pAchievement->Info()->GetMisc());
			VAchievement.Add("Have {}/{} {}", Progress, Required, pItem->GetName());
		}
		else if(Type == ACHIEVEMENT_TOTAL_DAMAGE)
		{
			VAchievement.Add("Total damage {}/{}", Progress, Required);
		}
		else if(Type == ACHIEVEMENT_DEATH)
		{
			VAchievement.Add("Deaths {}/{}", Progress, Required);
		}
		else if(Type == ACHIEVEMENT_DEFEAT_MOB)
		{
			int BotID = pAchievement->Info()->GetMisc();
			std::string BotName = DataBotInfo::ms_aDataBot[BotID].m_aNameBot;
			VAchievement.Add("Defeat {}/{} {}", Progress, Required, BotName);
		}
		else if(Type == ACHIEVEMENT_DEFEAT_PVE)
		{
			VAchievement.Add("Defeat {}/{} enemies", Progress, Required);
		}
		else if(Type == ACHIEVEMENT_DEFEAT_PVP)
		{
			VAchievement.Add("Defeat {}/{} players", Progress, Required);
		}
		else if(Type == ACHIEVEMENT_EQUIP)
		{
			CItemDescription* pItem = GS()->GetItemInfo(pAchievement->Info()->GetMisc());
			VAchievement.Add("Equip {}", pItem->GetName());
		}
		else if(Type == ACHIEVEMENT_UNLOCK_WORLD)
		{
			int WorldID = pAchievement->Info()->GetMisc();
			VAchievement.Add("Unlock {}", Progress, Required, Server()->GetWorldName(WorldID));
		}
		else if(Type == ACHIEVEMENT_LEVELING)
		{
			VAchievement.Add("Reach {}/{} levels", Progress, Required);
		}
		else if(Type == ACHIEVEMENT_CRAFT_ITEM)
		{
			if(CCraftItem* pItem = Core()->CraftManager()->GetCraftByID(pAchievement->Info()->GetMisc()))
				VAchievement.Add("Craft {}/{} {}", Progress, Required, pItem->GetItem()->Info()->GetName());
		}
	}
}

int CAchievementManager::GetCountByGroup(int Group) const
{
	return (int)std::count_if(CAchievementInfo::Data().begin(), CAchievementInfo::Data().end(), [Group](const CAchievementInfo* pAchievement)
	{ return pAchievement->GetGroup() == Group; });
}

int CAchievementManager::GetCompletedCountByGroup(int ClientID, int Group) const
{
	auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::count_if(pAchievements.begin(), pAchievements.end(), [Group](const CAchievement* p)
	{ return p->Info()->GetGroup() == Group && p->IsCompleted(); });
}

int CAchievementManager::GetCount() const { return (int)CAchievementInfo::Data().size(); }

int CAchievementManager::GetCompletedCount(int ClientID) const
{
	auto& pAchievements = CAchievement::Data()[ClientID];
	return (int)std::count_if(pAchievements.begin(), pAchievements.end(), [](const CAchievement* p)
	{ return p->IsCompleted(); });
}


void CAchievementManager::UpdateAchievement(CPlayer* pPlayer, int Type, int Misc, int Value, int ProgressType) const
{
	// check valid player
	if(!pPlayer || pPlayer->IsBot())
		return;

	// initialize variables
	bool Updated = false;
	auto& pAchievements = CAchievement::Data()[pPlayer->GetCID()];

	// search for the achievement
	for(auto& pAchievement : pAchievements)
	{
		// initialize variables
		const auto achievementType = pAchievement->Info()->GetType();
		const auto achievementMisc = pAchievement->Info()->GetMisc();

		if(achievementType == Type && achievementMisc == Misc && !pAchievement->IsCompleted())
		{
			if(pAchievement->UpdateProgress(Misc, Value, ProgressType))
				Updated = true;
		}
	}

	// update the achievement progress in the database
	if(Updated)
		GS()->Core()->SaveAccount(pPlayer, SAVE_ACHIEVEMENTS);
}
