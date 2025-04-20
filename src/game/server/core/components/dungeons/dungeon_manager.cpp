#include "dungeon_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/worldmodes/dungeon.h>

void CDungeonManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		auto Level = pRes->getInt("Level");
		auto WaitingDoorPos = vec2(pRes->getInt("DoorX"), pRes->getInt("DoorY"));
		auto WorldID = pRes->getInt("WorldID");

		auto* pDungeon = CDungeonData::CreateElement(ID);
		pDungeon->Init(WaitingDoorPos, Level, WorldID);
	}
}

bool CDungeonManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == EMenuList::MENU_DUNGEONS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Dungeons Information");
		VInfo.Add("In this section you can choose a dungeon");
		VInfo.Add("View the fastest players on the passage");
		VoteWrapper::AddEmptyline(ClientID);

		// dungeon list
		VoteWrapper(ClientID).Add("\u262C Test mode dungeon");
		if(!ShowDungeonsList(pPlayer, true))
			VoteWrapper(ClientID).Add("No dungeons available at the moment!");
		VoteWrapper::AddEmptyline(ClientID);

		// inside menu
		ShowInsideDungeonMenu(pPlayer);

		// add backpage buttom
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

bool CDungeonManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	if(!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
		return false;

	const int ClientID = pPlayer->GetCID();

	// dungeon enter
	if(PPSTR(pCmd, "DUNGEON_JOIN") == 0)
	{
		// check valid dungeon
		auto* pDungeon = GetDungeonByID(Extra1);
		if(!pDungeon)
			return true;

		// avaialbe dungeon join
		if(!g_Config.m_SvAvailableDungeonJoin)
		{
			GS()->Chat(ClientID, "Dungeons disabled by server.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		// check equal player world
		if(GS()->IsPlayerInWorld(ClientID, pDungeon->GetWorldID()))
		{
			GS()->Chat(ClientID, "You are already in this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		// check is playing
		if(pDungeon->IsPlaying())
		{
			GS()->Chat(ClientID, "At the moment players are passing this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		// check valid level
		if(pPlayer->Account()->GetLevel() < pDungeon->GetLevel())
		{
			GS()->Chat(ClientID, "Your level is low to pass this dungeon!");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
			return true;
		}

		// information and join
		GS()->Chat(-1, "'{}' joined to Dungeon '{}'!", Server()->ClientName(ClientID), pDungeon->GetName());
		pPlayer->ChangeWorld(pDungeon->GetWorldID());
		return true;
	}

	//dungeon exit
	if(PPSTR(pCmd, "DUNGEON_EXIT") == 0)
	{
		const int LatestCorrectWorldID = Core()->AccountManager()->GetLastVisitedWorldID(pPlayer);
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return true;
	}

	return false;
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
		VDungeon.AddOption("DUNGEON_JOIN", pDungeon->GetID(), "Join dungeon {}", pDungeon->GetName());
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
	if(!pController || !pController->GetDungeon())
		return;

	// exit from dungeon
	const char* pDungeonName = pController->GetDungeon()->GetName();
	VoteWrapper(ClientID).AddOption("DUNGEON_EXIT", "Exit dungeon {} (warning)", pDungeonName);
	VoteWrapper::AddEmptyline(ClientID);
}

CDungeonData* CDungeonManager::GetDungeonByID(int DungeonID) const
{
	auto pDungeon = std::ranges::find_if(CDungeonData::Data(), [DungeonID](auto* pDungeon)
	{ return pDungeon->GetID() == DungeonID; });
	return pDungeon != CDungeonData::Data().end() ? *pDungeon : nullptr;
}

CDungeonData* CDungeonManager::GetDungeonByWorldID(int WorldID) const
{
	auto pDungeon = std::ranges::find_if(CDungeonData::Data(), [WorldID](auto* pDungeon)
	{ return pDungeon->GetWorldID() == WorldID; });
	return pDungeon != CDungeonData::Data().end() ? *pDungeon : nullptr;
}
