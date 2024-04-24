/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

void CGuildManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_TABLE);
	while(pRes->next())
	{
		GuildIdentifier ID = pRes->getInt("ID");
		std::string Name = pRes->getString("Name").c_str();
		std::string MembersData = pRes->getString("Members").c_str();
		GuildRankIdentifier DefaultRankID = pRes->getInt("DefaultRankID");
		int LeaderUID = pRes->getInt("LeaderUID");
		int Level = pRes->getInt("Level");
		int Experience = pRes->getInt("Experience");
		int Bank = pRes->getInt("Bank");
		int Score = pRes->getInt("Score");
		int64_t LogFlag = pRes->getInt64("LogFlag");

		CGuildData::CreateElement(ID)->Init(Name, std::move(MembersData), DefaultRankID, Level, Experience, Score, LeaderUID, Bank, LogFlag, &pRes);
	}

	InitWars();
	Core()->ShowLoadingProgress("Guilds", CGuildData::Data().size());
}

void CGuildManager::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HOUSES, pWhereLocalWorld);
	while(pRes->next())
	{
		GuildHouseIdentifier ID = pRes->getInt("ID");
		GuildIdentifier GuildID = pRes->getInt("GuildID");
		int Price = pRes->getInt("Price");
		std::string JsPlantzones = pRes->getString("Plantzones").c_str();
		std::string JsPropersties = pRes->getString("Properties").c_str();

		CGuildData* pGuild = GetGuildByID(GuildID);
		CGuildHouseData::CreateElement(ID)->Init(pGuild, Price, GS()->GetWorldID(), std::move(JsPlantzones), std::move(JsPropersties));
	}

	Core()->ShowLoadingProgress("Guild houses", CGuildHouseData::Data().size());
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
	const auto& HouseData = CGuildHouseData::Data();
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

			//const int Exp = CGuildData::ms_aGuild[GuildID].m_UpgradesData(CGuildData::CHAIR_EXPERIENCE, 0).m_Value;
			//pPlayer->AccountManager()->AddExperience(Exp);
		}
		return true;
	}

	return false;
}

bool CGuildManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(CMD, "GUILD_HOUSE_SPAWN") == 0)
	{
		// Check if the player has a guild
		if(!pPlayer->Account()->HasGuild())
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Check if the guild has a house
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild->HasHouse())
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		CGuildHouseData* pHouse = pGuild->GetHouse();
		vec2 HousePosition = pHouse->GetPos();

		// Check if the player is in a different world than pAether
		if(!GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
		{
			// Change the player's world to pAether's world
			pPlayer->GetTempData().SetTeleportPosition(HousePosition);
			pPlayer->ChangeWorld(pHouse->GetWorldID());
			return true;
		}

		// Change the player's position to pAether's position
		pPlayer->GetCharacter()->ChangePosition(HousePosition);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(CMD, "GUILD_HOUSE_DECORATION") == 0)
	{
		// Check if the player has a guild or if they have the access rights to upgrade the house
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		CGuildHouseData* pHouse = pPlayer->Account()->GetGuild()->GetHouse();

		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		bool Result = pHouse->GetDecorationManager()->StartDrawing(pPlayer);
		if(Result)
		{
			GS()->Chat(ClientID, "You can now draw decorations.");
		}
		else
		{
			GS()->Chat(ClientID, "You can't draw decorations.");
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_SET_NEW_LEADER") == 0)
	{
		// Check if the player has access to set a new guild leader
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			// Inform the player that they have no access or are not a member of the guild
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Get the member UID, guild data for the new leader
		const int& MemberUID = VoteID;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();

		// Set the new leader for the guild and get the result
		GUILD_RESULT Result = pGuild->SetNewLeader(MemberUID);
		if(Result == GUILD_RESULT::SET_LEADER_NON_GUILD_PLAYER)
		{
			GS()->Chat(ClientID, "The player is not a member of your guild");
		}
		else if(Result == GUILD_RESULT::SET_LEADER_PLAYER_ALREADY_LEADER)
		{
			GS()->Chat(ClientID, "The player is already a leader");
		}
		else if(Result == GUILD_RESULT::SUCCESSFUL)
		{
			GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_CHANGE_PLAYER_RANK") == 0)
	{
		// Check if the player has a guild or has leader rights
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Get the player's guild data, guild member data, member UID and rank ID from the vote ID
		const int& MemberUID = VoteID;
		const GuildRankIdentifier& RankID = VoteID2;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		CGuildMemberData* pInterMember = pGuild->GetMembers()->Get(MemberUID);

		// Check if the member data is valid and set the rank for the member
		if(!pInterMember || !pInterMember->SetRank(RankID))
		{
			GS()->Chat(ClientID, "Set a player's rank failed, try again later");
			return true;
		}

		// Update the votes for all players to refresh the guild view players menu
		GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP);
		return true;
	}

	if(PPSTR(CMD, "GUILD_DISBAND") == 0)
	{
		// Check if the player has a guild or if they have the leader access rights
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Check if the input code is correct
		if(Get != 55428)
		{
			GS()->Chat(ClientID, "Random Touch Security Code has not been entered correctly.");
			return true;
		}

		// Disband the guild with the given ID
		Disband(pPlayer->Account()->GetGuild()->GetID());
		return true;
	}

	if(PPSTR(CMD, "GUILD_KICK_PLAYER") == 0)
	{
		// Check if the player has access to kick members from the guild
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Check if the player is trying to kick themselves
		if(pPlayer->Account()->GetID() == VoteID)
		{
			GS()->Chat(ClientID, "You can't kick yourself");
			return true;
		}

		// Attempt to kick the player from the guild
		GUILD_MEMBER_RESULT Result = pPlayer->Account()->GetGuild()->GetMembers()->Kick(VoteID);
		if(Result == GUILD_MEMBER_RESULT::KICK_IS_OWNER)
		{
			GS()->Chat(ClientID, "You can't kick a leader");
		}
		else if(Result == GUILD_MEMBER_RESULT::KICK_DOES_NOT_EXIST)
		{
			GS()->Chat(ClientID, "The player is no longer on the guild membership lists");
		}
		else if(Result == GUILD_MEMBER_RESULT::SUCCESSFUL)
		{
			GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_DEPOSIT_GOLD") == 0)
	{
		if(!pPlayer->Account()->HasGuild())
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Check if the amount to deposit is less than 100
		if(Get < 100)
		{
			GS()->Chat(ClientID, "Minimum number is 100 gold.");
			return true;
		}

		// Get the guild member data for the player
		CGuildMemberData* pMember = pPlayer->Account()->GetGuildMemberData();

		// Deposit the specified amount in the guild bank
		if(pMember->DepositInBank(Get))
		{
			pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_RANK_NAME_FIELD") == 0)
	{
		// Check if the input text is equal to "NULL"
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// Copy the input text to the player's temporary data buffer for guild rank
		str_copy(pPlayer->GetTempData().m_aRankGuildBuf, GetText, sizeof(pPlayer->GetTempData().m_aRankGuildBuf));

		// Update the votes for all players to refresh the guild rank menu
		GS()->UpdateVotesIfForAll(MENU_GUILD_RANKS);
		return true;
	}

	if(PPSTR(CMD, "GUILD_RANK_CREATE") == 0)
	{
		// Check if the player has a guild or has leader access
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Attempt to add the rank to the guild
		GUILD_RANK_RESULT Result = pPlayer->Account()->GetGuild()->GetRanks()->Add(pPlayer->GetTempData().m_aRankGuildBuf);
		if(Result == GUILD_RANK_RESULT::ADD_ALREADY_EXISTS)
		{
			GS()->Chat(ClientID, "The rank name already exists");
		}
		else if(Result == GUILD_RANK_RESULT::ADD_LIMIT_HAS_REACHED)
		{
			GS()->Chat(ClientID, "Rank limit reached, {} out of {}", (int)MAX_GUILD_RANK_NUM, (int)MAX_GUILD_RANK_NUM);
		}
		else if(Result == GUILD_RANK_RESULT::WRONG_NUMBER_OF_CHAR_IN_NAME)
		{
			GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16.");
			GS()->Chat(ClientID, "You may be using unauthorized characters in the name, like '");
		}
		else if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->Chat(ClientID, "The rank '{}' has been successfully added!", pPlayer->GetTempData().m_aRankGuildBuf);
			GS()->UpdateVotesIfForAll(MENU_GUILD_RANKS);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_RANK_RENAME") == 0)
	{
		// Check if the player has access to rename a guild rank
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Rename the guild rank and store the result
		GUILD_RANK_RESULT Result = pPlayer->Account()->GetGuild()->GetRanks()->Get(VoteID)->Rename(pPlayer->GetTempData().m_aRankGuildBuf);
		if(Result == GUILD_RANK_RESULT::RENAME_ALREADY_NAME_EXISTS)
		{
			GS()->Chat(ClientID, "The name is already in use by another rank");
		}
		else if(Result == GUILD_RANK_RESULT::WRONG_NUMBER_OF_CHAR_IN_NAME)
		{
			GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16.");
			GS()->Chat(ClientID, "You may be using unauthorized characters in the name, like '");
		}
		else if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->UpdateVotesIfForAll(MENU_GUILD_RANKS);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_RANK_REMOVE") == 0)
	{
		// Check if the player has access to remove a guild rank
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Get the rank ID, rank data
		const int& RankID = VoteID;
		CGuildRankData* pRank = pPlayer->Account()->GetGuild()->GetRanks()->Get(RankID);

		// Remove the rank from the guild
		GUILD_RANK_RESULT Result = pPlayer->Account()->GetGuild()->GetRanks()->Remove(pRank->GetName());
		if(Result == GUILD_RANK_RESULT::REMOVE_RANK_IS_DEFAULT)
		{
			GS()->Chat(ClientID, "You can't remove default rank");
		}
		else if(Result == GUILD_RANK_RESULT::REMOVE_RANK_DOES_NOT_EXIST)
		{
			GS()->Chat(ClientID, "There is no such rank");
		}
		else if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->UpdateVotesIfForAll(MENU_GUILD_RANKS);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_RANK_ACCESS") == 0)
	{
		// Check if the player has a guild and if they have the leader access
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Get the rank ID from the vote ID and guild rank data for the given rank ID
		const int& RankID = VoteID;
		CGuildRankData* pRank = pPlayer->Account()->GetGuild()->GetRanks()->Get(RankID);

		// Change the access for the rank
		pRank->ChangeAccess();

		// Update the votes for all players in the guild rank menu
		GS()->UpdateVotesIfForAll(MENU_GUILD_RANKS);
		return true;
	}

	if(PPSTR(CMD, "GUILD_REQUESTS_ACCEPT") == 0)
	{
		// Check if the player has access to invite or kick members from the guild
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Get the unique ID guild data of the player who sent the join request
		const int& RequestFromUID = VoteID;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();

		// Accept the join request and get the result
		GUILD_MEMBER_RESULT Result = pGuild->GetMembers()->GetRequests()->Accept(RequestFromUID, pPlayer->Account()->GetGuildMemberData());
		if(Result == GUILD_MEMBER_RESULT::JOIN_ALREADY_IN_GUILD)
		{
			GS()->Chat(ClientID, "The player is already in a guild");
		}
		else if(Result == GUILD_MEMBER_RESULT::NO_AVAILABLE_SLOTS)
		{
			GS()->Chat(ClientID, "No guild slots available.");
		}
		else if(Result == GUILD_MEMBER_RESULT::SUCCESSFUL)
		{
			GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP);
			GS()->UpdateVotesIfForAll(MENU_GUILD_INVITES);
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_REQUESTS_DENY") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Get the request UID, player's guild data
		const int& RequestFromUID = VoteID;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();

		// Deny the request from the specified UID
		pGuild->GetMembers()->GetRequests()->Deny(RequestFromUID, pPlayer->Account()->GetGuildMemberData());
		GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBERSHIP);
		GS()->UpdateVotesIfForAll(MENU_GUILD_INVITES);
		return true;
	}

	if(PPSTR(CMD, "GUILD_FINDER_SEARCH_FIELD") == 0)
	{
		// Check if the input text is "NULL"
		if(PPSTR(GetText, "NULL") == 0)
		{
			// If it is "NULL", send a chat message to the client
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// Copy the input text to the player's temporary data buffer
		str_copy(pPlayer->GetTempData().m_aGuildSearchBuf, GetText, sizeof(pPlayer->GetTempData().m_aGuildSearchBuf));

		// Update the votes for the client and open the guild finder menu
		pPlayer->m_VotesData.UpdateVotes(MENU_GUILD_FINDER);
		return true;
	}

	if(PPSTR(CMD, "GUILD_JOIN_REQUEST") == 0)
	{
		// Check if the player already has a guild
		if(pPlayer->Account()->HasGuild())
		{
			GS()->Chat(ClientID, "You're already in a guild.");
			return true;
		}

		// Get the guild ID, guild data using the guild ID and account ID from the vote
		const GuildIdentifier& ID = VoteID;
		const int& AccountID = VoteID2;
		CGuildData* pGuild = GetGuildByID(ID);
		dbg_assert(pGuild != nullptr, "GUILD_REQUEST_TO_JOIN - guild data is empty");

		// Send a request to join the guild and get the result
		GUILD_MEMBER_RESULT Result = pGuild->GetMembers()->GetRequests()->Request(AccountID);
		if(Result == GUILD_MEMBER_RESULT::REQUEST_ALREADY_SEND)
		{
			GS()->Chat(ClientID, "You have already sent a request to this guild.");
		}
		else if(Result == GUILD_MEMBER_RESULT::NO_AVAILABLE_SLOTS)
		{
			GS()->Chat(ClientID, "No guild slots available.");
		}
		else if(Result == GUILD_MEMBER_RESULT::SUCCESSFUL)
		{
			GS()->Chat(ClientID, "You sent a request to join the {} guild.", pGuild->GetName());
		}
		return true;
	}

	if(PPSTR(CMD, "GUILD_HOUSE_BUY") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const GuildHouseIdentifier& ID = VoteID;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();

		GUILD_RESULT Result = pGuild->BuyHouse(ID);
		if(Result == GUILD_RESULT::BUY_HOUSE_ALREADY_HAVE)
		{
			GS()->Chat(ClientID, "Your guild already has a house.");
		}
		else if(Result == GUILD_RESULT::BUY_HOUSE_ALREADY_PURCHASED)
		{
			GS()->Chat(ClientID, "This guild house has already been purchased.");
		}
		else if(Result == GUILD_RESULT::BUY_HOUSE_NOT_ENOUGH_GOLD)
		{
			GS()->Chat(ClientID, "Your guild doesn't have enough gold.");
		}
		else if(Result == GUILD_RESULT::BUY_HOUSE_UNAVAILABLE)
		{
			GS()->Chat(ClientID, "This guild house is not available for purchase.");
		}
		else if(Result == GUILD_RESULT::SUCCESSFUL)
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
			GS()->UpdateVotesIfForAll(MENU_GUILD);
		}

		return true;
	}

	if(PPSTR(CMD, "GUILD_DECLARE_WAR") == 0)
	{
		// Check if the player has a guild or has the right to declare war
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}
		// Check if the player is trying to declare war on their own guild
		if(pPlayer->Account()->GetGuild()->GetID() == VoteID)
		{
			GS()->Chat(ClientID, "You can't declare war on your own guild.");
			return true;
		}

		// Get the guild ID, guild data using the guild ID and account ID from the vote
		const GuildIdentifier& ID = VoteID;
		CGuildData* pGuild = GetGuildByID(ID);
		dbg_assert(pGuild != nullptr, "GUILD_DECLARE_WAR - guild data is empty");

		if(pPlayer->Account()->GetGuild()->StartWar(pGuild))
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

	if(PPSTR(CMD, "GUILD_HOUSE_PLANT_ZONE_TRY") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int& Useds = maximum(1, Get);
		const int& PlantzoneID = VoteID;
		const ItemIdentifier& ItemID = VoteID2;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		CGuildHouseData* pHouse = pGuild->GetHouse();

		// Check if the guild does not have a house
		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// Check if pPlantzone is null or undefined
		CGuildHousePlantzoneData* pPlantzone = pHouse->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID);
		if(!pPlantzone)
		{
			GS()->Chat(ClientID, "Plant zone not found.");
			return true;
		}

		// Check if the ItemID of the plant matches the ItemID of the plant zone
		if(ItemID == pPlantzone->GetItemID())
		{
			GS()->Chat(ClientID, "This plant is already planted.");
			return true;
		}

		// Check if the player has enough currency to spend
		if(pPlayer->Account()->SpendCurrency(Useds, ItemID))
		{
			// Check if the chance result is successful
			bool Success = false;
			Chance result(0.025f);
			for(int i = 0; i < Useds && !Success; i++)
			{
				if(result())
					Success = true;

				// Update the chance result for the next attempt
				result.Update();
			}

			// Check if the planting was successful
			if(!Success)
			{
				// If not successful, inform the client that they failed to plant the plant
				GS()->Chat(ClientID, "You failed to plant the plant.");
			}
			else
			{
				// Change the item in the plant zone to the planted item
				GS()->Chat(ClientID, "You have successfully planted the plant.");
				pPlantzone->ChangeItem(ItemID);
			}

			pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD_HOUSE_PLANT_ZONE_SELECTED);
		}

		return true;
	}

	if(PPSTR(CMD, "GUILD_HOUSE_DOOR") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check player house
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		CGuildHouseData* pHouse = pGuild->GetHouse();

		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// reverse door house
		int UniqueDoorID = VoteID;
		pHouse->GetDoorManager()->Reverse(UniqueDoorID);
		GS()->UpdateVotesIfForAll(MENU_GUILD);
		return true;
	}

	if(PPSTR(CMD, "GUILD_HOUSE_SELL") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		if(pGuild->SellHouse())
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
			GS()->UpdateVotesIfForAll(MENU_GUILD);
		}
	}

	if(PPSTR(CMD, "GUILD_UPGRADE") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild->Upgrade(VoteID))
		{
			GS()->Chat(ClientID, "Your guild does not have enough gold, or the maximum upgrade level has been reached.");
			return true;
		}

		GS()->UpdateVotesIfForAll(MENU_GUILD);
		return true;
	}

	if(PPSTR(CMD, "GUILD_LOGGER_SET") == 0)
	{
		// Check if the player has a guild or has the right to invite/kick members
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// Set the log flag to the value of VoteID
		const int& Logflag = VoteID;
		CGuildData* pGuild = pPlayer->Account()->GetGuild();

		// Set the logger of the guild data to the log flag
		pGuild->GetLogger()->SetActivityFlag(Logflag);
		GS()->UpdateVotesIfForAll(MENU_GUILD_LOGS);
		return true;
	}

	return false;
}

bool CGuildManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_GUILD_HOUSE_PURCHASE_INFO)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		CCharacter* pChr = pPlayer->GetCharacter();
		CGuildHouseData* pHouse = GetGuildHouseByPos(pChr->m_Core.m_Pos);
		ShowBuyHouse(ClientID, pHouse);
		return true;
	}

	if(Menulist == MENU_GUILD_FINDER)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowFinder(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_FINDER_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_FINDER);
		ShowFinderDetailInformation(ClientID, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		return true;
	}

	if(Menulist == MENU_GUILD)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_MEMBERSHIP)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowMembershipList(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_LOGS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowLogs(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_WARS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDeclareWar(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_RANKS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowRanksSettings(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_INVITES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowRequests(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_HOUSE_PLANT_ZONE_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowPlantZone(ClientID, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		return true;
	}

	return false;
}

void CGuildManager::OnHandleTimePeriod(TIME_PERIOD Period)
{
	// Call the TimePeriodEvent function for each guild passing the Period parameter
	for(auto& pGuild : CGuildData::Data())
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
		CGuildData* pGuild1 = GetGuildByID(GuildID1);
		CGuildData* pGuild2 = GetGuildByID(GuildID2);
		dbg_assert(pGuild1 != nullptr, "GUILD_WAR - guild data is empty");
		dbg_assert(pGuild2 != nullptr, "GUILD_WAR - guild data is empty");

		// Create instance
		CGuildWarHandler* pWarHandler = CGuildWarHandler::CreateElement();
		pWarHandler->Init({ pGuild1, pGuild2, Score1 }, { pGuild2, pGuild1, Score2 }, TimeUntilEnd);
	}
}

void CGuildManager::ShowMembershipList(int ClientID) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// If the player is not in a guild
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	// Membership information
	auto CurrentSlots = pGuild->GetMembers()->GetCurrentSlots();
	CVoteWrapper VMembershipInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2723 Membership information", pGuild->GetName());
	VMembershipInfo.Add("\u2723 Guild name: {}", pGuild->GetName());
	VMembershipInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VMembershipInfo.Add("Members: {} of {}", CurrentSlots.first, CurrentSlots.second);
	CVoteWrapper::AddLine(ClientID);

	// Membership list
	CVoteWrapper(ClientID).Add("\u2635 Membership list of {}", pGuild->GetName());
	CGuildMemberData* pPlayerMember = pPlayer->Account()->GetGuildMemberData();
	for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
	{
		CGuildMemberData* pMember = pIterMember.second;
		const int& MemberUID = pMember->GetAccountID();
		const char* pNickname = Server()->GetAccountNickname(MemberUID);

		// Print the member's rank, account nickname, and deposit
		CVoteWrapper VMember(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "{} {} Deposit: {c}", pMember->GetRank()->GetName(), pNickname, pMember->GetDeposit());
		const bool IsSelfSlot = pPlayer->Account()->GetID() == pMember->GetAccountID();

		// Check if the player has leader rights
		if(pPlayerMember->CheckAccess(RIGHTS_LEADER))
		{
			// Loop through each rank in the guild
			for(auto& pRank : pGuild->GetRanks()->GetContainer())
			{
				if(pMember->GetRank()->GetID() != pRank->GetID())
					VMember.AddOption("GUILD_CHANGE_PLAYER_RANK", MemberUID, pRank->GetID(), "Change rank to: {}",
						pRank->GetName(), pRank->GetAccess() > RIGHTS_DEFAULT ? "*" : "");
			}

			// Check if the member is not the player themselves
			if(!IsSelfSlot)
				VMember.AddOption("GUILD_SET_NEW_LEADER", MemberUID, "Give Leader (in reason 134)");
		}

		// Check if the player has invite/kick rights
		if(pPlayerMember->CheckAccess(RIGHTS_INVITE_KICK))
		{
			// Check if the member is not the player themselves
			if(!IsSelfSlot)
				VMember.AddOption("GUILD_KICK_PLAYER", MemberUID, "Kick");
		}

		// Check if there are no available actions for the member
		VMember.AddIf(VMember.IsEmpty(), "No available actions on the player");
	}
	CVoteWrapper::AddEmptyline(ClientID);
	CVoteWrapper::AddEmptyline(ClientID);

	CVoteWrapper TestWrap(ClientID, VWF_CLOSED | VWF_STYLE_STRICT_BOLD);
	TestWrap.AddLine();
	TestWrap.Add("Deposit gold to the guild bank");
	TestWrap.AddLine();

	CVoteWrapper TestWrap2(ClientID, VWF_CLOSED | VWF_STYLE_STRICT_BOLD);
	TestWrap2.AddLine();
	TestWrap2.Add("Deposit gold to the guild bank");
	TestWrap2.AddLine();

	CVoteWrapper::AddEmptyline(ClientID);
	CVoteWrapper::AddEmptyline(ClientID);

	CVoteWrapper TestWrap3(ClientID,  VWF_STYLE_STRICT_BOLD);
	TestWrap3.AddLine();
	TestWrap3.Add("Deposit gold to the guild bank");
	TestWrap3.AddLine();

	CVoteWrapper TestWrap4(ClientID,  VWF_STYLE_STRICT_BOLD);
	TestWrap4.AddLine();
	TestWrap4.Add("Deposit gold to the guild bank");
	TestWrap4.AddLine();

	// Add the votes backpage for the player
	CVoteWrapper::AddBackpage(ClientID);
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
	std::string MembersData = R"({"members":[{"id":)" + std::to_string(pPlayer->Account()->GetID()) + R"(,"rank_id":0,"deposit":0}]})";

	CGuildData* pGuild = CGuildData::CreateElement(InitID);
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
	auto pIterGuild = std::find_if(CGuildData::Data().begin(), CGuildData::Data().end(), [&ID](CGuildData* pGuild) { return pGuild->GetID() == ID; });
	if(!(*pIterGuild))
		return;

	// Get a pointer to the guild
	CGuildData* pGuild = (*pIterGuild);

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
	GS()->SendInbox("System", pGuild->GetLeaderUID(), "Your guild was disbanded.", "We returned some gold from your guild.", itGold, ReturnsGold);
	GS()->Chat(-1, "The {} guild has been disbanded.", pGuild->GetName());

	// Remove all guild-related entries from the database
	Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '%d'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_RANKS_TABLE, "WHERE GuildID = '%d'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_TABLE, "WHERE ID = '%d'", pGuild->GetID());

	// Delete the guild object and remove it from the guild data container
	if(pIterGuild != CGuildData::Data().end())
	{
		delete (*pIterGuild);
		CGuildData::Data().erase(pIterGuild);
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
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	bool HasHouse = pGuild->HasHouse();
	int ExpNeed = computeExperience(pGuild->GetLevel());
	const int MemberUsedSlots = pGuild->GetMembers()->GetContainer().size();
	const int MemberMaxSlots = pGuild->GetUpgrades(CGuildData::UPGRADE_AVAILABLE_SLOTS)->m_Value;

	// Guild information
	CVoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "\u2747 Information about {}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VInfo.Add("Level: {} Experience: {}/{}", pGuild->GetLevel(), pGuild->GetExperience(), ExpNeed);
	VInfo.Add("Members: {} of {}", MemberUsedSlots, MemberMaxSlots);
	VInfo.AddLine(ClientID);

	if(HasHouse)
	{
		// Rent information
		char aBufTimeStamp[64];
		CVoteWrapper VRent(ClientID);
		VRent.Add("\u2679 House rent price per day: {c} golds", pGuild->GetHouse()->GetRentPrice());
		pGuild->GetHouse()->GetRentTimeStamp(aBufTimeStamp, sizeof(aBufTimeStamp));
		VRent.Add("Approximate rental time: {}", aBufTimeStamp);
		VRent.AddLine();
	}

	// Guild deposit
	CVoteWrapper VDeposit(ClientID, VWF_SEPARATE_OPEN, "\u2727 Your: {c} | Bank: {c} golds", pPlayer->GetItem(itGold)->GetValue(), pGuild->GetBank()->Get());
	VDeposit.AddOption("GUILD_DEPOSIT_GOLD", "Deposit. (Amount in a reason)");
	VDeposit.AddLine();

	// Guild management
	CVoteWrapper VManagement(ClientID, VWF_SEPARATE_OPEN, "\u262B Guild Management");
	VManagement.AddMenu(MENU_GUILD_MEMBERSHIP, "Membership list");
	VManagement.AddMenu(MENU_GUILD_INVITES, "Requests membership");
	VManagement.AddMenu(MENU_GUILD_LOGS, "Logs of activity");
	VManagement.AddMenu(MENU_GUILD_RANKS, "Rank management");
	VManagement.AddMenu(MENU_GUILD_WARS, "Guild wars");
	VManagement.AddLine();

	// Guild append house menu
	if(HasHouse)
	{
		CGuildHouseData* pHouse = pGuild->GetHouse();

		// House management
		CVoteWrapper VHouse(ClientID, VWF_SEPARATE_OPEN, "\u2302 House Management");
		VHouse.AddOption("GUILD_HOUSE_DECORATION", "Decoration editor");
		VHouse.AddOption("GUILD_HOUSE_SPAWN", "Move to the house");
		VHouse.AddOption("GUILD_HOUSE_SELL", "Sell the house");
		VHouse.AddLine();

		// House doors
		if(!pHouse->GetDoorManager()->GetContainer().empty())
		{
			CVoteWrapper VDoors(ClientID, VWF_SEPARATE_OPEN, "\u2743 House has {c} controlled door's", (int)pHouse->GetDoorManager()->GetContainer().size());
			for(auto& [Number, DoorData] : pHouse->GetDoorManager()->GetContainer())
			{
				bool StateDoor = DoorData->IsClosed();
				VDoors.AddOption("GUILD_HOUSE_DOOR", Number, "Door {} {}", StateDoor ? "Open" : "Close", DoorData->GetName());
			}
			VDoors.AddLine();
		}

		// House plant zones
		if(!pHouse->GetPlantzonesManager()->GetContainer().empty())
		{
			CVoteWrapper VPlantZones(ClientID, VWF_SEPARATE_OPEN, "\u2741 House has {c} plant zone's", (int)pHouse->GetPlantzonesManager()->GetContainer().size());
			for(auto& [ID, Plantzone] : pHouse->GetPlantzonesManager()->GetContainer())
			{
				VPlantZones.AddMenu(MENU_GUILD_HOUSE_PLANT_ZONE_SELECTED, ID, "Plant zone {} / {}", Plantzone.GetName(), GS()->GetItemInfo(Plantzone.GetItemID())->GetName());
			}
			VPlantZones.AddLine();
		}
	}

	// Guild disband
	CVoteWrapper VDisband(ClientID, VWF_SEPARATE_OPEN, "\u2716 Disband guild");
	VDisband.Add("Gold spent on upgrades will not be refunded");
	VDisband.Add("All gold will be returned to the leader only");
	VDisband.AddOption("GUILD_DISBAND", "Disband. (reason 55428)");
	VDisband.AddLine(ClientID);

	// Guild upgrades
	CVoteWrapper VUpgrades(ClientID, VWF_SEPARATE_OPEN, "\u2730 Guild upgrades");
	for(int i = CGuildData::UPGRADE_AVAILABLE_SLOTS; i < CGuildData::NUM_GUILD_UPGRADES; i++)
	{
		if(i == CGuildData::UPGRADE_CHAIR_EXPERIENCE && !HasHouse)
			continue;

		const auto* pUpgrade = pGuild->GetUpgrades(i);
		int Price = pUpgrade->m_Value * (i == CGuildData::UPGRADE_AVAILABLE_SLOTS ? g_Config.m_SvPriceUpgradeGuildSlot : g_Config.m_SvPriceUpgradeGuildAnother);
		VUpgrades.AddOption("GUILD_UPGRADE", i, "Upgrade {} ({}) {c}gold", pUpgrade->getDescription(), pUpgrade->m_Value, Price);
	}

	// Add backpage
	CVoteWrapper::AddBackpage(ClientID);
}

void CGuildManager::ShowRanksSettings(int ClientID) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// If the player is not in a guild
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	// Guild rank information
	CVoteWrapper VRanks(ClientID, VWF_SEPARATE_CLOSED|VWF_STYLE_SIMPLE, "\u2324 Rank management (Information)");
	VRanks.Add("Use reason how enter Value, Click fields!");
	VRanks.Add("Example: Name rank: [], in reason name, and use this");
	VRanks.Add("For leader access full, ignored ranks");
	VRanks.Add("Maximal 5 ranks for one guild");
	CVoteWrapper::AddLine(ClientID);

	CVoteWrapper VRankSettings(ClientID, VWF_SEPARATE_OPEN, "\u2743 Rank management (Settings)");
	VRankSettings.AddOption("GUILD_RANK_NAME_FIELD", "Name rank: {}", pPlayer->GetTempData().m_aRankGuildBuf);
	VRankSettings.AddOption("GUILD_RANK_CREATE", "Create new rank");
	CVoteWrapper::AddLine(ClientID);

	// Guild rank list
	for(auto pRank : pPlayer->Account()->GetGuild()->GetRanks()->GetContainer())
	{
		GuildRankIdentifier ID = pRank->GetID();
		bool IsDefaultRank = (pRank == pPlayer->Account()->GetGuild()->GetRanks()->GetDefaultRank());
		std::string StrAppendRankInfo = IsDefaultRank ? "- Beginning" : "- " + std::string(pRank->GetAccessName());

		CVoteWrapper VRank(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "\u2742 Rank [{}] {}", pRank->GetName(), StrAppendRankInfo.c_str());
		VRank.AddOption("GUILD_RANK_RENAME", ID, "Rename to ({})", pPlayer->GetTempData().m_aRankGuildBuf);
		VRank.AddIfOption(!IsDefaultRank, "GUILD_RANK_ACCESS", ID, "Change access", pRank->GetAccessName());
		VRank.AddIfOption(!IsDefaultRank, "GUILD_RANK_REMOVE", ID, "Remove");
	}

	// Add the votes to the player's back page
	CVoteWrapper::AddBackpage(ClientID);
}

void CGuildManager::ShowRequests(int ClientID) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// If the player is not in a guild
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	// If there are requests in the container
	const GuildRequestsContainer& aRequest = pGuild->GetMembers()->GetRequests()->GetContainer();
	if(!aRequest.empty())
	{
		for(const auto& pRequest : aRequest)
		{
			CVoteWrapper VRequest(ClientID, VWF_UNIQUE, "Request from {}", Server()->GetAccountNickname(pRequest->GetFromUID()));
			VRequest.AddOption("GUILD_REQUESTS_ACCEPT", pRequest->GetFromUID(), "Accept");
			VRequest.AddOption("GUILD_REQUESTS_DENY", pRequest->GetFromUID(), "Deny");
		}
	}
	else
	{
		// Show a message indicating that there are no requests
		CVoteWrapper(ClientID).Add("Requests is empty");
	}

	// Add the votes to the player's back page
	CVoteWrapper::AddBackpage(ClientID);
}

void CGuildManager::ShowPlantZone(int ClientID, int PlantzoneID) const
{
	// Check player
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// Check some guild data
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild || !pGuild->GetHouse() || !pGuild->GetHouse()->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID))
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	// information
	CVoteWrapper VPlantInfo(ClientID, VWF_SEPARATE_CLOSED, "\u2741 Plant zones information");
	VPlantInfo.Add("You can plant some kind of plantation.");
	CVoteWrapper::AddLine(ClientID);

	// settings
	CGuildHouseData* pHouse = pGuild->GetHouse();
	CGuildHousePlantzoneData* pPlantzone = pHouse->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID);
	CItemDescription* pItem = GS()->GetItemInfo(pPlantzone->GetItemID());
	CVoteWrapper VPlantSettings(ClientID, VWF_STYLE_STRICT_BOLD);
	VPlantSettings.Add("\u2741 Plant zone: {}", pPlantzone->GetName());
	VPlantSettings.Add("Planted: {}", pItem->GetName());
	VPlantSettings.AddLine();

	// items
	CVoteWrapper VPlantItems(ClientID, VWF_SEPARATE_OPEN, "\u2741 Possible items for planting");
	std::vector<ItemIdentifier> vItems = Core()->InventoryManager()->GetItemIDsCollectionByFunction(ItemFunctional::FUNCTION_PLANT);
	for(auto& ID : vItems)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(ID);
		if(pPlayerItem->HasItem())
			VPlantItems.AddOption("GUILD_HOUSE_PLANT_ZONE_TRY", PlantzoneID, ID, "Try plant {} (has {c})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
	}
	if(VPlantItems.IsEmpty())
		VPlantItems.Add("You have no plants for planting");

	// Add the votes to the player's back page
	CVoteWrapper::AddBackpage(ClientID);
}

void CGuildManager::ShowFinder(int ClientID) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	CVoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Guild finder information");
	VInfo.Add("You can find a guild by name or select from the list");
	VInfo.AddLine();

	// Check if the player already has a guild
	if(pPlayer->Account()->HasGuild())
	{
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		CVoteWrapper(ClientID).Add("You already in guild '{}'!", pGuild->GetName());
		CVoteWrapper::AddLine(ClientID);
	}

	// Show search option
	CVoteWrapper VSearch(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2732 Guild finder");
	VSearch.MarkList().Add("Find guild by name:");
	{
		VSearch.BeginDepth();
		VSearch.AddOption("GUILD_FINDER_SEARCH_FIELD", "Field: [{}]", pPlayer->GetTempData().m_aGuildSearchBuf);
		VSearch.EndDepth();
	}
	VSearch.AddLine();

	// Iterate through all guilds
	VSearch.MarkList().Add("Guild list:");
	{
		VSearch.BeginDepth();
		for(auto& pGuild : CGuildData::Data())
		{
			int OwnerUID = pGuild->GetLeaderUID();
			VSearch.AddMenu(MENU_GUILD_FINDER_SELECTED, pGuild->GetID(), "{} (leader {})", pGuild->GetName(), Server()->GetAccountNickname(OwnerUID));
		}
		VSearch.EndDepth();
	}
	VSearch.AddLine();

	// Add votes to the player's back page
	CVoteWrapper::AddBackpage(ClientID);
}

void CGuildManager::ShowFinderDetailInformation(int ClientID, GuildIdentifier ID) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	if(CGuildData* pGuild = GetGuildByID(ID))
	{
		// Detail information
		auto CurrentSlots = pGuild->GetMembers()->GetCurrentSlots();
		CVoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2723 Membership information", pGuild->GetName());
		VInfo.Add("Guild name: {}", pGuild->GetName());
		VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
		VInfo.Add("Members: {} of {}", CurrentSlots.first, CurrentSlots.second);
		VInfo.Add("Has house: {}", pGuild->HasHouse() ? "Yes" : "No");
		VInfo.Add("Guild bank: {c} golds", pGuild->GetBank()->Get());
		VInfo.AddIfOption(!pPlayer->Account()->HasGuild(), "GUILD_JOIN_REQUEST", pGuild->GetID(), pPlayer->Account()->GetID(), "Send request to join");
		VInfo.AddLine();

		// Memberlist
		CVoteWrapper VMemberlist(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2635 Membership list of {}", pGuild->GetName());
		for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
		{
			CGuildMemberData* pMember = pIterMember.second;
			VMemberlist.Add("{} {} Deposit: {c}", pMember->GetRank()->GetName(), Server()->GetAccountNickname(pMember->GetAccountID()), pMember->GetDeposit());
		}
		VMemberlist.AddLine();
	}

	// Add the votes backpage for the player
	CVoteWrapper::AddBackpage(ClientID);
}

void CGuildManager::ShowBuyHouse(int ClientID, CGuildHouseData* pHouse) const
{
	// If the player object does not exist, return from the function
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// Show information about guild house
	CVoteWrapper VHouseInfo(ClientID, VWF_SEPARATE_CLOSED, "Information guild house");
	VHouseInfo.Add("Buying a house you will need to constantly the Treasury");
	VHouseInfo.Add("In the intervals of time will be paid house");
	CVoteWrapper::AddLine(ClientID);

	// Check if house is available for sale
	CVoteWrapper VHouse(ClientID, VWF_SEPARATE_OPEN, "House detail information");
	if(!pHouse)
	{
		VHouse.Add("This house is not for sale yet");
		return;
	}

	VHouse.Add("House owned by: {}", pHouse->GetOwnerName());
	VHouse.Add("House price: {c} gold", pHouse->GetPrice());
	VHouse.Add("House rent price per day: {c} gold", pHouse->GetRentPrice());
	VHouse.Add("House has {c} plant zone's", (int)pHouse->GetPlantzonesManager()->GetContainer().size());
	VHouse.Add("House has {c} controlled door's", (int)pHouse->GetDoorManager()->GetContainer().size());
	CVoteWrapper::AddLine(ClientID);

	// Check if house is not purchased
	if(!pHouse->IsPurchased())
	{
		// Check if the player does not have a guild
		if(!pPlayer->Account()->HasGuild())
		{
			CVoteWrapper(ClientID).Add("You need to be in a guild to buy a house");
			return;
		}

		// Check if player has leader rights in the guild
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		CVoteWrapper VBuyHouse(ClientID, VWF_SEPARATE_OPEN, "Guild bank: {c} gold", pGuild->GetBank()->Get());
		if(pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
			VBuyHouse.AddOption("GUILD_HOUSE_BUY", pHouse->GetID(), "Purchase this guild house! Cost: {c} golds", pHouse->GetPrice());
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
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	// TODO
	CVoteWrapper VWar(ClientID);
	VWar.Add("\u2646 Declare war on another guild");
	VWar.Add("Cooldown: {} minutes", 10);
	VWar.AddLine();

	if(pGuild->GetWar())
	{
		CGuildWarData* pWar = pGuild->GetWar();
		CGuildData* pTargetGuild = pWar->GetTargetGuild();

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

		CVoteWrapper VWarList(ClientID, "List of guilds to declare war");
		for(auto& p : CGuildData::Data())
		{
			if(p->GetID() != pGuild->GetID())
				VWarList.AddOption("GUILD_DECLARE_WAR", p->GetID(), "{} (online {} players)", p->GetName(), p->GetMembers()->GetOnlinePlayersCount());
		}
	}

	// Add the votes backpage for the player
	CVoteWrapper::AddBackpage(ClientID);
}

// Function to show guild logs for a specific player
void CGuildManager::ShowLogs(int ClientID) const
{
	// If player does not exist or does not have a guild, return
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// Check some guild data
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
	{
		CVoteWrapper::AddBackpage(ClientID);
		return;
	}

	// Get the logger for the player's guild
	CGuildLoggerManager* pLogger = pGuild->GetLogger();

	// Display guild activity log settings to the player
	auto flagStatus = [&](int flag) { return pLogger->IsActivityFlagSet(flag) ? "[\u2714]" : "[\u2715]"; };
	CVoteWrapper VLogger(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Activity log settings");
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_GUILD_FULL, "Full changes {}", flagStatus(LOGFLAG_GUILD_FULL));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_GUILD_MAIN_CHANGES, "Main changes {}", flagStatus(LOGFLAG_GUILD_MAIN_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_MEMBERS_CHANGES, "Members changes {}", flagStatus(LOGFLAG_MEMBERS_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_BANK_CHANGES, "Bank changes {}", flagStatus(LOGFLAG_BANK_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_RANKS_CHANGES, "Ranks changes {}", flagStatus(LOGFLAG_RANKS_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_UPGRADES_CHANGES, "Upgrades changes {}", flagStatus(LOGFLAG_UPGRADES_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_HOUSE_MAIN_CHANGES, "House main changes {}", flagStatus(LOGFLAG_HOUSE_MAIN_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_HOUSE_DOORS_CHANGES, "House doors changes {}", flagStatus(LOGFLAG_HOUSE_DOORS_CHANGES));
	VLogger.AddOption("GUILD_LOGGER_SET", LOGFLAG_HOUSE_DECORATIONS_CHANGES, "House decorations changes {}", flagStatus(LOGFLAG_HOUSE_DECORATIONS_CHANGES));
	CVoteWrapper::AddLine(ClientID);

	// Logs
	char aBuf[128];
	CVoteWrapper VLogs(ClientID, VWF_SEPARATE_CLOSED, "Logs");
	const GuildLogContainer& aLogs = pGuild->GetLogger()->GetContainer();
	for(const auto& pLog : aLogs)
	{
		// Format the log entry and display it
		str_format(aBuf, sizeof(aBuf), "[%s] %s", pLog.m_Time.c_str(), pLog.m_Text.c_str());
		VLogs.Add("{}", aBuf);
	}

	// Add the votes backpage for the player
	CVoteWrapper::AddBackpage(ClientID);
}

CGuildHouseData* CGuildManager::GetGuildHouseByID(const GuildHouseIdentifier& ID) const
{
	auto itHouse = std::find_if(CGuildHouseData::Data().begin(), CGuildHouseData::Data().end(), [&ID](const CGuildHouseData* p)
	{
		return ID == p->GetID();
	});

	return itHouse != CGuildHouseData::Data().end() ? *itHouse : nullptr;
}

CGuildHouseData* CGuildManager::GetGuildHouseByPos(vec2 Pos) const
{
	auto itHouse = std::find_if(CGuildHouseData::Data().begin(), CGuildHouseData::Data().end(), [&Pos, this](const CGuildHouseData* p)
	{
		return GS()->GetWorldID() == p->GetWorldID() && distance(Pos, p->GetPos()) < p->GetRadius();
	});

	return itHouse != CGuildHouseData::Data().end() ? *itHouse : nullptr;
}

CGuildData* CGuildManager::GetGuildByID(GuildIdentifier ID) const
{
	auto itGuild = std::find_if(CGuildData::Data().begin(), CGuildData::Data().end(), [&ID](CGuildData* p)
	{
		return p->GetID() == ID;
	});

	return itGuild != CGuildData::Data().end() ? (*itGuild) : nullptr;
}

CGuildData* CGuildManager::GetGuildByName(const char* pGuildname) const
{
	auto itGuild = std::find_if(CGuildData::Data().begin(), CGuildData::Data().end(), [&pGuildname](CGuildData* p)
	{
		return str_comp_nocase(p->GetName(), pGuildname) == 0;
	});

	return itGuild != CGuildData::Data().end() ? (*itGuild) : nullptr;
}

CGuildHousePlantzoneData* CGuildManager::GetGuildHousePlantzoneByPos(vec2 Pos) const
{
	for(auto& p : CGuildHouseData::Data())
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
	return CGuildData::IsAccountMemberGuild(AccountID);
}
