/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "DungeonManager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Accounts/AccountManager.h>

#include "game/server/worldmodes/dungeon.h"

void CDungeonManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		str_copy(CDungeonData::ms_aDungeon[ID].m_aName, pRes->getString("Name").c_str(), sizeof(CDungeonData::ms_aDungeon[ID].m_aName));
		CDungeonData::ms_aDungeon[ID].m_Level = pRes->getInt("Level");
		CDungeonData::ms_aDungeon[ID].m_DoorX = pRes->getInt("DoorX");
		CDungeonData::ms_aDungeon[ID].m_DoorY = pRes->getInt("DoorY");
		CDungeonData::ms_aDungeon[ID].m_RequiredQuestID = pRes->getInt("RequiredQuestID");
		CDungeonData::ms_aDungeon[ID].m_WorldID = pRes->getInt("WorldID");
		CDungeonData::ms_aDungeon[ID].m_IsStory = pRes->getBoolean("Story");
	}
}

bool CDungeonManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MenuList::MENU_DUNGEONS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		VoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Dungeons Information");
		VInfo.Add("In this section you can choose a dungeon");
		VInfo.Add("View the fastest players on the passage");

		VoteWrapper::AddLine(ClientID);
		VoteWrapper(ClientID).Add("\u262C Story dungeon's");
		if(!ShowDungeonsList(pPlayer, true))
			VoteWrapper(ClientID).Add("No dungeons available at the moment!");

		VoteWrapper::AddLine(ClientID);
		VoteWrapper(ClientID).Add("\u274A Alternative story dungeon's");
		if(!ShowDungeonsList(pPlayer, false))
			VoteWrapper(ClientID).Add("No dungeons available at the moment!");

		VoteWrapper::AddLine(ClientID);
		ShowInsideDungeonMenu(pPlayer);

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

bool CDungeonManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
		return false;

	if(PPSTR(CMD, "DUNGEONJOIN") == 0)
	{
		if(GS()->IsPlayerEqualWorld(ClientID, CDungeonData::ms_aDungeon[VoteID].m_WorldID))
		{
			GS()->Chat(ClientID, "You are already in this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}
		if(CDungeonData::ms_aDungeon[VoteID].IsDungeonPlaying())
		{
			GS()->Chat(ClientID, "At the moment players are passing this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		if(pPlayer->Account()->GetLevel() < CDungeonData::ms_aDungeon[VoteID].m_Level)
		{
			GS()->Chat(ClientID, "Your level is low to pass this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		if(!GS()->IsWorldType(WorldType::Dungeon))
		{
			pPlayer->GetTempData().SetTeleportPosition(pPlayer->GetCharacter()->m_Core.m_Pos);
			GS()->Core()->SaveAccount(pPlayer, SaveType::SAVE_POSITION);
		}

		GS()->Chat(-1, "{} joined to Dungeon {}!", Server()->ClientName(ClientID), CDungeonData::ms_aDungeon[VoteID].m_aName);
		GS()->Chat(ClientID, "You can vote for the choice of tank (Dungeon Tab)!");
		pPlayer->ChangeWorld(CDungeonData::ms_aDungeon[VoteID].m_WorldID);
		return true;
	}

	// dungeon exit
	else if(PPSTR(CMD, "DUNGEONEXIT") == 0)
	{
		const int LatestCorrectWorldID = Core()->AccountManager()->GetLastVisitedWorldID(pPlayer);
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return true;
	}

	return false;
}

bool CDungeonManager::IsDungeonWorld(int WorldID)
{
	return std::find_if(CDungeonData::ms_aDungeon.begin(), CDungeonData::ms_aDungeon.end(),
		[WorldID](const std::pair<int, CDungeonData>& pDungeon) { return pDungeon.second.m_WorldID == WorldID; }) != CDungeonData::ms_aDungeon.end();
}

void CDungeonManager::SaveDungeonRecord(CPlayer* pPlayer, int DungeonID, CPlayerDungeonRecord* pPlayerDungeonRecord)
{
	const int Seconds = pPlayerDungeonRecord->m_Time;
	const float PassageHelp = pPlayerDungeonRecord->m_PassageHelp;

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_records", "WHERE UserID = '%d' AND DungeonID = '%d'", pPlayer->Account()->GetID(), DungeonID);
	if(pRes->next())
	{
		if(pRes->getInt("Seconds") > Seconds && pRes->getInt("PassageHelp") < PassageHelp)
			Database->Execute<DB::UPDATE>("tw_dungeons_records", "Seconds = '%d', PassageHelp = '%f' WHERE UserID = '%d' AND DungeonID = '%d'",
				Seconds, PassageHelp, pPlayer->Account()->GetID(), DungeonID);
		return;
	}
	Database->Execute<DB::INSERT>("tw_dungeons_records", "(UserID, DungeonID, Seconds, PassageHelp) VALUES ('%d', '%d', '%d', '%f')", pPlayer->Account()->GetID(), DungeonID, Seconds, PassageHelp);
}

void CDungeonManager::InsertVotesDungeonTop(int DungeonID, VoteWrapper* pWrapper) const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_records", "WHERE DungeonID = '%d' ORDER BY Seconds ASC LIMIT 5", DungeonID);
	while(pRes->next())
	{
		const int Rank = pRes->getRow();
		const int UserID = pRes->getInt("UserID");
		const int BaseSeconds = pRes->getInt("Seconds");
		const int BasePassageHelp = pRes->getInt("PassageHelp");

		const int Minutes = BaseSeconds / 60;
		const int Seconds = BaseSeconds - (BaseSeconds / 60 * 60);
		pWrapper->Add("{}. {} | {}:{}min | {}P", Rank, Server()->GetAccountNickname(UserID), Minutes, Seconds, BasePassageHelp);
	}
}

bool CDungeonManager::ShowDungeonsList(CPlayer* pPlayer, bool Story) const
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();
	for(const auto& dungeon : CDungeonData::ms_aDungeon)
	{
		if(dungeon.second.m_IsStory != Story)
			continue;

		VoteWrapper VDungeon(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "Lvl{} {} : Players {} : {} [{}%]",
			dungeon.second.m_Level, dungeon.second.m_aName, dungeon.second.m_Players, 
			(dungeon.second.IsDungeonPlaying() ? "Active dungeon" : "Waiting players"), dungeon.second.m_Progress);

		InsertVotesDungeonTop(dungeon.first, &VDungeon);

		const int NeededQuestID = dungeon.second.m_RequiredQuestID;
		if(NeededQuestID <= 0 || pPlayer->GetQuest(NeededQuestID)->IsCompleted())
		{
			VDungeon.AddOption("DUNGEONJOIN", dungeon.first, "Join dungeon {}", dungeon.second.m_aName);
		}
		else
		{
			VDungeon.Add("Need to complete quest {}", pPlayer->GetQuest(NeededQuestID)->Info()->GetName());
		}
		Found = true;
	}

	return Found;
}

void CDungeonManager::ShowInsideDungeonMenu(CPlayer* pPlayer) const
{
	if(!GS()->IsWorldType(WorldType::Dungeon))
		return;

	const int ClientID = pPlayer->GetCID();
	CGameControllerDungeon* pController = (CGameControllerDungeon*)GS()->m_pController;
	int DungeonID = pController->GetDungeonID();

	// exit from dungeon
	VoteWrapper::AddLine(ClientID);
	VoteWrapper(ClientID).AddOption("DUNGEONEXIT", "Exit dungeon {} (warning)", CDungeonData::ms_aDungeon[DungeonID].m_aName);
}

void CDungeonManager::NotifyUnlockedDungeonsByQuest(CPlayer* pPlayer, int QuestID) const
{
	const int ClientID = pPlayer->GetCID();
	for(const auto& dungeon : CDungeonData::ms_aDungeon)
	{
		if(QuestID == dungeon.second.m_RequiredQuestID)
			GS()->Chat(-1, "{} opened dungeon ({})!", Server()->ClientName(ClientID), dungeon.second.m_aName);
	}
}