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
		GS()->Chat(ClientID, "You can vote for the choice of tank (Dungeon Tab)!");
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
	if(!pController)
		return;

	int DungeonID = pController->GetDungeonID();
	auto* pDungeon = GetDungeonByID(DungeonID);
	if(!pDungeon)
		return;

	// exit from dungeon
	VoteWrapper::AddLine(ClientID);
	VoteWrapper(ClientID).AddOption("DUNGEON_EXIT", "Exit dungeon {} (warning)", pDungeon->GetName());
}

CDungeonData* CDungeonManager::GetDungeonByID(int DungeonID) const
{
	auto pDungeon = std::ranges::find_if(CDungeonData::Data(), [DungeonID](auto* pDungeon)
	{
		return pDungeon->GetID() == DungeonID;
	});

	return pDungeon != CDungeonData::Data().end() ? *pDungeon : nullptr;
}