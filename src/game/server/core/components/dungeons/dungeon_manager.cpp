#include "dungeon_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/accounts/account_manager.h>
#include "game/server/worldmodes/dungeon.h"

void CDungeonManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		auto Name = pRes->getString("Name");
		auto Level = pRes->getInt("Level");
		auto DoorPos = vec2(pRes->getInt("DoorX"), pRes->getInt("DoorY"));
		auto WorldID = pRes->getInt("WorldID");

		auto* pDungeon = CDungeonData::CreateElement(ID);
		pDungeon->Init(DoorPos, Level, Name, WorldID);
	}
}

bool CDungeonManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == EMenuList::MENU_DUNGEONS)
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

bool CDungeonManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();
	if(!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
		return false;

/*	if(PPSTR(pCmd, "DUNGEONJOIN") == 0)
	{
		if(GS()->IsPlayerInWorld(ClientID, CDungeonData::ms_aDungeon[Extra1].m_WorldID))
		{
			GS()->Chat(ClientID, "You are already in this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}
		if(CDungeonData::ms_aDungeon[Extra1].IsDungeonPlaying())
		{
			GS()->Chat(ClientID, "At the moment players are passing this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		if(pPlayer->Account()->GetLevel() < CDungeonData::ms_aDungeon[Extra1].m_Level)
		{
			GS()->Chat(ClientID, "Your level is low to pass this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		if(!GS()->IsWorldType(WorldType::Dungeon))
		{
			pPlayer->GetTempData().SetSpawnPosition(pPlayer->GetCharacter()->m_Core.m_Pos);
			GS()->Core()->SaveAccount(pPlayer, ESaveType::SAVE_POSITION);
		}

		GS()->Chat(-1, "'{}' joined to Dungeon '{}'!", Server()->ClientName(ClientID), CDungeonData::ms_aDungeon[Extra1].m_aName);
		GS()->Chat(ClientID, "You can vote for the choice of tank (Dungeon Tab)!");
		pPlayer->ChangeWorld(CDungeonData::ms_aDungeon[Extra1].m_WorldID);
		return true;
	}

	// dungeon exit
	else if(PPSTR(pCmd, "DUNGEONEXIT") == 0)
	{
		const int LatestCorrectWorldID = Core()->AccountManager()->GetLastVisitedWorldID(pPlayer);
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return true;
	}*/

	return false;
}

void CDungeonManager::InsertVotesDungeonTop(int DungeonID, VoteWrapper* pWrapper) const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_records", "WHERE DungeonID = '{}' ORDER BY Lifetime ASC LIMIT 5", DungeonID);
	while(pRes->next())
	{
		const int Rank = pRes->getRow();
		const int UserID = pRes->getInt("UserID");
		const int BaseSeconds = pRes->getInt("Lifetime");
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
	for(const auto* pDungeon : CDungeonData::Data())
	{
		VoteWrapper VDungeon(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "Lvl{} {} : Players {} : {} [{}%]",
			pDungeon->GetLevel(), pDungeon->GetName(), pDungeon->GetPlayersNum(),
			(pDungeon->IsPlaying() ? "Active dungeon" : "Waiting players"), pDungeon->GetProgress());

		InsertVotesDungeonTop(pDungeon->GetID(), &VDungeon);
		VDungeon.AddOption("DUNGEONJOIN", pDungeon->GetID(), "Join dungeon {}", pDungeon->GetName());
		Found = true;
	}

	return Found;
}

void CDungeonManager::ShowInsideDungeonMenu(CPlayer* pPlayer) const
{
	if(!GS()->IsWorldType(WorldType::Dungeon))
		return;

	const int ClientID = pPlayer->GetCID();
	const auto* pController = (CGameControllerDungeon*)GS()->m_pController;
	if(!pController)
		return;

	int DungeonID = pController->GetDungeonID();
	auto* pDungeon = GetDungeonByID(DungeonID);
	if(!pDungeon)
		return;

	// exit from dungeon
	VoteWrapper::AddLine(ClientID);
	VoteWrapper(ClientID).AddOption("DUNGEONEXIT", "Exit dungeon {} (warning)", pDungeon->GetName());
}

CDungeonData* CDungeonManager::GetDungeonByID(int DungeonID) const
{
	auto pDungeon = std::ranges::find_if(CDungeonData::Data(), [DungeonID](auto* pDungeon)
	{
		return pDungeon->GetID() == DungeonID;
	});

	return pDungeon != CDungeonData::Data().end() ? *pDungeon : nullptr;
}