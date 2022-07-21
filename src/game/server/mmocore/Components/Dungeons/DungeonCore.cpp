/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "DungeonCore.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Accounts/AccountCore.h>

void DungeonCore::OnInit()
{
	ResultPtr pRes = SJK.SD("*", "tw_dungeons");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		str_copy(CDungeonData::ms_aDungeon[ID].m_aName, pRes->getString("Name").c_str(), sizeof(CDungeonData::ms_aDungeon[ID].m_aName));
		CDungeonData::ms_aDungeon[ID].m_Level = pRes->getInt("Level");
		CDungeonData::ms_aDungeon[ID].m_DoorX = pRes->getInt("DoorX");
		CDungeonData::ms_aDungeon[ID].m_DoorY = pRes->getInt("DoorY");
		CDungeonData::ms_aDungeon[ID].m_RequiredQuestID = pRes->getInt("RequiredQuestID");
		CDungeonData::ms_aDungeon[ID].m_WorldID = pRes->getInt("WorldID");
		CDungeonData::ms_aDungeon[ID].m_IsStory = (bool)pRes->getBoolean("Story");
	}
}

bool DungeonCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		return false;
	}

	if(Menulist == MenuList::MENU_DUNGEONS)
	{
		pPlayer->m_LastVoteMenu = MenuList::MAIN_MENU;
		GS()->AVH(ClientID, TAB_INFO_DUNGEON, GREEN_COLOR, "Dungeons Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DUNGEON, "In this section you can choose a dungeon");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DUNGEON, "View the fastest players on the passage");
		GS()->AV(ClientID, "null");

		pPlayer->m_VoteColored = GOLDEN_COLOR;
		GS()->AVL(ClientID, "null", "Story dungeon's");
		ShowDungeonsList(pPlayer, true);
		GS()->AV(ClientID, "null");

		pPlayer->m_VoteColored = GOLDEN_COLOR;
		GS()->AVL(ClientID, "null", "Alternative story dungeon's");
		ShowDungeonsList(pPlayer, false);

		if(GS()->IsDungeon())
		{
			GS()->AV(ClientID, "null");
			ShowTankVotingDungeon(pPlayer);
			GS()->AV(ClientID, "null");
			pPlayer->m_VoteColored = { 30, 8, 8 };
			GS()->AVL(ClientID, "DUNGEONEXIT", "Exit dungeon {STR} (warning)", CDungeonData::ms_aDungeon[GS()->GetDungeonID()].m_aName);
		}
		GS()->AddVotesBackpage(ClientID);
		return true;
	}
	return false;
}

bool DungeonCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
		return false;

	if(PPSTR(CMD, "DUNGEONJOIN") == 0)
	{
		if(GS()->IsPlayerEqualWorldID(ClientID, CDungeonData::ms_aDungeon[VoteID].m_WorldID))
		{
			GS()->Chat(ClientID, "You are already in this dungeon!");
			GS()->StrongUpdateVotes(ClientID, MenuList::MENU_DUNGEONS);
			return true;
		}
		if(CDungeonData::ms_aDungeon[VoteID].IsDungeonPlaying())
		{
			GS()->Chat(ClientID, "At the moment players are passing this dungeon!");
			GS()->StrongUpdateVotes(ClientID, MenuList::MENU_DUNGEONS);
			return true;
		}

		if(pPlayer->Acc().m_Level < CDungeonData::ms_aDungeon[VoteID].m_Level)
		{
			GS()->Chat(ClientID, "Your level is low to pass this dungeon!");
			GS()->StrongUpdateVotes(ClientID, MenuList::MENU_DUNGEONS);
			return true;
		}

		if(!GS()->IsDungeon())
		{
			pPlayer->GetTempData().m_TempTeleportPos = pPlayer->GetCharacter()->m_Core.m_Pos;
			GS()->Mmo()->SaveAccount(pPlayer, SaveType::SAVE_POSITION);
		}

		GS()->Chat(-1, "{STR} joined to Dungeon {STR}!", Server()->ClientName(ClientID), CDungeonData::ms_aDungeon[VoteID].m_aName);
		GS()->Chat(ClientID, "You can vote for the choice of tank (Dungeon Tab)!");
		pPlayer->ChangeWorld(CDungeonData::ms_aDungeon[VoteID].m_WorldID);
		return true;
	}

	// dungeon exit
	else if(PPSTR(CMD, "DUNGEONEXIT") == 0)
	{
		const int LatestCorrectWorldID = Job()->Account()->GetHistoryLatestCorrectWorldID(pPlayer);
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return true;
	}

	// dungeon voting
	else if(PPSTR(CMD, "DUNGEONVOTE") == 0)
	{
		CPlayer* pSearchPlayer = GS()->GetPlayer(VoteID, true);
		if(!pSearchPlayer)
		{
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
			return true;
		}

		if(VoteID == ClientID)
		{
			GS()->Chat(ClientID, "You can't vote for yourself!");
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
			return true;
		}

		if(pPlayer->GetTempData().m_TempAlreadyVotedDungeon)
		{
			GS()->Chat(ClientID, "You already voted!");
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
			return true;
		}

		pPlayer->GetTempData().m_TempAlreadyVotedDungeon = true;
		pSearchPlayer->GetTempData().m_TempTankVotingDungeon++;
		GS()->ChatWorldID(pPlayer->GetPlayerWorldID(), "[Dungeon]", "{STR} voted for {STR}.", Server()->ClientName(ClientID), Server()->ClientName(VoteID));
		GS()->StrongUpdateVotesForAll(pPlayer->m_OpenVoteMenu);
		return true;
	}
	return false;
}

bool DungeonCore::IsDungeonWorld(int WorldID)
{
	return std::find_if(CDungeonData::ms_aDungeon.begin(), CDungeonData::ms_aDungeon.end(),
	                    [WorldID](const std::pair<int, CDungeonData>& pDungeon) { return pDungeon.second.m_WorldID == WorldID; }) != CDungeonData::ms_aDungeon.end();
}

void DungeonCore::SaveDungeonRecord(CPlayer* pPlayer, int DungeonID, CPlayerDungeonRecord *pPlayerDungeonRecord)
{
	const int Seconds = pPlayerDungeonRecord->m_Time;
	const float PassageHelp = pPlayerDungeonRecord->m_PassageHelp;

	ResultPtr pRes = SJK.SD("*", "tw_dungeons_records", "WHERE UserID = '%d' AND DungeonID = '%d'", pPlayer->Acc().m_UserID, DungeonID);
	if (pRes->next())
	{
		if (pRes->getInt("Seconds") > Seconds && pRes->getInt("PassageHelp") < PassageHelp)
			SJK.UD("tw_dungeons_records", "Seconds = '%d', PassageHelp = '%f' WHERE UserID = '%d' AND DungeonID = '%d'",
				Seconds, PassageHelp, pPlayer->Acc().m_UserID, DungeonID);
		return;
	}
	SJK.ID("tw_dungeons_records", "(UserID, DungeonID, Seconds, PassageHelp) VALUES ('%d', '%d', '%d', '%f')", pPlayer->Acc().m_UserID, DungeonID, Seconds, PassageHelp);
}

void DungeonCore::ShowDungeonTop(CPlayer* pPlayer, int DungeonID, int HideID) const
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = SJK.SD("*", "tw_dungeons_records", "WHERE DungeonID = '%d' ORDER BY Seconds ASC LIMIT 5", DungeonID);
	while (pRes->next())
	{
		const int Rank = pRes->getRow();
		const int UserID = pRes->getInt("UserID");
		const int BaseSeconds = pRes->getInt("Seconds");
		const int BasePassageHelp = pRes->getInt("PassageHelp");

		const int Minutes = BaseSeconds / 60;
		const int Seconds = BaseSeconds - (BaseSeconds / 60 * 60);
		GS()->AVM(ClientID, "null", NOPE, HideID, "{INT}. {STR} | {INT}:{INT}min | {INT}P", Rank, Job()->PlayerName(UserID), Minutes, Seconds, BasePassageHelp);
	}
}

void DungeonCore::ShowDungeonsList(CPlayer* pPlayer, bool Story) const
{
	const int ClientID = pPlayer->GetCID();
	for (const auto& dungeon : CDungeonData::ms_aDungeon)
	{
		if(dungeon.second.m_IsStory != Story)
			continue;

		const int HideID = 7500 + dungeon.first;
		GS()->AVH(ClientID, HideID, LIGHT_GOLDEN_COLOR, "Lvl{INT} {STR} : Players {INT} : {STR} [{INT}%]",
			dungeon.second.m_Level, dungeon.second.m_aName, dungeon.second.m_Players, (dungeon.second.IsDungeonPlaying() ? "Active dungeon" : "Waiting players"), dungeon.second.m_Progress);

		ShowDungeonTop(pPlayer, dungeon.first, HideID);

		const int NeededQuestID = dungeon.second.m_RequiredQuestID;
		if(NeededQuestID <= 0 || pPlayer->GetQuest(NeededQuestID).IsComplected())
			GS()->AVM(ClientID, "DUNGEONJOIN", dungeon.first, HideID, "Join dungeon {STR}", dungeon.second.m_aName);
		else
			GS()->AVM(ClientID, "null", NOPE, HideID, "Need to complete quest {STR}", pPlayer->GetQuest(NeededQuestID).Info().GetName());
	}
}

void DungeonCore::ShowTankVotingDungeon(CPlayer* pPlayer) const
{
	if(!GS()->IsDungeon())
		return;

	const int ClientID = pPlayer->GetCID();
	const int DungeonWorldID = CDungeonData::ms_aDungeon[GS()->GetDungeonID()].m_WorldID;
	pPlayer->m_VoteColored = GRAY_COLOR;
	GS()->AVL(ClientID, "null", "Voting for the choice of tank!");
	pPlayer->m_VoteColored = LIGHT_GRAY_COLOR;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pSearchPlayer = GS()->GetPlayer(i, true);
		if(!pSearchPlayer || pSearchPlayer->GetPlayerWorldID() != DungeonWorldID)
			continue;

		GS()->AVM(ClientID, "DUNGEONVOTE", i, NOPE, "Vote for {STR} (Votes: {INT})", Server()->ClientName(i), pSearchPlayer->GetTempData().m_TempTankVotingDungeon);
	}
}

void DungeonCore::CheckQuestingOpened(CPlayer *pPlayer, int QuestID) const
{
	const int ClientID = pPlayer->GetCID();
	for (const auto& dungeon : CDungeonData::ms_aDungeon)
	{
		if (QuestID == dungeon.second.m_RequiredQuestID)
			GS()->Chat(-1, "{STR} opened dungeon ({STR})!", Server()->ClientName(ClientID), dungeon.second.m_aName);
	}
}
