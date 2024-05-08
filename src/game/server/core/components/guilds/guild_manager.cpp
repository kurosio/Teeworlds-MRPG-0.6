/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_manager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include "entities/guild_door.h"
#include <game/server/core/components/Inventory/InventoryManager.h>
#include <game/server/core/components/mails/mail_wrapper.h>

void CGuildManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_TABLE);
	while(pRes->next())
	{
		GuildIdentifier ID = pRes->getInt("ID");
		std::string Name = pRes->getString("Name").c_str();
		std::string JsonMembers = pRes->getString("Members").c_str();
		GuildRankIdentifier DefaultRankID = pRes->getInt("DefaultRankID");
		int LeaderUID = pRes->getInt("LeaderUID");
		int Level = pRes->getInt("Level");
		int Experience = pRes->getInt("Experience");
		int Bank = pRes->getInt("Bank");
		int Score = pRes->getInt("Score");
		int64_t LogFlag = pRes->getInt64("LogFlag");

		CGuild::CreateElement(ID)->Init(Name, std::move(JsonMembers), DefaultRankID, Level, Experience, Score, LeaderUID, Bank, LogFlag, &pRes);
	}

	InitWars();
	Core()->ShowLoadingProgress("Guilds", CGuild::Data().size());
}

void CGuildManager::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HOUSES, pWhereLocalWorld);
	while(pRes->next())
	{
		GuildHouseIdentifier ID = pRes->getInt("ID");
		GuildIdentifier GuildID = pRes->getInt("GuildID");
		int Price = pRes->getInt("Price");
		std::string JsonDoors = pRes->getString("Doors").c_str();
		std::string JsonPlantzones = pRes->getString("Plantzones").c_str();
		std::string JsonPropersties = pRes->getString("Properties").c_str();

		CGuild* pGuild = GetGuildByID(GuildID);
		CGuildHouse::CreateElement(ID)->Init(pGuild, Price, GS()->GetWorldID(), std::move(JsonDoors), std::move(JsonPlantzones), std::move(JsonPropersties));
	}

	Core()->ShowLoadingProgress("Guild houses", CGuildHouse::Data().size());
}

void CGuildManager::OnTick()
{
	// Check if the current world ID is not equal to the main world (once use House get instance object self world id) ID and current tick
	if(GS()->GetWorldID() != MAIN_WORLD_ID || (Server()->Tick() % Server()->TickSpeed() != 0))
		return;

	// Loop through all guild war handlers in the CGuildWarHandler::Data() container
	for(auto& pWarHandler : CGuildWarHandler::Data())
		pWarHandler->Tick();

	// Calculate the remaining lifetime of a text update
	int LifeTime = (Server()->TickSpeed() * 10);

	// Get the house data
	const auto& HouseData = CGuildHouse::Data();
	for(const auto& p : HouseData)
	{
		// Update the text with the remaining lifetime
		p->TextUpdate(LifeTime);
	}
}

bool CGuildManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_GUILD_HOUSE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_GUILD_HOUSE_PURCHASE_INFO);
		return true;
	}
	if(pChr->GetHelper()->TileExit(IndexCollision, TILE_GUILD_HOUSE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_GUILD_CHAIR))
	{
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_GUILD_CHAIR))
	{
		return true;
	}
	if(pChr->GetHelper()->BoolIndex(TILE_GUILD_CHAIR))
	{
		if(Server()->Tick() % (Server()->TickSpeed() * 5) == 0)
		{
			//const int HouseID = GetPosHouseID(pChr->m_Core.m_Pos);
			//const int GuildID = GetHouseGuildID(HouseID);
			//if(HouseID <= 0 || GuildID <= 0)
			//	return true;

			//const int Exp = CGuild::ms_aGuild[GuildID].m_UpgradesData(CGuild::CHAIR_EXPERIENCE, 0).m_Value;
			//pPlayer->AccountManager()->AddExperience(Exp);
		}
		return true;
	}

	return false;
}

bool CGuildManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// teleport to house
	if(PPSTR(CMD, "GUILD_HOUSE_SPAWN") == 0)
	{
		// check guild valid
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild)
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check guild house valid
		auto* pHouse = pGuild->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// Check if the player is in a different world than pAether
		if(!GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
		{
			// Change the player's world to pAether's world
			pPlayer->GetTempData().SetTeleportPosition(pHouse->GetPos());
			pPlayer->ChangeWorld(pHouse->GetWorldID());
			return true;
		}

		// Change the player's position to pAether's position
		pPlayer->GetCharacter()->ChangePosition(pHouse->GetPos());
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// start house decoration edit
	if(PPSTR(CMD, "GUILD_HOUSE_DECORATION_EDIT") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check house valid
		auto* pHouse = pGuild->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// result
		const bool Result = pHouse->GetDecorationManager()->StartDrawing(pPlayer);
		if(Result)
			GS()->Chat(ClientID, "You can now draw decorations.");
		else
			GS()->Chat(ClientID, "You can't draw decorations.");
		return true;
	}

	// set leader
	if(PPSTR(CMD, "GUILD_SET_LEADER") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		switch(pGuild->SetLeader(VoteID))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::SET_LEADER_NON_GUILD_PLAYER: GS()->Chat(ClientID, "The player is not a member of your guild"); break;
			case GuildResult::SET_LEADER_PLAYER_ALREADY_LEADER: GS()->Chat(ClientID, "The player is already a leader"); break;
			case GuildResult::SUCCESSFUL: 
				GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP_LIST);
				pPlayer->m_VotesData.UpdateCurrentVotes();
			break;
		}
		return true;
	}

	// change member rank
	if(PPSTR(CMD, "GUILD_CHANGE_MEMBER_RANK") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// initialize variables
		const int& MemberUID = VoteID;
		const GuildRankIdentifier& RankID = VoteID2;
		auto pMember = pGuild->GetMembers()->Get(MemberUID);

		// check member valid and result from setrank
		if(!pMember || !pMember->SetRank(RankID))
		{
			GS()->Chat(ClientID, "Set a player's rank failed, try again later");
			return true;
		}

		// succesful
		GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP_LIST);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// disband
	if(PPSTR(CMD, "GUILD_DISBAND") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// prevent accidental pressing
		if(Get != 55428)
		{
			GS()->Chat(ClientID, "Random Touch Security Code has not been entered correctly.");
			return true;
		}

		// disband guild
		Disband(pPlayer->Account()->GetGuild()->GetID());
		return true;
	}

	// kick member
	if(PPSTR(CMD, "GUILD_KICK_MEMBER") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check exclude oneself
		if(pPlayer->Account()->GetID() == VoteID)
		{
			GS()->Chat(ClientID, "You can't kick yourself");
			return true;
		}

		// result
		switch(pPlayer->Account()->GetGuild()->GetMembers()->Kick(VoteID))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::MEMBER_KICK_IS_OWNER: GS()->Chat(ClientID, "You can't kick a leader"); break;
			case GuildResult::MEMBER_KICK_DOES_NOT_EXIST: GS()->Chat(ClientID, "The player is no longer on the guild membership lists"); break;
			case GuildResult::MEMBER_SUCCESSFUL: 
				GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP_LIST);
				pPlayer->m_VotesData.UpdateVotes(MENU_GUILD_MEMBERSHIP_LIST);
			break;
		}
		return true;
	}

	// deposit gold
	if(PPSTR(CMD, "GUILD_DEPOSIT_GOLD") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild)
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// minimal 100
		if(Get < 100)
		{
			GS()->Chat(ClientID, "Minimum is 100 gold.");
			return true;
		}

		// check member valid
		auto pMember = pPlayer->Account()->GetGuildMember();
		if(!pMember)
		{
			GS()->Chat(ClientID, "Unforeseen error");
			return true;
		}

		// deposit gold
		if(pMember->DepositInBank(Get))
			pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD);
		return true;
	}

	// create new rank
	if(PPSTR(CMD, "GUILD_RANK_CREATE") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check valid text from reason
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// result
		switch(pGuild->GetRanks()->Add(GetText))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::RANK_ADD_ALREADY_EXISTS: GS()->Chat(ClientID, "The rank name already exists"); break;
			case GuildResult::RANK_ADD_LIMIT_HAS_REACHED: GS()->Chat(ClientID, "Rank limit reached, {} out of {}", (int)GUILD_RANKS_MAX_COUNT, (int)GUILD_RANKS_MAX_COUNT); break;
			case GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME: GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16."); break;
			case GuildResult::RANK_SUCCESSFUL:
				GS()->Chat(ClientID, "The rank '{}' has been successfully added!", GetText);
				GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_LIST);
			break;
		}
		return true;
	}

	// rename rank
	if(PPSTR(CMD, "GUILD_RANK_RENAME") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check valid text from reason
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// check rank valid
		auto pRank = pGuild->GetRanks()->Get(VoteID);
		if(!pRank)
		{
			GS()->Chat(ClientID, "Unforeseen error.");
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// result
		switch(pRank->Rename(GetText))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::RANK_RENAME_ALREADY_NAME_EXISTS: GS()->Chat(ClientID, "The name is already in use by another rank"); break;
			case GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME: GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16."); break;
			case GuildResult::RANK_SUCCESSFUL: GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_SELECTED); break;
		}
		return true;
	}

	// remove rank
	if(PPSTR(CMD, "GUILD_RANK_REMOVE") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check rank valid
		auto pRank = pGuild->GetRanks()->Get(VoteID);
		if(!pRank)
		{
			GS()->Chat(ClientID, "Unforeseen error.");
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// result
		switch(pGuild->GetRanks()->Remove(pRank->GetName()))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::RANK_REMOVE_IS_DEFAULT: GS()->Chat(ClientID, "You can't remove default rank"); break;
			case GuildResult::RANK_REMOVE_DOES_NOT_EXIST: GS()->Chat(ClientID, "There is no such rank"); break;
			case GuildResult::RANK_SUCCESSFUL:
				pPlayer->m_VotesData.UpdateVotes(MENU_GUILD_RANK_LIST);
				GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_SELECTED);
			break;
		}
		return true;
	}

	// set rights for rank
	if(PPSTR(CMD, "GUILD_RANK_SET_RIGHTS") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check rank valid
		auto pRank = pGuild->GetRanks()->Get(VoteID);
		if(!pRank)
		{
			GS()->Chat(ClientID, "Unforeseen error.");
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// check for same of rights
		const auto Rights = static_cast<GuildRankRights>(VoteID2);
		if(pRank->GetRights() == Rights)
		{
			GS()->Chat(ClientID, "You already have current rights set.");
			return true;
		}

		// update rights
		pRank->SetRights(Rights);
		GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_SELECTED);
		return true;
	}

	// request accept
	if(PPSTR(CMD, "GUILD_REQUESTS_ACCEPT") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		switch(pGuild->GetMembers()->GetRequests()->Accept(VoteID, pPlayer->Account()->GetGuildMember()))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::MEMBER_JOIN_ALREADY_IN_GUILD: GS()->Chat(ClientID, "The player is already in a guild"); break;
			case GuildResult::MEMBER_NO_AVAILABLE_SLOTS: GS()->Chat(ClientID, "No guild slots available."); break;
			case GuildResult::MEMBER_SUCCESSFUL:
				GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP_LIST);
				GS()->UpdateVotesIfForAll(MENU_GUILD_INVITES);
			break;
		}
		return true;
	}

	// request deny
	if(PPSTR(CMD, "GUILD_REQUESTS_DENY") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// deny the request
		pGuild->GetMembers()->GetRequests()->Deny(VoteID, pPlayer->Account()->GetGuildMember());
		GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP_LIST);
		GS()->UpdateVotesIfForAll(MENU_GUILD_INVITES);
		return true;
	}

	// field search guilds
	if(PPSTR(CMD, "GUILD_FINDER_SEARCH_FIELD") == 0)
	{
		// check text valid
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// update search buffer and reset votes
		str_copy(pPlayer->GetTempData().m_aGuildSearchBuf, GetText, sizeof(pPlayer->GetTempData().m_aGuildSearchBuf));
		pPlayer->m_VotesData.UpdateVotes(MENU_GUILD_FINDER);
		return true;
	}

	// send request
	if(PPSTR(CMD, "GUILD_SEND_REQUEST") == 0)
	{
		// check guild if have
		if(pPlayer->Account()->HasGuild())
		{
			GS()->Chat(ClientID, "You're already in a guild.");
			return true;
		}

		// initialize variables
		const GuildIdentifier& ID = VoteID;
		const int& AccountID = VoteID2;

		// check guild valid
		auto* pGuild = GetGuildByID(ID);
		if(!pGuild)
		{
			GS()->Chat(ClientID, "Unforeseen error.");
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// result
		switch(pGuild->GetMembers()->GetRequests()->Request(AccountID))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::MEMBER_REQUEST_ALREADY_SEND: GS()->Chat(ClientID, "You have already sent a request to this guild."); break;
			case GuildResult::MEMBER_NO_AVAILABLE_SLOTS: GS()->Chat(ClientID, "No guild slots available."); break;
			case GuildResult::MEMBER_SUCCESSFUL: GS()->Chat(ClientID, "You sent a request to join the {} guild.", pGuild->GetName()); break;
		}
		return true;
	}

	// buy house
	if(PPSTR(CMD, "GUILD_HOUSE_BUY") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		switch(pGuild->BuyHouse(VoteID))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::BUY_HOUSE_ALREADY_HAVE: GS()->Chat(ClientID, "Your guild already has a house."); break;
			case GuildResult::BUY_HOUSE_ALREADY_PURCHASED: GS()->Chat(ClientID, "This guild house has already been purchased."); break;
			case GuildResult::BUY_HOUSE_NOT_ENOUGH_GOLD: GS()->Chat(ClientID, "Your guild doesn't have enough gold."); break;
			case GuildResult::BUY_HOUSE_UNAVAILABLE: GS()->Chat(ClientID, "This guild house is not available for purchase."); break;
			case GuildResult::SUCCESSFUL: 
				pPlayer->m_VotesData.UpdateCurrentVotes();
				GS()->UpdateVotesIfForAll(MENU_GUILD);
			break;
		}
		return true;
	}

	// declare guild war
	if(PPSTR(CMD, "GUILD_DECLARE_WAR") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check if war and self it's same guild
		if(pGuild->GetID() == VoteID)
		{
			GS()->Chat(ClientID, "You can't declare war on your own guild.");
			return true;
		}

		// check war guild valid
		CGuild* pWarGuild = GetGuildByID(VoteID);
		if(!pWarGuild)
		{
			GS()->Chat(ClientID, "This guild cannot be declared war at this time.");
			return true;
		}

		// result
		if(pGuild->StartWar(pWarGuild))
		{
			dbg_msg("guild", "war created");
		}
		else
		{
			dbg_msg("guild", "war not created");
		}

		GS()->UpdateVotesIfForAll(MENU_GUILD_WARS);
		return true;
	}

	// try plant to house
	if(PPSTR(CMD, "GUILD_HOUSE_PLANT_ZONE_TRY") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check house valid
		auto* pHouse = pGuild->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// initialize variables
		const int& Useds = maximum(1, Get);
		const int& PlantzoneID = VoteID;
		const ItemIdentifier& ItemID = VoteID2;

		// check plantzone valid
		auto pPlantzone = pHouse->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID);
		if(!pPlantzone)
		{
			GS()->Chat(ClientID, "Plant zone not found.");
			return true;
		}

		// check is same plant item with current
		if(ItemID == pPlantzone->GetItemID())
		{
			GS()->Chat(ClientID, "This plant is already planted.");
			return true;
		}

		// check spend currency (planting item)
		if(pPlayer->Account()->SpendCurrency(Useds, ItemID))
		{
			// Check if the chance result is successful
			bool Success = false;
			Chance result(s_GuildChancePlanting);
			for(int i = 0; i < Useds && !Success; i++)
			{
				Success = result() ? true : Success;
				result.Update();
			}

			// result
			if(Success)
			{
				// Change the item in the plant zone to the planted item
				GS()->Chat(ClientID, "You have successfully planted the plant.");
				pPlantzone->ChangeItem(ItemID);
			}
			else
			{
				GS()->Chat(ClientID, "You failed to plant the plant.");
			}
			pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD_HOUSE_PLANTZONE_SELECTED);
		}

		return true;
	}

	// house door
	if(PPSTR(CMD, "GUILD_HOUSE_DOOR") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check house valid
		auto* pHouse = pGuild->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// reverse door house
		int UniqueDoorID = VoteID;
		pHouse->GetDoorManager()->Reverse(UniqueDoorID);
		GS()->UpdateVotesIfForAll(MENU_GUILD_HOUSE_DOORS);
		return true;
	}

	// house sell
	if(PPSTR(CMD, "GUILD_HOUSE_SELL") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// prevent accidental pressing
		if(Get != 3342)
		{
			GS()->Chat(ClientID, "Random Touch Security Code has not been entered correctly.");
			return true;
		}

		// result
		if(pGuild->SellHouse())
		{
			pPlayer->m_VotesData.UpdateVotes(MENU_GUILD);
			GS()->UpdateVotesIfForAll(MENU_GUILD);
		}
	}

	// upgrade guild
	if(PPSTR(CMD, "GUILD_UPGRADE") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		GuildUpgrade UpgrID = static_cast<GuildUpgrade>(VoteID);
		if(!pGuild->Upgrade(UpgrID))
		{
			GS()->Chat(ClientID, "Your guild does not have enough gold, or the maximum upgrade level has been reached.");
			return true;
		}

		GS()->UpdateVotesIfForAll(MENU_GUILD_UPGRADES);
		return true;
	}

	// logger set activity
	if(PPSTR(CMD, "GUILD_LOGGER_SET") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		pGuild->GetLogger()->SetActivityFlag(VoteID);
		GS()->UpdateVotesIfForAll(MENU_GUILD_LOGS);
		return true;
	}

	return false;
}

bool CGuildManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_GUILD_FINDER)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowFinder(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_FINDER_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_FINDER);
		ShowFinderDetail(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_UPGRADES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowUpgrades(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	
	if(Menulist == MENU_GUILD_DISBAND)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDisband(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_HOUSE_SELL)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowHouseSell(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_HOUSE_PURCHASE_INFO)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		CCharacter* pChr = pPlayer->GetCharacter();
		CGuildHouse* pHouse = GetGuildHouseByPos(pChr->m_Core.m_Pos);
		ShowBuyHouse(ClientID, pHouse);
		return true;
	}

	if(Menulist == MENU_GUILD_LOGS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowLogs(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_WARS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDeclareWar(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_INVITES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowRequests(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// doors-related menus
	if(Menulist == MENU_GUILD_HOUSE_DOORS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDoorsControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// membership-related menus
	if(Menulist == MENU_GUILD_MEMBERSHIP_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowMembershipList(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_MEMBERSHIP_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_MEMBERSHIP_LIST);
		ShowMembershipEdit(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// rank-related menus
	if(Menulist == MENU_GUILD_RANK_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowRanksList(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_RANK_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_RANK_LIST);
		ShowRankEdit(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// plantzones-related menus
	if(Menulist == MENU_GUILD_HOUSE_PLANTZONE_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowPlantzonesControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_HOUSE_PLANTZONE_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_HOUSE_PLANTZONE_LIST);
		ShowPlantzoneEdit(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

void CGuildManager::OnHandleTimePeriod(TIME_PERIOD Period)
{
	// Call the TimePeriodEvent function for each guild passing the Period parameter
	for(auto& pGuild : CGuild::Data())
		pGuild->TimePeriodEvent(Period);
}

void CGuildManager::InitWars() const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_WARS_TABLE);
	if(pRes->next())
	{
		// Get the ID of the guild that is not the current guild
		const int GuildID1 = pRes->getInt("GuildID1");
		const int GuildID2 = pRes->getInt("GuildID1");

		// Get scores
		const int Score1 = pRes->getInt("Score1");
		const int Score2 = pRes->getInt("Score2");

		// Get time until
		const time_t TimeUntilEnd = pRes->getInt64("TimeUntilEnd");

		// Get the pointer to the guild data
		CGuild* pGuild1 = GetGuildByID(GuildID1);
		CGuild* pGuild2 = GetGuildByID(GuildID2);
		dbg_assert(pGuild1 != nullptr, "GUILD_WAR - guild data is empty");
		dbg_assert(pGuild2 != nullptr, "GUILD_WAR - guild data is empty");

		// Create instance
		CGuildWarHandler* pWarHandler = CGuildWarHandler::CreateElement();
		pWarHandler->Init({ pGuild1, pGuild2, Score1 }, { pGuild2, pGuild1, Score2 }, TimeUntilEnd);
	}
}

void CGuildManager::Create(CPlayer* pPlayer, const char* pGuildName) const
{
	if(!pPlayer)
		return;

	// check whether we are already in the guild
	const int ClientID = pPlayer->GetCID();
	if(pPlayer->Account()->HasGuild())
	{
		GS()->Chat(ClientID, "You already in guild group!");
		return;
	}

	// we check the availability of the guild's name
	CSqlString<64> GuildName(pGuildName);
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", TW_GUILDS_TABLE, "WHERE Name = '%s'", GuildName.cstr());
	if(pRes->next())
	{
		GS()->Chat(ClientID, "This guild name already useds!");
		return;
	}

	// we check the ticket, we take it and create
	if(!pPlayer->GetItem(itTicketGuild)->HasItem() || !pPlayer->GetItem(itTicketGuild)->Remove(1))
	{
		GS()->Chat(ClientID, "You need first buy guild ticket on shop!");
		return;
	}

	// get ID for initialization
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", TW_GUILDS_TABLE, "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // TODO: thread save ? hm need for table all time auto increment = 1; NEED FIX IT -- use some kind of uuid

	// initialize the guild
	std::string MembersData = R"({"members":[{"id":)" + std::to_string(pPlayer->Account()->GetID()) + R"(,"rank_id":0,"deposit":"0"}]})";

	CGuild* pGuild = CGuild::CreateElement(InitID);
	pGuild->Init(GuildName.cstr(), std::forward<std::string>(MembersData), -1, 1, 0, 0, pPlayer->Account()->GetID(), 0, -1, nullptr);
	pPlayer->Account()->ReinitializeGuild();

	// we create a guild in the table
	Database->Execute<DB::INSERT>(TW_GUILDS_TABLE, "(ID, Name, LeaderUID, Members) VALUES ('%d', '%s', '%d', '%s')",
		InitID, GuildName.cstr(), pPlayer->Account()->GetID(), MembersData.c_str());
	GS()->Chat(-1, "New guilds [{}] have been created!", GuildName.cstr());
	pPlayer->m_VotesData.UpdateVotesIf(MENU_MAIN);
}

void CGuildManager::Disband(GuildIdentifier ID) const
{
	// Find the guild with the given ID
	auto pIterGuild = std::find_if(CGuild::Data().begin(), CGuild::Data().end(), [&ID](CGuild* pGuild) { return pGuild->GetID() == ID; });
	if(!(*pIterGuild))
		return;

	// Get a pointer to the guild
	CGuild* pGuild = (*pIterGuild);

	// End guild wars for disbanded guild
	if(pGuild->GetWar() && pGuild->GetWar()->GetHandler())
	{
		pGuild->GetWar()->GetHandler()->End();

	}

	// If the guild has a house, sell it
	if(pGuild->SellHouse())
	{
		GS()->Chat(-1, "The guild {} has lost house.", pGuild->GetName());
	}

	// Calculate the amount of gold to return to the guild leader
	const int ReturnsGold = maximum(1, pGuild->GetBank()->Get());

	// Send mail
	MailWrapper Mail("System", pGuild->GetLeaderUID(), "Your guild was disbanded.");
	Mail.AddDescLine("We returned some gold from your guild.");
	Mail.AttachItem(CItem(itGold, ReturnsGold));
	Mail.Send();
	GS()->Chat(-1, "The {} guild has been disbanded.", pGuild->GetName());

	// Remove all guild-related entries from the database
	Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '%d'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_RANKS_TABLE, "WHERE GuildID = '%d'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_TABLE, "WHERE ID = '%d'", pGuild->GetID());

	// Delete the guild object and remove it from the guild data container
	if(pIterGuild != CGuild::Data().end())
	{
		delete (*pIterGuild);
		CGuild::Data().erase(pIterGuild);
		pGuild = nullptr;
	}
}

void CGuildManager::ShowMenu(int ClientID) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// If the player is not in a guild
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	bool HasHouse = pGuild->HasHouse();
	int ExpNeed = computeExperience(pGuild->GetLevel());
	const int MemberUsedSlots = pGuild->GetMembers()->GetContainer().size();
	const int MemberMaxSlots = pGuild->GetUpgrades(GuildUpgrade::AVAILABLE_SLOTS)->m_Value;

	// Guild information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE|VWF_STYLE_STRICT_BOLD, "\u2747 Information about {}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VInfo.Add("Level: {} Experience: {}/{}", pGuild->GetLevel(), pGuild->GetExperience(), ExpNeed);
	VInfo.Add("Members: {} of {}", MemberUsedSlots, MemberMaxSlots);
	VInfo.Add("Bank: {} golds", pGuild->GetBank()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// Guild management
	VoteWrapper VManagement(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "\u262B Guild Management");
	VManagement.AddMenu(MENU_GUILD_UPGRADES, "Improvements & Upgrades");
	VManagement.AddMenu(MENU_GUILD_MEMBERSHIP_LIST, "Membership list");
	VManagement.AddMenu(MENU_GUILD_INVITES, "Membership requests");
	VManagement.AddMenu(MENU_GUILD_RANK_LIST, "Rank management");
	VManagement.AddMenu(MENU_GUILD_LOGS, "Logs of activity");
	VManagement.AddMenu(MENU_GUILD_WARS, "Guild wars");
	VManagement.AddMenu(MENU_GUILD_DISBAND, "Disband");
	VoteWrapper::AddEmptyline(ClientID);

	// Guild append house menu
	if(HasHouse)
	{
		// initialize variables
		char aBufTimeStamp[64]{};
		auto* pHouse = pGuild->GetHouse();

		// House management
		VoteWrapper VHouse(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "\u2302 House Management");
		VHouse.Add("Rent price per day: {} golds", pHouse->GetRentPrice());
		pHouse->GetRentTimeStamp(aBufTimeStamp, sizeof(aBufTimeStamp));
		VHouse.Add("Approximate rental time: {}", aBufTimeStamp);
		VHouse.AddOption("GUILD_HOUSE_DECORATION_EDIT", "Decoration editor");
		VHouse.AddMenu(MENU_GUILD_HOUSE_DOORS, "Doors control");
		VHouse.AddMenu(MENU_GUILD_HOUSE_PLANTZONE_LIST, "Plant zones");
		VHouse.AddOption("GUILD_HOUSE_SPAWN", "Move to the house");
		VHouse.AddMenu(MENU_GUILD_HOUSE_SELL, "Sell");
		VoteWrapper::AddEmptyline(ClientID);
	}

	// Guild deposit
	VoteWrapper VBank(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "\u2727 Bank Management");
	VBank.Add("Your: {} | Bank: {} golds", pPlayer->GetItem(itGold)->GetValue(), pGuild->GetBank()->Get());
	VBank.AddOption("GUILD_DEPOSIT_GOLD", "Deposit. (Amount in a reason)");
}

void CGuildManager::ShowUpgrades(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Guild upgrades (Information)");
	VInfo.Add("All improvements are solely related to the guild itself.");
	VInfo.Add("Bank: {}", pGuild->GetBank()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// guild-related upgrades
	VoteWrapper VUpgr(ClientID, VWF_STYLE_SIMPLE, "\u2730 Guild-related upgrades");
	for(int i = (int)GuildUpgrade::START_GUILD_UPGRADES; i < (int)GuildUpgrade::END_GUILD_UPGRADES; i++)
	{
		int Price = pGuild->GetUpgradePrice(static_cast<GuildUpgrade>(i));
		const auto* pUpgrade = pGuild->GetUpgrades(static_cast<GuildUpgrade>(i));
		VUpgr.AddOption("GUILD_UPGRADE", i, "Upgrade {} ({}) {}gold", pUpgrade->getDescription(), pUpgrade->m_Value, Price);
	}
	VoteWrapper::AddEmptyline(ClientID);

	// house-related upgrades
	if(pGuild->HasHouse())
	{
		VoteWrapper VUpgrHouse(ClientID, VWF_STYLE_SIMPLE, "\u2725 House-related upgrades");
		for(int i = (int)GuildUpgrade::START_GUILD_HOUSE_UPGRADES; i < (int)GuildUpgrade::END_GUILD_HOUSE_UPGRADES; i++)
		{
			int Price = pGuild->GetUpgradePrice(static_cast<GuildUpgrade>(i));
			const auto* pUpgrade = pGuild->GetUpgrades(static_cast<GuildUpgrade>(i));
			VUpgrHouse.AddOption("GUILD_UPGRADE", i, "Upgrade {} ({}) {}gold", pUpgrade->getDescription(), pUpgrade->m_Value, Price);
		}
		VoteWrapper::AddEmptyline(ClientID);
	}
}

void CGuildManager::ShowDisband(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Guild disbandment (Information)");
	VInfo.Add("Gold spent on upgrades will not be refunded.");
	VInfo.Add("The gold will be returned to the leader.");
	VoteWrapper::AddEmptyline(ClientID);

	// disband
	VoteWrapper(ClientID).AddOption("GUILD_DISBAND", "Disband. (in reason send 55428)");
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowHouseSell(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pHouse = pGuild->GetHouse();
	if(!pHouse)
		return;

	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Selling a house (Information)");
	VInfo.Add("The gold will be returned to the leader.");
	VoteWrapper::AddEmptyline(ClientID);

	// disband
	VoteWrapper(ClientID).AddOption("GUILD_HOUSE_SELL", "Sell. (in reason send 3342)");
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowMembershipList(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	auto pSelfMember = pPlayer->Account()->GetGuildMember();
	auto CurrentSlots = pGuild->GetMembers()->GetCurrentSlots();

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2324 Membership information", pGuild->GetName());
	VInfo.Add("Guild name: {}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VoteWrapper::AddEmptyline(ClientID);

	// list
	int Position = 1;
	VoteWrapper VList(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "List of membership ({} of {})", CurrentSlots.first, CurrentSlots.second);
	for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
	{
		auto pMember = pIterMember.second;
		const int& UID = pMember->GetAccountID();
		const char* pNickname = Server()->GetAccountNickname(UID);
		VList.AddMenu(MENU_GUILD_MEMBERSHIP_SELECTED, UID, "{}. {} {} Deposit: {}", Position, pMember->GetRank()->GetName(), pNickname, pMember->GetDeposit());
		Position++;
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowMembershipEdit(CPlayer* pPlayer, int AccountID) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pMember = pGuild->GetMembers()->Get(AccountID);
	auto* pSelfMember = pPlayer->Account()->GetGuildMember();
	if(!pMember || !pSelfMember)
		return;

	// initialize variables
	bool RightsLeader = pSelfMember->CheckAccess(GUILD_RANK_RIGHT_LEADER);
	bool RightsInviteKick = pSelfMember->CheckAccess(GUILD_RANK_RIGHT_INVITE_KICK);
	bool SelfSlot = (pMember->GetAccountID() == pSelfMember->GetAccountID());

	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2324 Editing member {}", Server()->GetAccountNickname(pMember->GetAccountID()));
	VInfo.Add("Deposit: {}", pMember->GetDeposit());
	VInfo.Add("Rank: {}", pMember->GetRank()->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// top-middle
	VoteWrapper VTop(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "Management");
	VTop.AddIfOption(RightsLeader && !SelfSlot, "GUILD_SET_LEADER", "Give Leader (in reason 134)");
	VTop.AddIfOption(RightsInviteKick && !SelfSlot, "GUILD_KICK_MEMBER", AccountID, "Kick");
	VoteWrapper::AddEmptyline(ClientID);

	// selector rights
	if(RightsLeader)
	{
		VoteWrapper VSelector(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "Set new rank", Server()->GetAccountNickname(pMember->GetAccountID()));
		VSelector.ReinitNumeralDepthStyles({ {DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD} });
		for(auto& pRank : pGuild->GetRanks()->GetContainer())
		{
			if(pMember->GetRank()->GetID() != pRank->GetID())
			{
				VSelector.MarkList().AddOption("GUILD_CHANGE_MEMBER_RANK", AccountID, pRank->GetID(), "Change rank to: {}",
					pRank->GetName(), pRank->GetRights() > GUILD_RANK_RIGHT_DEFAULT ? "*" : "");
			}
		}
		VoteWrapper::AddEmptyline(ClientID);
	}
}

void CGuildManager::ShowRanksList(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	int ClientID = pPlayer->GetCID();
	int MaxRanksNum = (int)GUILD_RANKS_MAX_COUNT;
	int CurrentRanksNum = pPlayer->Account()->GetGuild()->GetRanks()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE, "\u2324 Rank management (Information)");
	VInfo.Add("Maximal {} ranks for one guild", MaxRanksNum);
	VInfo.Add("Guild leader ignores rank rights");
	VInfo.Add("Use the reason as a text field.");
	VoteWrapper::AddEmptyline(ClientID);

	// top-middle
	VoteWrapper VTop(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "Management");
	VTop.AddOption("GUILD_RANK_CREATE", "New rank (by reason field)");
	VoteWrapper::AddEmptyline(ClientID);

	// ranks list
	VoteWrapper VList(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "List of ranks ({} of {})", CurrentRanksNum, MaxRanksNum);
	VList.ReinitNumeralDepthStyles({ {DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD} });
	for(auto pRank : pPlayer->Account()->GetGuild()->GetRanks()->GetContainer())
	{
		GuildRankIdentifier ID = pRank->GetID();
		bool IsDefaultRank = (pRank == pPlayer->Account()->GetGuild()->GetRanks()->GetDefaultRank());
		std::string StrAppendRankInfo = IsDefaultRank ? "- Beginning" : "- " + std::string(pRank->GetRightsName());
		VList.MarkList().AddMenu(MENU_GUILD_RANK_SELECTED, ID, "{} {}", pRank->GetName(), StrAppendRankInfo.c_str());
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowRankEdit(CPlayer* pPlayer, GuildRankIdentifier ID) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pRank = pGuild->GetRanks()->Get(ID);
	if(!pRank)
		return;

	// information
	const int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2324 Editing rank '{}'", pRank->GetName());
	VInfo.Add("Current rigths: {}", pRank->GetRightsName());
	VoteWrapper::AddEmptyline(ClientID);

	// top-middle
	VoteWrapper VTop(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "Setting important", pRank->GetName());
	VTop.AddOption("GUILD_RANK_RENAME", pRank->GetID(), "Rename (by reason field)");
	VTop.AddOption("GUILD_RANK_REMOVE", pRank->GetID(), "Remove");
	VoteWrapper::AddEmptyline(ClientID);

	// selector rights
	VoteWrapper VSelector(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "Setting rights");
	for(int i = GUILD_RANK_RIGHT_START; i < GUILD_RANK_RIGHT_END; i++)
	{
		bool IsSet = (pRank->GetRights() == static_cast<GuildRankRights>(i));
		VSelector.AddOption("GUILD_RANK_SET_RIGHTS", pRank->GetID(), i, "[{}] {}", (IsSet ? "✔" : "×"), pRank->GetRightsName((GuildRankRights)i));
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowRequests(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	// information
	const int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Membership requests (Information)");
	VInfo.Add("Players can send requests to join the guild");
	VInfo.Add("Guild members with the necessary rights,");
	VInfo.Add("can (accept and reject) invitations.");
	VoteWrapper::AddEmptyline(ClientID);

	// If there are requests in the container
	auto& vRequests = pGuild->GetMembers()->GetRequests()->GetContainer();
	if(vRequests.empty())
	{
		VoteWrapper(ClientID).Add("Requests is empty");
		VoteWrapper::AddEmptyline(ClientID);
		return;
	}

	// request list
	for(const auto& pRequest : vRequests)
	{
		VoteWrapper VRequest(ClientID, VWF_UNIQUE, "Request from {}", Server()->GetAccountNickname(pRequest->GetFromUID()));
		VRequest.AddOption("GUILD_REQUESTS_ACCEPT", pRequest->GetFromUID(), "Accept");
		VRequest.AddOption("GUILD_REQUESTS_DENY", pRequest->GetFromUID(), "Deny");
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowDoorsControl(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pHouse = pGuild->GetHouse();
	if(!pHouse)
		return;

	int ClientID = pPlayer->GetCID();
	int DoorsNum = (int)pHouse->GetDoorManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 House doors information");
	VInfo.Add("You can control your doors in the house");
	VInfo.Add("Your home has: {} doors.", DoorsNum);
	VoteWrapper::AddEmptyline(ClientID);

	// doors control
	VoteWrapper VDoors(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "\u2743 Door's control");
	for(auto& [Number, DoorData] : pHouse->GetDoorManager()->GetContainer())
	{
		bool StateDoor = DoorData->IsClosed();
		VDoors.AddOption("GUILD_HOUSE_DOOR", Number, "[{}] {} door", StateDoor ? "Closed" : "Open", DoorData->GetName());
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowPlantzonesControl(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pHouse = pGuild->GetHouse();
	if(!pHouse)
		return;

	int ClientID = pPlayer->GetCID();
	int PlantzonesNum = (int)pHouse->GetPlantzonesManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Plant zones information");
	VInfo.Add("You can control your plant zones in the house");
	VInfo.Add("Your home has: {} plant zones.", PlantzonesNum);
	VoteWrapper::AddEmptyline(ClientID);

	// plant zones control
	VoteWrapper VPlantzones(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "\u2743 Plant zone's control");
	for(auto& [ID, Plantzone] : pHouse->GetPlantzonesManager()->GetContainer())
		VPlantzones.AddMenu(MENU_GUILD_HOUSE_PLANTZONE_SELECTED, ID, "Plant {} zone / {}", Plantzone.GetName(), GS()->GetItemInfo(Plantzone.GetItemID())->GetName());

	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowPlantzoneEdit(CPlayer* pPlayer, int PlantzoneID) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pHouse = pGuild->GetHouse();
	if(!pHouse)
		return;

	auto* pPlantzone = pHouse->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID);
	if(!pPlantzone)
		return;

	int ClientID = pPlayer->GetCID();
	CItemDescription* pItem = GS()->GetItemInfo(pPlantzone->GetItemID());

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE|VWF_STYLE_STRICT_BOLD, "\u2741 Plant {} zone", pPlantzone->GetName());
	VInfo.Add("You can grow a plant on the property");
	VInfo.Add("Chance: {}%", s_GuildChancePlanting);
	VInfo.Add("Planted: {}", pItem->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// items list availables can be planted
	VoteWrapper VPlantItems(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "\u2741 Possible items for planting");
	std::vector<ItemIdentifier> vItems = Core()->InventoryManager()->GetItemIDsCollectionByFunction(FUNCTION_PLANT);
	for(auto& ID : vItems)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(ID);
		if(pPlayerItem->HasItem() && ID != pPlantzone->GetItemID())
		{
			VPlantItems.AddOption("GUILD_HOUSE_PLANT_ZONE_TRY", PlantzoneID, ID, "Try plant {} (has {})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
		}
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowFinder(CPlayer* pPlayer) const
{
	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Guild finder information");
	VInfo.Add("You can find a guild by name or select from the list");
	if(pPlayer->Account()->HasGuild())
		VInfo.Add("You already in guild '{}'!", pPlayer->Account()->GetGuild()->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// search field
	VoteWrapper VSearch(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2732 Search by name");
	VSearch.AddOption("GUILD_FINDER_SEARCH_FIELD", "Search: [{}]", pPlayer->GetTempData().m_aGuildSearchBuf[0] == '\0' ? "by reason field" : pPlayer->GetTempData().m_aGuildSearchBuf);
	VoteWrapper::AddEmptyline(ClientID);

	// search list
	VoteWrapper VList(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "Guild list");
	for(auto& pGuild : CGuild::Data())
	{
		if(pPlayer->GetTempData().m_aGuildSearchBuf[0] == '\0' || str_utf8_find_nocase(pGuild->GetName(), pPlayer->GetTempData().m_aGuildSearchBuf) != nullptr)
		{
			int OwnerUID = pGuild->GetLeaderUID();
			VList.AddMenu(MENU_GUILD_FINDER_SELECTED, pGuild->GetID(), "{} (leader {})", pGuild->GetName(), Server()->GetAccountNickname(OwnerUID));
		}
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowFinderDetail(CPlayer* pPlayer, GuildIdentifier ID) const
{
	auto* pGuild = GetGuildByID(ID);
	if(!pGuild)
		return;

	// initialize variables
	int Position = 1;
	int ClientID = pPlayer->GetCID();
	auto CurrentSlots = pGuild->GetMembers()->GetCurrentSlots();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Information about {}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VInfo.Add("Members: {} of {}", CurrentSlots.first, CurrentSlots.second);
	VInfo.Add("Has house: {}", pGuild->HasHouse() ? "Yes" : "No");
	VInfo.Add("Bank: {} golds", pGuild->GetBank()->Get());
	VInfo.AddIfOption(!pPlayer->Account()->HasGuild(), "GUILD_SEND_REQUEST", pGuild->GetID(), pPlayer->Account()->GetID(), "Send request to join");
	VoteWrapper::AddEmptyline(ClientID);

	// Memberlist
	VoteWrapper VMemberlist(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "List of membership");
	for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
	{
		auto pMember = pIterMember.second;
		VMemberlist.Add("{}. {} {} Deposit: {}", Position, pMember->GetRank()->GetName(), Server()->GetAccountNickname(pMember->GetAccountID()), pMember->GetDeposit());
		Position++;
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowBuyHouse(int ClientID, CGuildHouse* pHouse) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// Show information about guild house
	VoteWrapper VHouseInfo(ClientID, VWF_SEPARATE_CLOSED, "Information guild house");
	VHouseInfo.Add("Buying a house you will need to constantly the Treasury");
	VHouseInfo.Add("In the intervals of time will be paid house");
	VoteWrapper::AddLine(ClientID);

	// Check if house is available for sale
	VoteWrapper VHouse(ClientID, VWF_SEPARATE_OPEN, "House detail information");
	if(!pHouse)
	{
		VHouse.Add("This house is not for sale yet");
		return;
	}

	VHouse.Add("House owned by: {}", pHouse->GetOwnerName());
	VHouse.Add("House price: {} gold", pHouse->GetPrice());
	VHouse.Add("House rent price per day: {} gold", pHouse->GetRentPrice());
	VHouse.Add("House has {} plant zone's", (int)pHouse->GetPlantzonesManager()->GetContainer().size());
	VHouse.Add("House has {} controlled door's", (int)pHouse->GetDoorManager()->GetContainer().size());
	VoteWrapper::AddLine(ClientID);

	// Check if house is not purchased
	if(!pHouse->IsPurchased())
	{
		// Check if the player does not have a guild
		if(!pPlayer->Account()->HasGuild())
		{
			VoteWrapper(ClientID).Add("You need to be in a guild to buy a house");
			return;
		}

		// Check if player has leader rights in the guild
		CGuild* pGuild = pPlayer->Account()->GetGuild();
		VoteWrapper VBuyHouse(ClientID, VWF_SEPARATE_OPEN, "Guild bank: {} gold", pGuild->GetBank()->Get());
		if(pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
			VBuyHouse.AddOption("GUILD_HOUSE_BUY", pHouse->GetID(), "Purchase this guild house! Cost: {} golds", pHouse->GetPrice());
		else
			VBuyHouse.Add("You need to be the leader rights");
	}
}

void CGuildManager::ShowDeclareWar(int ClientID) const
{
	// If player does not exist or does not have a guild, return
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// Check some guild data
	CGuild* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		VoteWrapper::AddBackpage(ClientID);
		return;
	}

	// TODO
	VoteWrapper VWar(ClientID);
	VWar.Add("\u2646 Declare war on another guild");
	VWar.Add("Cooldown: {} minutes", 10);
	VWar.AddLine();

	if(pGuild->GetWar())
	{
		CGuildWarData* pWar = pGuild->GetWar();
		CGuild* pTargetGuild = pWar->GetTargetGuild();

		char aBufTimeLeft[64];
		pWar->GetHandler()->FormatTimeLeft(aBufTimeLeft, sizeof(aBufTimeLeft));

		VWar.Add("War with the guild: {}", pTargetGuild->GetName());
		VWar.Add("Time until the end of the war: {}", aBufTimeLeft);
		VWar.Add("Score: {}(your) / {}(enemy)", pWar->GetScore(), pTargetGuild->GetWar()->GetScore());
	}
	else
	{
		VWar.Add("You can declare war on another guild");
		VWar.Add("To do this, select the guild from the list below");
		VWar.AddLine();

		VoteWrapper VWarList(ClientID, "List of guilds to declare war");
		for(auto& p : CGuild::Data())
		{
			if(p->GetID() != pGuild->GetID())
				VWarList.AddOption("GUILD_DECLARE_WAR", p->GetID(), "{} (online {} players)", p->GetName(), p->GetMembers()->GetOnlinePlayersCount());
		}
	}

	// Add the votes backpage for the player
	VoteWrapper::AddBackpage(ClientID);
}

// Function to show guild logs for a specific player
void CGuildManager::ShowLogs(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	auto* pLogger = pGuild->GetLogger();
	auto flagStatus = [&](int flag) { return pLogger->IsActivityFlagSet(flag) ? "[\u2714]" : "[\u2715]"; };

	// logger settings
	VoteWrapper VLogger(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Activity log settings");
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_GUILD_FULL, "Full changes {}", flagStatus(LOGFLAG_GUILD_FULL));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_GUILD_MAIN_CHANGES, "Main changes {}", flagStatus(LOGFLAG_GUILD_MAIN_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_MEMBERS_CHANGES, "Members changes {}", flagStatus(LOGFLAG_MEMBERS_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_BANK_CHANGES, "Bank changes {}", flagStatus(LOGFLAG_BANK_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_RANKS_CHANGES, "Ranks changes {}", flagStatus(LOGFLAG_RANKS_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_UPGRADES_CHANGES, "Upgrades changes {}", flagStatus(LOGFLAG_UPGRADES_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_HOUSE_MAIN_CHANGES, "House main changes {}", flagStatus(LOGFLAG_HOUSE_MAIN_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_HOUSE_DOORS_CHANGES, "House doors changes {}", flagStatus(LOGFLAG_HOUSE_DOORS_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_HOUSE_DECORATIONS_CHANGES, "House decorations changes {}", flagStatus(LOGFLAG_HOUSE_DECORATIONS_CHANGES));
	VoteWrapper::AddEmptyline(ClientID);

	// Logs
	char aBuf[128];
	VoteWrapper VLogs(ClientID, VWF_CLOSED, "Logs");
	const CGuild::LogContainer& aLogs = pGuild->GetLogger()->GetContainer();
	for(const auto& pLog : aLogs)
	{
		// Format the log entry and display it
		str_format(aBuf, sizeof(aBuf), "[%s] %s", pLog.m_Time.c_str(), pLog.m_Text.c_str());
		VLogs.Add("{}", aBuf);
	}
	VoteWrapper::AddEmptyline(ClientID);
}

CGuildHouse* CGuildManager::GetGuildHouseByID(const GuildHouseIdentifier& ID) const
{
	auto itHouse = std::find_if(CGuildHouse::Data().begin(), CGuildHouse::Data().end(), [&ID](const CGuildHouse* p)
	{
		return ID == p->GetID();
	});

	return itHouse != CGuildHouse::Data().end() ? *itHouse : nullptr;
}

CGuildHouse* CGuildManager::GetGuildHouseByPos(vec2 Pos) const
{
	auto itHouse = std::find_if(CGuildHouse::Data().begin(), CGuildHouse::Data().end(), [&Pos, this](const CGuildHouse* p)
	{
		return GS()->GetWorldID() == p->GetWorldID() && distance(Pos, p->GetPos()) < p->GetRadius();
	});

	return itHouse != CGuildHouse::Data().end() ? *itHouse : nullptr;
}

CGuild* CGuildManager::GetGuildByID(GuildIdentifier ID) const
{
	auto itGuild = std::find_if(CGuild::Data().begin(), CGuild::Data().end(), [&ID](CGuild* p)
	{
		return p->GetID() == ID;
	});

	return itGuild != CGuild::Data().end() ? (*itGuild) : nullptr;
}

CGuild* CGuildManager::GetGuildByName(const char* pGuildname) const
{
	auto itGuild = std::find_if(CGuild::Data().begin(), CGuild::Data().end(), [&pGuildname](CGuild* p)
	{
		return str_comp_nocase(p->GetName(), pGuildname) == 0;
	});

	return itGuild != CGuild::Data().end() ? (*itGuild) : nullptr;
}

CGuildHouse::CPlantzone* CGuildManager::GetGuildHousePlantzoneByPos(vec2 Pos) const
{
	for(auto& p : CGuildHouse::Data())
	{
		for(auto& Plantzone : p->GetPlantzonesManager()->GetContainer())
		{
			if(distance(Pos, Plantzone.second.GetPos()) < Plantzone.second.GetRadius())
				return &Plantzone.second;
		}
	}

	return nullptr;
}

bool CGuildManager::IsAccountMemberGuild(int AccountID) const
{
	return CGuild::IsAccountMemberGuild(AccountID);
}
