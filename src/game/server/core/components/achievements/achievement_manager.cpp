/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_manager.h"

#include <game/server/gamecontext.h>

void CAchievementManager::OnInit()
{
	ResultPtr pResult = Database->Execute<DB::SELECT>(TW_ACHIEVEMENTS, "SELECT * FROM `tw_achievements`");
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

void CAchievementManager::OnInitAccount(CPlayer* pPlayer)
{

}

bool CAchievementManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	return false;
}

bool CAchievementManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	return false;
}