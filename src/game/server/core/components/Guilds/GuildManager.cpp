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
		std::string JsPropersties = pRes->getString("Properties").c_str();

		CGuildData* pGuild = GetGuildByID(GuildID);
		CGuildHouseData::CreateElement(ID)->Init(pGuild, Price, GS()->GetWorldID(), std::move(JsPropersties));
	}

	Core()->ShowLoadingProgress("Guild houses", CGuildHouseData::Data().size());
}

void CGuildManager::OnTick()
{
	// Check if the current world ID is not equal to the main world (once use House get instance object self world id) ID and current tick
	if(GS()->GetWorldID() != MAIN_WORLD_ID || (Server()->Tick() % Server()->TickSpeed() != 0))
		return;

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
		_DEF_TILE_ENTER_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->UpdateVotes(ClientID, MENU_MAIN);
		return true;
	}
	if(pChr->GetHelper()->TileExit(IndexCollision, TILE_GUILD_HOUSE))
	{
		_DEF_TILE_EXIT_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->UpdateVotes(ClientID, MENU_MAIN);
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
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
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
			GS()->StrongUpdateVotesForAll(MENU_GUILD_MEMBERSHIP_LIST);
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
		GS()->StrongUpdateVotesForAll(MENU_GUILD_MEMBERSHIP_LIST);
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
			GS()->StrongUpdateVotesForAll(MENU_GUILD_MEMBERSHIP_LIST);
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
			GS()->StrongUpdateVotes(ClientID, MENU_GUILD);
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
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
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
			GS()->Chat(ClientID, "Rank limit reached, {INT} out of {INT}", (int)MAX_GUILD_RANK_NUM, (int)MAX_GUILD_RANK_NUM);
		}
		else if(Result == GUILD_RANK_RESULT::WRONG_NUMBER_OF_CHAR_IN_NAME)
		{
			GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16.");
			GS()->Chat(ClientID, "You may be using unauthorized characters in the name, like '");
		}
		else if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->Chat(ClientID, "The rank '{STR}' has been successfully added!", pPlayer->GetTempData().m_aRankGuildBuf);
			GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
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
			GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
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
			GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
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
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
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
			GS()->StrongUpdateVotesForAll(MENU_GUILD_MEMBERSHIP_LIST);
			GS()->StrongUpdateVotesForAll(MENU_GUILD_INVITES);
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
		GS()->StrongUpdateVotesForAll(MENU_GUILD_MEMBERSHIP_LIST);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_INVITES);
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
		GS()->UpdateVotes(ClientID, MENU_GUILD_FINDER);
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
			GS()->Chat(ClientID, "You sent a request to join the {STR} guild.", pGuild->GetName());
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
			GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
			GS()->StrongUpdateVotesForAll(MENU_GUILD);
		}

		return true;
	}

	if(PPSTR(CMD, "GUILD_HOUSE_DECORATION_EDIT") == 0)
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

		// check distance between player and house
		if(distance(pHouse->GetPos(), pPlayer->GetCharacter()->m_Core.m_Pos) > 600)
		{
			GS()->Chat(ClientID, "You have a lot of distance between you and the house, try to get closer!");
			return true;
		}

		// start custom vote
		GS()->StartCustomVotes(ClientID, MENU_GUILD_HOUSE_DECORATION);
		GS()->AV(ClientID, "null", "Please close vote and press Left Mouse,");
		GS()->AV(ClientID, "null", "on position where add decoration!");
		GS()->AddVotesBackpage(ClientID);
		GS()->EndCustomVotes(ClientID);
		// end custom vote

		// set temp data
		if(pHouse->GetDecorations()->StartDrawing(pPlayer))
		{
			GS()->Chat(ClientID, "You start drawing mode!");
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
		pHouse->GetDoors()->Reverse(UniqueDoorID);
		GS()->StrongUpdateVotesForAll(MENU_GUILD);
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
			GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
			GS()->StrongUpdateVotesForAll(MENU_GUILD);
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

		GS()->StrongUpdateVotesForAll(MENU_GUILD);
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
		GS()->StrongUpdateVotesForAll(MENU_GUILD_LOGS);
		return true;
	}

	return false;
}

bool CGuildManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			return false;

		if(pChr->GetHelper()->BoolIndex(TILE_GUILD_HOUSE))
		{
			CGuildHouseData* pHouse = GetGuildHouseByPos(pChr->m_Core.m_Pos);
			ShowBuyHouse(pPlayer, pHouse);
			return true;
		}

		return false;
	}

	if(Menulist == MENU_GUILD_FINDER)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;
		ShowFinder(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_FINDER_MEMBERSHIP_LIST)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD_FINDER;
		ShowPlayerlist(pPlayer, pPlayer->m_TempMenuValue);
		return true;
	}

	if(Menulist == MENU_GUILD)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;
		ShowMenu(pPlayer);
		return true;
	}

	if(Menulist == MENU_GUILD_MEMBERSHIP_LIST)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowPlayerlist(pPlayer);
		return true;
	}


	if(Menulist == MENU_GUILD_LOGS)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowLogs(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_RANK)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowRanksSettings(pPlayer);
		return true;
	}

	if(Menulist == MENU_GUILD_INVITES)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowRequests(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_HOUSE_DECORATION)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		GS()->AVH(ClientID, TAB_INFO_DECORATION, "Decorations Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "Add: SELECT your item in list. SELECT (Add to house),");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "later press (ESC) and mouse select position");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "Return in inventory: SELECT down your decorations");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "and press (Back to inventory).");

		Core()->InventoryManager()->ListInventory(ClientID, ItemType::TYPE_DECORATION);
		GS()->AV(ClientID, "null");
		//ShowDecorationList(pPlayer);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

void CGuildManager::ShowPlayerlist(CPlayer* pPlayer) const
{
	// Check if the player or the player's account has a guild
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	// Initialize variables
	int HideID = START_SELF_HIDE_ID;
	int ClientID = pPlayer->GetCID();
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	CGuildMemberData* pPlayerMember = pPlayer->Account()->GetGuildMemberData();

	// Print the membership list of the guild
	GS()->AVL(ClientID, "null", "Membership list of {STR}", pGuild->GetName());

	// Loop through each member in the guild
	for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
	{
		CGuildMemberData* pMember = pIterMember.second;
		const int& MemberUID = pMember->GetAccountID();
		const char* pNickname = Server()->GetAccountNickname(MemberUID);

		// Print the member's rank, account nickname, and deposit
		GS()->AVH(ClientID, HideID, "{STR} {STR} Deposit: {VAL}", pMember->GetRank()->GetName(), pNickname, pMember->GetDeposit());
		{
			bool CanInteract = false;
			const bool IsSelfSlot = pPlayer->Account()->GetID() == pMember->GetAccountID();

			// Check if the player has leader rights
			if(pPlayerMember->CheckAccess(RIGHTS_LEADER))
			{
				// Loop through each rank in the guild
				for(auto& pRank : pGuild->GetRanks()->GetContainer())
				{
					// Check if the member's rank is different from the current rank
					if(pMember->GetRank()->GetID() != pRank->GetID())
					{
						// Print the option to change the member's rank
						GS()->AVD(ClientID, "GUILD_CHANGE_PLAYER_RANK", MemberUID, pRank->GetID(), HideID, "Change rank to: {STR}",
							pRank->GetName(), pRank->GetAccess() > RIGHTS_DEFAULT ? "*" : "");
						CanInteract = true;
					}
				}

				// Check if the member is not the player themselves
				if(!IsSelfSlot)
				{
					// Print the option to give the leader role to the member
					GS()->AVM(ClientID, "GUILD_SET_NEW_LEADER", MemberUID, HideID, "Give Leader (in reason 134)");
					CanInteract = true;
				}
			}

			// Check if the player has invite/kick rights
			if(pPlayerMember->CheckAccess(RIGHTS_INVITE_KICK))
			{
				// Check if the member is not the player themselves
				if(!IsSelfSlot)
				{
					// Print the option to kick the member
					GS()->AVM(ClientID, "GUILD_KICK_PLAYER", MemberUID, HideID, "Kick");
					CanInteract = true;
				}
			}

			// Check if there are no available actions for the member
			if(!CanInteract)
			{
				GS()->AVM(ClientID, "null", MemberUID, HideID, "No available actions on the player");
			}
		}

		HideID++;
	}

	// Add the votes backpage for the player
	GS()->AddVotesBackpage(ClientID);
}

void CGuildManager::ShowPlayerlist(CPlayer* pPlayer, GuildIdentifier ID) const
{
	if(!pPlayer)
		return;

	// show more information
	if(pPlayer->Account()->HasGuild() && pPlayer->Account()->GetGuild()->GetID() == ID)
	{
		ShowPlayerlist(pPlayer);
		return;
	}

	// show simple information
	int ClientID = pPlayer->GetCID();
	if(CGuildData* pGuild = GetGuildByID(ID))
	{
		int HideID = START_SELF_HIDE_ID;

		GS()->AVL(ClientID, "null", "Membership list of {STR}", pGuild->GetName());
		for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
		{
			CGuildMemberData* pMember = pIterMember.second;
			GS()->AVL(ClientID, "null", "{STR} {STR} Deposit: {VAL}",
				pMember->GetRank()->GetName(), Server()->GetAccountNickname(pMember->GetAccountID()), pMember->GetDeposit());
			HideID++;
		}
	}

	// Add the votes backpage for the player
	GS()->AddVotesBackpage(ClientID);
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
	GS()->Chat(-1, "New guilds [{STR}] have been created!", GuildName.cstr());
	GS()->StrongUpdateVotes(ClientID, MENU_MAIN);
}

void CGuildManager::Disband(GuildIdentifier ID) const
{
	// Find the guild with the given ID
	auto pIterGuild = std::find_if(CGuildData::Data().begin(), CGuildData::Data().end(), [&ID](CGuildData* pGuild) { return pGuild->GetID() == ID; });
	if(!(*pIterGuild))
		return;

	// Get a pointer to the guild
	CGuildData* pGuild = (*pIterGuild);

	// If the guild has a house, sell it
	if(pGuild->SellHouse())
	{
		GS()->Chat(-1, "The guild {STR} has lost house.", pGuild->GetName());
	}

	// Calculate the amount of gold to return to the guild leader
	const int ReturnsGold = maximum(1, pGuild->GetBank()->Get());
	GS()->SendInbox("System", pGuild->GetLeaderUID(), "Your guild was disbanded.", "We returned some gold from your guild.", itGold, ReturnsGold);
	GS()->Chat(-1, "The {STR} guild has been disbanded.", pGuild->GetName());

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


void CGuildManager::ShowMenu(CPlayer* pPlayer) const
{
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	int ClientID = pPlayer->GetCID();
	bool HasHouse = pGuild->HasHouse();
	int ExpNeed = computeExperience(pGuild->GetLevel());
	const int MemberUsedSlots = pGuild->GetMembers()->GetContainer().size();
	const int MemberMaxSlots = pGuild->GetUpgrades(CGuildData::UPGRADE_AVAILABLE_SLOTS)->m_Value;

	GS()->AVH(ClientID, TAB_GUILD_STAT, "\u2747 Information about {STR}", pGuild->GetName());
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Leader: {STR}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Level: {INT} Experience: {INT}/{INT}", pGuild->GetLevel(), pGuild->GetExperience(), ExpNeed);
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Members: {INT} of {INT}", MemberUsedSlots, MemberMaxSlots);
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "\u2727 Your: {VAL} | Bank: {VAL} golds", pPlayer->GetItem(itGold)->GetValue(), pGuild->GetBank()->Get());
	GS()->AVL(ClientID, "GUILD_DEPOSIT_GOLD", "Deposit. (Amount in a reason)", pGuild->GetName());
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "\u262B Guild Management");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_MEMBERSHIP_LIST, NOPE, "Membership list");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_INVITES, NOPE, "Requests membership");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_LOGS, NOPE, "Logs of activity");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_RANK, NOPE, "Rank management");
	if(HasHouse)
	{
		CGuildHouseData* pHouse = pGuild->GetHouse();

		GS()->AV(ClientID, "null");
		GS()->AVL(ClientID, "null", "\u2302 House Management");
		GS()->AVM(ClientID, "MENU", MENU_GUILD_HOUSE_DECORATION, NOPE, "Customizing");
		GS()->AVL(ClientID, "GUILD_HOUSE_SPAWN", "Move to the house");
		GS()->AVL(ClientID, "GUILD_HOUSE_SELL", "Sell the house");

		GS()->AV(ClientID, "null");
		GS()->AVL(ClientID, "null", "\u2747 House has {VAL} controlled door's", (int)pHouse->GetDoors()->GetContainer().size());
		for(auto& [Number, DoorData] : pHouse->GetDoors()->GetContainer())
		{
			bool StateDoor = DoorData->IsClosed();
			GS()->AVM(ClientID, "GUILD_HOUSE_DOOR", Number, NOPE, "{STR} {STR} door", StateDoor ? "Open" : "Close", DoorData->GetName());
		}
	}
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "\u2716 Disband guild");
	GS()->AVL(ClientID, "null", "Gold spent on upgrades will not be refunded");
	GS()->AVL(ClientID, "null", "All gold will be returned to the leader only");
	GS()->AVL(ClientID, "GUILD_DISBAND", "Disband (reason 55428)");
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "☆ Guild upgrades");
	for(int i = CGuildData::UPGRADE_AVAILABLE_SLOTS; i < CGuildData::NUM_GUILD_UPGRADES; i++)
	{
		if(i == CGuildData::UPGRADE_CHAIR_EXPERIENCE && !HasHouse)
			continue;

		const auto* pUpgrade = pGuild->GetUpgrades(i);
		int Price = pUpgrade->m_Value * (i == CGuildData::UPGRADE_AVAILABLE_SLOTS ? g_Config.m_SvPriceUpgradeGuildSlot : g_Config.m_SvPriceUpgradeGuildAnother);
		GS()->AVM(ClientID, "GUILD_UPGRADE", i, NOPE, "Upgrade {STR} ({INT}) {VAL}gold", pUpgrade->getDescription(), pUpgrade->m_Value, Price);
	}

	GS()->AddVotesBackpage(ClientID);
}

void CGuildManager::ShowRanksSettings(CPlayer* pPlayer) const
{
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	const int ClientID = pPlayer->GetCID();

	pPlayer->m_LastVoteMenu = MENU_GUILD;
	GS()->AV(ClientID, "null", "Use reason how enter Value, Click fields!");
	GS()->AV(ClientID, "null", "Example: Name rank: [], in reason name, and use this");
	GS()->AV(ClientID, "null", "For leader access full, ignored ranks");
	GS()->AV(ClientID, "null", "- - - - - - - - - -");
	GS()->AV(ClientID, "null", "- Maximal 5 ranks for one guild");
	GS()->AVM(ClientID, "GUILD_RANK_NAME_FIELD", 1, NOPE, "Name rank: {STR}", pPlayer->GetTempData().m_aRankGuildBuf);
	GS()->AVM(ClientID, "GUILD_RANK_CREATE", 1, NOPE, "Create new rank");
	GS()->AV(ClientID, "null");

	int HideID = START_SELF_HIDE_ID;
	for(auto pRank : pPlayer->Account()->GetGuild()->GetRanks()->GetContainer())
	{
		GuildRankIdentifier ID = pRank->GetID();
		bool IsDefaultRank = (pRank == pPlayer->Account()->GetGuild()->GetRanks()->GetDefaultRank());
		std::string StrAppendRankInfo = IsDefaultRank ? "- Beginning" : "- " + std::string(pRank->GetAccessName());

		GS()->AVH(ClientID, HideID, "Rank [{STR}] {STR}", pRank->GetName(), StrAppendRankInfo.c_str());
		GS()->AVM(ClientID, "GUILD_RANK_RENAME", ID, HideID, "Rename to ({STR})", pPlayer->GetTempData().m_aRankGuildBuf);

		if(!IsDefaultRank)
		{
			GS()->AVM(ClientID, "GUILD_RANK_ACCESS", ID, HideID, "Change access", pRank->GetAccessName());
			GS()->AVM(ClientID, "GUILD_RANK_REMOVE", ID, HideID, "Remove");
		}
		HideID++;
	}

	GS()->AddVotesBackpage(ClientID);
}

void CGuildManager::ShowRequests(int ClientID) const
{
	// If the player object doesn't exist or the player doesn't have a guild, return
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	// Get the guild object for the player's account
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	const GuildRequestsContainer& aRequest = pGuild->GetMembers()->GetRequests()->GetContainer();

	// If there are requests in the container
	if(!aRequest.empty())
	{
		// Start with a hide ID and iterate through each request in the container
		int HideID = START_SELF_HIDE_ID;
		for(const auto& pRequest : aRequest)
		{
			// Show a vote to accept or deny the request
			GS()->AVH(ClientID, HideID, "{STR} request to join", Server()->GetAccountNickname(pRequest->GetFromUID()));
			{
				GS()->AVM(ClientID, "GUILD_REQUESTS_ACCEPT", pRequest->GetFromUID(), HideID, "Accept");
				GS()->AVM(ClientID, "GUILD_REQUESTS_DENY", pRequest->GetFromUID(), HideID, "Deny");
			}

			// Increment the hide ID
			HideID++;
		}
	}
	else
	{
		// Show a message indicating that there are no requests
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Requests is empty");
	}

	// Add the votes to the player's back page
	GS()->AddVotesBackpage(ClientID);
}

void CGuildManager::ShowFinder(int ClientID) const
{
	// Get the player
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);

	// Check if the player already has a guild
	if(pPlayer->Account()->HasGuild())
	{
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		GS()->AVL(ClientID, "null", "\u02DA\u029A\u2665\u025E\u02DA You already in guild '{STR}'!", pGuild->GetName());
		GS()->AV(ClientID, "null");
	}

	// Show instructions to the player
	GS()->AV(ClientID, "null", "Use reason how Value.");
	GS()->AV(ClientID, "null", "Example: Find guild: [], in reason name.");
	GS()->AV(ClientID, "null");

	// Show search option
	GS()->AV(ClientID, "null", "\u270E Search for a guild by name.");
	GS()->AVM(ClientID, "GUILD_FINDER_SEARCH_FIELD", 1, NOPE, "Find guild: [{STR}]", pPlayer->GetTempData().m_aGuildSearchBuf);
	GS()->AV(ClientID, "null");

	// Iterate through all guilds
	int HideID = START_SELF_HIDE_ID;
	for(auto& pGuild : CGuildData::Data())
	{
		int OwnerUID = pGuild->GetLeaderUID();
		auto [UsedSlots, MaxSlots] = pGuild->GetMembers()->GetCurrentSlots();

		// Show guild information
		GS()->AVH(ClientID, HideID, "{STR} : Leader {STR} ({INT} of {INT} players)", pGuild->GetName(), Server()->GetAccountNickname(OwnerUID), UsedSlots, MaxSlots);

		// Show whether the guild has its own house or not
		if(pGuild->HasHouse())
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "* The guild has its own house");
		}

		// Show guild bank balance
		GS()->AVM(ClientID, "null", NOPE, HideID, "* Accumulations are: {VAL} gold's", pGuild->GetBank()->Get());

		// Show options to view player list and send join request
		GS()->AVD(ClientID, "MENU", MENU_GUILD_FINDER_MEMBERSHIP_LIST, pGuild->GetID(), HideID, "Membership list");
		if(!pPlayer->Account()->HasGuild())
		{
			GS()->AVD(ClientID, "GUILD_JOIN_REQUEST", pGuild->GetID(), pPlayer->Account()->GetID(), HideID, "Send request to join {STR}", pGuild->GetName());
		}

		HideID++;
	}

	// Add votes to the player's back page
	GS()->AddVotesBackpage(ClientID);
}

void CGuildManager::ShowBuyHouse(CPlayer* pPlayer, CGuildHouseData* pHouse) const
{
	// Check if player is valid
	if(!pPlayer)
	{
		return;
	}

	const int ClientID = pPlayer->GetCID();

	// Show information about guild house
	GS()->AVH(ClientID, TAB_INFO_GUILD_HOUSE, "Information guild house");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_GUILD_HOUSE, "Buying a house you will need to constantly the Treasury");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_GUILD_HOUSE, "In the intervals of time will be paid house");
	GS()->AV(ClientID, "null");

	// Check if house is available for sale
	if(!pHouse)
	{
		GS()->AVL(ClientID, "null", "This house is not for sale yet");
		return;
	}

	// Check if house is already purchased
	if(pHouse->IsPurchased())
	{
		CGuildData* pGuild = pHouse->GetGuild();
		GS()->AVM(ClientID, "null", NOPE, NOPE, "House owned by the guild: {STR}", pGuild->GetName());
		return;
	}

	// Check if player has a guild
	if(pPlayer->Account()->HasGuild())
	{
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Your guild have {VAL} Gold", pGuild->GetBank()->Get());

		// Check if player has leader rights in the guild
		if(pPlayer->Account()->GetGuildMemberData()->CheckAccess(RIGHTS_LEADER))
		{
			GS()->AVM(ClientID, "GUILD_HOUSE_BUY", pHouse->GetID(), NOPE, "Purchase this guild house! Cost: {VAL} golds", pHouse->GetPrice());
		}
	}
}

// Function to show guild logs for a specific player
void CGuildManager::ShowLogs(int ClientID) const
{
	// If player does not exist or does not have a guild, return
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	// Get the guild data and logger for the player's guild
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	CGuildLoggerManager* pLogger = pGuild->GetLogger();

	// Display guild activity log settings to the player
	auto flagStatus = [&](int flag) { return pLogger->IsActivityFlagSet(flag) ? "[\u2714]" : "[\u2715]"; };
	GS()->AVH(ClientID, TAB_GUILD_LOG_SETTINGS, "Guild activity log settings");
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_GUILD_FULL, TAB_GUILD_LOG_SETTINGS, "{STR} Full changes", flagStatus(LOGFLAG_GUILD_FULL));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_GUILD_MAIN_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} Main changes", flagStatus(LOGFLAG_GUILD_MAIN_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_MEMBERS_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} Members changes", flagStatus(LOGFLAG_MEMBERS_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_BANK_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} Bank changes", flagStatus(LOGFLAG_BANK_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_RANKS_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} Ranks changes", flagStatus(LOGFLAG_RANKS_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_UPGRADES_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} Upgrades changes", flagStatus(LOGFLAG_UPGRADES_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_HOUSE_MAIN_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} House main changes", flagStatus(LOGFLAG_HOUSE_MAIN_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_HOUSE_DOORS_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} House doors changes", flagStatus(LOGFLAG_HOUSE_DOORS_CHANGES));
	GS()->AVM(ClientID, "GUILD_LOGGER_SET", LOGFLAG_HOUSE_DECORATIONS_CHANGES, TAB_GUILD_LOG_SETTINGS, "{STR} House decorations changes", flagStatus(LOGFLAG_HOUSE_DECORATIONS_CHANGES));
	GS()->AV(ClientID, "null");

	// If logs exist, display them to the player
	const GuildLogContainer& aLogs = pGuild->GetLogger()->GetContainer();
	if(!aLogs.empty())
	{
		char aBuf[128];
		for(const auto& pLog : aLogs)
		{
			// Format the log entry and display it
			str_format(aBuf, sizeof(aBuf), "[%s] %s", pLog.m_Time.c_str(), pLog.m_Text.c_str());
			GS()->AVM(ClientID, "null", NOPE, NOPE, "{STR}", aBuf);
		}
	}
	else
	{
		// If logs are empty, display a message to the player
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Log is empty");
	}

	// Add the votes backpage for the player
	GS()->AddVotesBackpage(ClientID);
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

bool CGuildManager::IsAccountMemberGuild(int AccountID) const
{
	return CGuildData::IsAccountMemberGuild(AccountID);
}
