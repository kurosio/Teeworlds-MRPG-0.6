/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_manager.h"

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
		std::string Description = pResult->getString("Description").c_str();
		std::string Data = pResult->getString("Data").c_str();

		// create element
		auto* pAchievement = CAchievementInfo::CreateElement(ID);
		pAchievement->Init(Name, Description, Type, Data);
	}
}

void CAchievementManager::OnResetClient(int ClientID)
{
	// free achievement data
	for(auto& p : CAchievement::Data()[ClientID])
		delete p.second;
	CAchievement::Data()[ClientID].clear();
}

bool CAchievementManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	return false;
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
    for(auto& p : pAchievements)
    {
		// initialize variables
        auto* pAchievement = p.second;
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

bool CAchievementManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	return false;
}