/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_manager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>
#include <game/server/core/components/mails/mail_wrapper.h>

#include "../houses/entities/house_door.h"

void CGuildManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_TABLE);
	while(pRes->next())
	{
		// initialize variables
		GuildIdentifier ID = pRes->getInt("ID");
		const auto Name = pRes->getString("Name");
		const auto JsonMembers = pRes->getString("Members");
		const auto DefaultRankID = pRes->getInt("DefaultRankID");
		const auto LeaderUID = pRes->getInt("LeaderUID");
		const auto Level = pRes->getInt("Level");
		const auto Experience = pRes->getUInt64("Exp");
		const auto Bank = pRes->getBigInt("Bank");
		const auto Score = pRes->getInt("Score");
		const auto LogFlag = pRes->getInt64("LogFlag");

		// initialize guild
		CGuild::CreateElement(ID)->Init(Name, JsonMembers, DefaultRankID, Level, Experience, Score, LeaderUID, Bank, LogFlag, &pRes);
	}

	InitWars();
}

void CGuildManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HOUSES, SqlQueryWhereWorld.c_str());
	while(pRes->next())
	{
		// initialize variables
		GuildHouseIdentifier ID = pRes->getInt("ID");
		GuildIdentifier GuildID = pRes->getInt("GuildID");
		int InitialFee = pRes->getInt("InitialFee");
		int RentDays = pRes->getInt("RentDays");
		std::string JsonDoors = pRes->getString("Doors");
		std::string JsonFarmzones = pRes->getString("Farmzones");
		std::string JsonPropersties = pRes->getString("Properties");

		// initialize guild houses
		CGuild* pGuild = GetGuildByID(GuildID);
		CGuildHouse::CreateElement(ID)->Init(pGuild, RentDays, InitialFee, GS()->GetWorldID(), std::move(JsonDoors), std::move(JsonFarmzones), std::move(JsonPropersties));
	}
}

void CGuildManager::OnTick()
{
	// check if we are in the main world
	if(GS()->GetWorldID() != MAIN_WORLD_ID)
		return;

	// update guild wars
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		for(auto& pWarHandler : CGuildWarHandler::Data())
			pWarHandler->Handle();
	}

	// update guild houses text
	if(Server()->Tick() % g_Config.m_SvUpdateEntityTextNames == 0)
	{
		for(const auto& p : CGuildHouse::Data())
			p->UpdateText(g_Config.m_SvUpdateEntityTextNames);
	}
}

void CGuildManager::OnCharacterTile(CCharacter* pChr)
{
	auto* pPlayer = pChr->GetPlayer();
	const auto ClientID = pPlayer->GetCID();

	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_GUILD_HOUSE, MOTD_MENU_GUILD_HOUSE_DETAIL)

	if(pChr->GetTiles()->IsActive(TILE_GUILD_CHAIR))
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
	}
}

bool CGuildManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// teleport to house
	if(PPSTR(pCmd, "GUILD_HOUSE_TELEPORT") == 0)
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
		if(!GS()->IsPlayerInWorld(ClientID, pHouse->GetWorldID()))
		{
			pPlayer->ChangeWorld(pHouse->GetWorldID(), pHouse->GetPos());
			return true;
		}

		// Change the player's position to pAether's position
		pPlayer->GetCharacter()->ChangePosition(pHouse->GetPos());
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// start house decoration edit
	if(PPSTR(pCmd, "GUILD_HOUSE_DECORATION") == 0)
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
	if(PPSTR(pCmd, "GUILD_SET_LEADER") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		switch(pGuild->SetLeader(Extra1))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::SET_LEADER_NON_GUILD_PLAYER: GS()->Chat(ClientID, "The player is not a member of your guild"); break;
			case GuildResult::SET_LEADER_PLAYER_ALREADY_LEADER: GS()->Chat(ClientID, "The player is already a leader"); break;
			case GuildResult::SUCCESSFUL:
				GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBER_LIST);
				pPlayer->m_VotesData.UpdateCurrentVotes();
			break;
		}
		return true;
	}

	// change member rank
	if(PPSTR(pCmd, "GUILD_CHANGE_MEMBER_RANK") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// initialize variables
		const int& MemberUID = Extra1;
		const GuildRankIdentifier& RankID = Extra2;
		auto pMember = pGuild->GetMembers()->Get(MemberUID);

		// check member valid and result from setrank
		if(!pMember || !pMember->SetRank(RankID))
		{
			GS()->Chat(ClientID, "Set a player's rank failed, try again later");
			return true;
		}

		// succesful
		GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBER_LIST);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// disband
	if(PPSTR(pCmd, "GUILD_DISBAND") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// prevent accidental pressing
		if(ReasonNumber != 55428)
		{
			GS()->Chat(ClientID, "Random Touch Security Code has not been entered correctly.");
			return true;
		}

		// disband guild
		Disband(pPlayer->Account()->GetGuild()->GetID());
		return true;
	}

	// kick member
	if(PPSTR(pCmd, "GUILD_KICK_MEMBER") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check exclude oneself
		if(pPlayer->Account()->GetID() == Extra1)
		{
			GS()->Chat(ClientID, "You can't kick yourself");
			return true;
		}

		// result
		switch(pPlayer->Account()->GetGuild()->GetMembers()->Kick(Extra1))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::MEMBER_KICK_IS_OWNER: GS()->Chat(ClientID, "You can't kick a leader"); break;
			case GuildResult::MEMBER_KICK_DOES_NOT_EXIST: GS()->Chat(ClientID, "The player is no longer on the guild membership lists"); break;
			case GuildResult::MEMBER_SUCCESSFUL:
				GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBER_LIST);
				pPlayer->m_VotesData.UpdateVotes(MENU_GUILD_MEMBER_LIST);
			break;
		}
		return true;
	}

	// deposit gold
	if(PPSTR(pCmd, "GUILD_DEPOSIT_GOLD") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild)
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// minimal 100
		if(ReasonNumber < 100)
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
		if(pMember->DepositInBank(ReasonNumber))
			pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD);
		return true;
	}

	// extend rent
	if(PPSTR(pCmd, "GUILD_HOUSE_EXTEND_RENT") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check valid house
		auto* pHouse = pGuild->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "Your guild does not have a house.");
			return true;
		}

		// check maximal rent days
		if(pHouse->GetRentDays() >= MAX_HOUSE_RENT_DAYS)
		{
			GS()->Chat(ClientID, "You can not extend the rent anymore then {# (day|days)}.", (int)MAX_HOUSE_RENT_DAYS);
			return true;
		}

		// check minimal
		ReasonNumber = clamp(ReasonNumber, 0, MAX_HOUSE_RENT_DAYS - pHouse->GetRentDays());
		if(!ReasonNumber)
		{
			GS()->Chat(ClientID, "Minimum is 1 day. Maximum {# total (day|days)}.", (int)MAX_HOUSE_RENT_DAYS);
			return true;
		}

		// extend
		if(pHouse->ExtendRentDays(ReasonNumber))
		{
			GS()->ChatGuild(pGuild->GetID(), "Your house was extended by {# total (day|days)}.", ReasonNumber);
			pGuild->GetLogger()->Add(LOGFLAG_HOUSE_MAIN_CHANGES, "House extended by %d days.", ReasonNumber);
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// failed
		GS()->Chat(ClientID, "Not enough gold.");
		return true;
	}

	// create new rank
	if(PPSTR(pCmd, "GUILD_RANK_CREATE") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check valid text from reason
		if(PPSTR(pReason, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// result
		switch(pGuild->GetRanks()->Add(pReason))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::RANK_ADD_ALREADY_EXISTS: GS()->Chat(ClientID, "The rank name already exists"); break;
			case GuildResult::RANK_ADD_LIMIT_HAS_REACHED: GS()->Chat(ClientID, "Rank limit reached, '{} out of {}'.", (int)GUILD_RANKS_MAX_COUNT, (int)GUILD_RANKS_MAX_COUNT); break;
			case GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME: GS()->Chat(ClientID, "Minimum number of 'characters 2, maximum 16'."); break;
			case GuildResult::RANK_SUCCESSFUL:
				GS()->Chat(ClientID, "The rank '{}' has been successfully added!", pReason);
				GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_LIST);
			break;
		}
		return true;
	}

	// rename rank
	if(PPSTR(pCmd, "GUILD_RANK_RENAME") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check valid text from reason
		if(PPSTR(pReason, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// check rank valid
		auto pRank = pGuild->GetRanks()->Get(Extra1);
		if(!pRank)
		{
			GS()->Chat(ClientID, "Unforeseen error.");
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// result
		switch(pRank->Rename(pReason))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::RANK_RENAME_ALREADY_NAME_EXISTS: GS()->Chat(ClientID, "The name is already in use by another rank"); break;
			case GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME: GS()->Chat(ClientID, "Minimum number of 'characters 2, maximum 16'."); break;
			case GuildResult::RANK_SUCCESSFUL: GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_SELECT); break;
		}
		return true;
	}

	// remove rank
	if(PPSTR(pCmd, "GUILD_RANK_REMOVE") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check rank valid
		auto pRank = pGuild->GetRanks()->Get(Extra1);
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
				GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_SELECT);
			break;
		}
		return true;
	}

	// set rights for rank
	if(PPSTR(pCmd, "GUILD_RANK_SET_RIGHTS") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check rank valid
		auto pRank = pGuild->GetRanks()->Get(Extra1);
		if(!pRank)
		{
			GS()->Chat(ClientID, "Unforeseen error.");
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// check for same of rights
		const auto Rights = static_cast<GuildRankRights>(Extra2);
		if(pRank->GetRights() == Rights)
		{
			GS()->Chat(ClientID, "You already have current rights set.");
			return true;
		}

		// update rights
		pRank->SetRights(Rights);
		GS()->UpdateVotesIfForAll(MENU_GUILD_RANK_SELECT);
		return true;
	}

	// request accept
	if(PPSTR(pCmd, "GUILD_REQUESTS_ACCEPT") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		switch(pGuild->GetMembers()->GetRequests()->Accept(Extra1, pPlayer->Account()->GetGuildMember()))
		{
			default: GS()->Chat(ClientID, "Unforeseen error."); break;
			case GuildResult::MEMBER_JOIN_ALREADY_IN_GUILD: GS()->Chat(ClientID, "The player is already in a guild"); break;
			case GuildResult::MEMBER_NO_AVAILABLE_SLOTS: GS()->Chat(ClientID, "No guild slots available."); break;
			case GuildResult::MEMBER_SUCCESSFUL:
				GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBER_LIST);
				GS()->UpdateVotesIfForAll(MENU_GUILD_INVITATIONS);
			break;
		}
		return true;
	}

	// request deny
	if(PPSTR(pCmd, "GUILD_REQUESTS_DENY") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// deny the request
		pGuild->GetMembers()->GetRequests()->Deny(Extra1, pPlayer->Account()->GetGuildMember());
		GS()->UpdateVotesIfForAll(MENU_GUILD_MEMBER_LIST);
		GS()->UpdateVotesIfForAll(MENU_GUILD_INVITATIONS);
		return true;
	}

	// field search guilds
	if(PPSTR(pCmd, "GUILD_FINDER_SEARCH_FIELD") == 0)
	{
		// check text valid
		if(PPSTR(pReason, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		// update search buffer and reset votes
		str_copy(pPlayer->GetTempData().m_aGuildSearchBuf, pReason, sizeof(pPlayer->GetTempData().m_aGuildSearchBuf));
		pPlayer->m_VotesData.UpdateVotes(MENU_GUILD_FINDER);
		return true;
	}

	// send request
	if(PPSTR(pCmd, "GUILD_SEND_REQUEST") == 0)
	{
		// check guild if have
		if(pPlayer->Account()->HasGuild())
		{
			GS()->Chat(ClientID, "You're already in a guild.");
			return true;
		}

		// initialize variables
		const GuildIdentifier& ID = Extra1;
		const int& AccountID = Extra2;

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
			case GuildResult::MEMBER_SUCCESSFUL: GS()->Chat(ClientID, "You sent a request to join the '{}' guild.", pGuild->GetName()); break;
		}
		return true;
	}

	// declare guild war
	if(PPSTR(pCmd, "GUILD_DECLARE_WAR") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// check if war and self it's same guild
		if(pGuild->GetID() == Extra1)
		{
			GS()->Chat(ClientID, "You can't declare war on your own guild.");
			return true;
		}

		// check war guild valid
		CGuild* pWarGuild = GetGuildByID(Extra1);
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

	// remove plant
	if(PPSTR(pCmd, "GUILD_HOUSE_FARM_ZONE_REMOVE_PLANT") == 0)
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
		const int& FarmzoneID = Extra1;
		const ItemIdentifier& ItemID = Extra2;

		// check farmzone valid
		auto* pManager = pHouse->GetFarmzonesManager();
		auto pFarmzone = pManager->GetFarmzoneByID(FarmzoneID);
		if(!pFarmzone)
		{
			GS()->Chat(ClientID, "Farm zone not found.");
			return true;
		}

		// check is same planted item with current
		if(pFarmzone->RemoveItemFromNode(ItemID))
		{
			auto* pItemInfo = GS()->GetItemInfo(ItemID);
			GS()->Chat(ClientID, "You have successfully removed the '{}' from '{}'.", pItemInfo->GetName(), pFarmzone->GetName());
			pManager->Save();
		}

		pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD_HOUSE_FARMZONE_SELECT);
		return true;
	}

	// try plant to house
	if(PPSTR(pCmd, "GUILD_HOUSE_FARM_ZONE_TRY_PLANT") == 0)
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
		const int& Useds = maximum(1, ReasonNumber);
		const int& FarmzoneID = Extra1;
		const ItemIdentifier& ItemID = Extra2;

		// check farmzone valid
		auto* pManager = pHouse->GetFarmzonesManager();
		auto* pFarmzone = pManager->GetFarmzoneByID(FarmzoneID);
		if(!pFarmzone)
		{
			GS()->Chat(ClientID, "Farm zone not found.");
			return true;
		}

		// check is same plant item with current
		if(ItemID == pFarmzone->GetItemID())
		{
			GS()->Chat(ClientID, "This item has already been planted on this farm.");
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
				// update
				GS()->Chat(ClientID, "You have successfully plant to farm zone.");
				pFarmzone->AddItemToNode(ItemID);
				pManager->Save();
			}
			else
			{
				GS()->Chat(ClientID, "You failed plant to farm zone.");
			}
			pPlayer->m_VotesData.UpdateVotesIf(MENU_GUILD_HOUSE_FARMZONE_SELECT);
		}

		return true;
	}

	// house door
	if(PPSTR(pCmd, "GUILD_HOUSE_DOOR") == 0)
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
		int UniqueDoorID = Extra1;
		pHouse->GetDoorManager()->Reverse(UniqueDoorID);
		GS()->UpdateVotesIfForAll(MENU_GUILD_HOUSE_DOOR_LIST);
		return true;
	}

	// house sell
	if(PPSTR(pCmd, "GUILD_HOUSE_SELL") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// prevent accidental pressing
		if(ReasonNumber != 3342)
		{
			GS()->Chat(ClientID, "Random Touch Security Code has not been entered correctly.");
			return true;
		}

		// result
		pGuild->SellHouse();
		pPlayer->m_VotesData.UpdateVotes(MENU_GUILD);
		GS()->UpdateVotesIfForAll(MENU_GUILD);
	}

	// upgrade guild
	if(PPSTR(pCmd, "GUILD_UPGRADE") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		const auto UpgrID = (GuildUpgrade)Extra1;
		if(!pGuild->Upgrade(UpgrID))
		{
			GS()->Chat(ClientID, "Your guild does not have enough gold, or the maximum upgrade level has been reached.");
			return true;
		}

		GS()->CreatePlayerSound(ClientID, SOUND_VOTE_UPGRADE);
		GS()->UpdateVotesIfForAll(MENU_GUILD_UPGRADES);
		return true;
	}

	// logger set activity
	if(PPSTR(pCmd, "GUILD_LOGGER_SET") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		pGuild->GetLogger()->SetActivityFlag(Extra1);
		GS()->UpdateVotesIfForAll(MENU_GUILD_LOGS);
		return true;
	}

	return false;
}

bool CGuildManager::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd)
{
	const auto ClientID = pPlayer->GetCID();

	// buy house
	if(PPSTR(pCmd, "GUILD_HOUSE_BUY") == 0)
	{
		// check guild valid and access rights
		auto* pGuild = pPlayer->Account()->GetGuild();
		if(!pGuild || !pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// result
		const auto& [pHouse] = pMotdData->GetCurrent()->Unpack<CGuildHouse*>();
		if(pHouse)
		{
			switch(pGuild->BuyHouse(pHouse->GetID()))
			{
				default: GS()->Chat(ClientID, "Unforeseen error."); break;
				case GuildResult::BUY_HOUSE_ALREADY_HAVE: GS()->Chat(ClientID, "Your guild already has a house."); break;
				case GuildResult::BUY_HOUSE_ALREADY_PURCHASED: GS()->Chat(ClientID, "This guild house has already been purchased."); break;
				case GuildResult::BUY_HOUSE_NOT_ENOUGH_GOLD: GS()->Chat(ClientID, "Your guild doesn't have enough gold."); break;
				case GuildResult::BUY_HOUSE_UNAVAILABLE: GS()->Chat(ClientID, "This guild house is not available for purchase."); break;
				case GuildResult::SUCCESSFUL: break;
			}
		}

		return true;
	}

	return false;
}

bool CGuildManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// menu finder
	if(Menulist == MENU_GUILD_FINDER)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowFinder(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_FINDER_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_FINDER);

		if(const auto GuildID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowFinderDetail(pPlayer, GuildID.value());
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild
	if(Menulist == MENU_GUILD)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild upgrades
	if(Menulist == MENU_GUILD_UPGRADES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowUpgrades(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild disband
	if(Menulist == MENU_GUILD_DISBAND)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDisband(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild house sell
	if(Menulist == MENU_GUILD_SELL_HOUSE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowHouseSell(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild logs
	if(Menulist == MENU_GUILD_LOGS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowLogsMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild war
	if(Menulist == MENU_GUILD_WARS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDeclareWarMenu(ClientID);
		return true;
	}

	// menu guild invites
	if(Menulist == MENU_GUILD_INVITATIONS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowRequests(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild house door
	if(Menulist == MENU_GUILD_HOUSE_DOOR_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowDoorsControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild membership
	if(Menulist == MENU_GUILD_MEMBER_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowMembershipList(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_MEMBER_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_MEMBER_LIST);

		if(const auto AccountID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowMembershipEdit(pPlayer, AccountID.value());
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild ranks
	if(Menulist == MENU_GUILD_RANK_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowRanksList(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_RANK_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_RANK_LIST);

		if(const auto RankID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowRankEdit(pPlayer, RankID.value());
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu guild house farm zone
	if(Menulist == MENU_GUILD_HOUSE_FARMZONE_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD);
		ShowFarmzonesControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}
	if(Menulist == MENU_GUILD_HOUSE_FARMZONE_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUILD_HOUSE_FARMZONE_LIST);

		if(const auto FarmzoneID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowFarmzoneEdit(pPlayer, FarmzoneID.value());
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CGuildManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	if(Menulist == MOTD_MENU_GUILD_HOUSE_DETAIL)
	{
		auto* pChr = pPlayer->GetCharacter();
		auto* pHouse = GetHouseByPos(pChr->m_Core.m_Pos);
		ShowDetail(pPlayer, pHouse);
		return true;
	}

	return false;
}

void CGuildManager::OnTimePeriod(ETimePeriod Period)
{
	// handle time period for each guilds
	for(auto& pGuild : CGuild::Data())
		pGuild->HandleTimePeriod(Period);
}

void CGuildManager::InitWars() const
{
	/*ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_WARS_TABLE);
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
	}*/
}

void CGuildManager::Create(CPlayer* pPlayer, const char* pGuildName) const
{
	// check validity
	if(!pPlayer)
		return;

	// check if we already in guild
	const int ClientID = pPlayer->GetCID();
	if(pPlayer->Account()->HasGuild())
	{
		GS()->Chat(ClientID, "You already in guild group!");
		return;
	}

	// check guild name
	CSqlString<64> GuildName(pGuildName);
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", TW_GUILDS_TABLE, "WHERE Name = '{}'", GuildName.cstr());
	if(pRes->next())
	{
		GS()->Chat(ClientID, "This guild name already useds!");
		return;
	}

	// check guild ticket
	if(!pPlayer->Account()->SpendCurrency(1, itTicketGuild))
	{
		GS()->Chat(ClientID, "You need first buy guild ticket on shop!");
		return;
	}

	// get next guild ID
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", TW_GUILDS_TABLE, "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // TODO: thread save ? hm need for table all time auto increment = 1; NEED FIX IT -- use some kind of uuid

	// implement creation and add to table
	CGuild* pGuild = CGuild::CreateElement(InitID);
	const std::string MembersData = R"({"members":[{"id":)" + std::to_string(pPlayer->Account()->GetID()) + R"(,"rank_id":0,"deposit":"0"}]})";
	pGuild->Init(GuildName.cstr(), MembersData, -1, 1, 0, 0, pPlayer->Account()->GetID(), 0, -1, nullptr);
	pPlayer->Account()->ReinitializeGuild();
	Database->Execute<DB::INSERT>(TW_GUILDS_TABLE, "(ID, Name, LeaderUID, Members) VALUES ('{}', '{}', '{}', '{}')",
		InitID, GuildName.cstr(), pPlayer->Account()->GetID(), MembersData.c_str());
	GS()->Chat(-1, "New guilds '{}' have been created!", GuildName.cstr());
	pPlayer->m_VotesData.UpdateVotesIf(MENU_MAIN);
}

void CGuildManager::Disband(GuildIdentifier ID) const
{
	// check if guild exists
	auto pGuild = GetGuildByID(ID);
	if(!pGuild)
		return;

	// End guild wars for disbanded guild
	if(pGuild->GetWar() && pGuild->GetWar()->GetHandler())
		pGuild->GetWar()->GetHandler()->End();

	// if guild has house then sell it
	if(pGuild->HasHouse())
	{
		pGuild->SellHouse();
		GS()->Chat(-1, "The guild '{}' has lost house.", pGuild->GetName());
	}

	// send mail
	BigInt ReturnsGold = std::max((BigInt)1, pGuild->GetBankManager()->Get());
	MailWrapper Mail("System", pGuild->GetLeaderUID(), "Your guild was disbanded.");
	Mail.AddDescLine("We returned some gold from your guild.");
	mystd::process_bigint_in_chunks<int>(ReturnsGold, [&Mail](int chunk)
	{
		Mail.AttachItem(CItem(itGold, chunk));
	});
	Mail.Send();
	GS()->Chat(-1, "The '{}' guild has been disbanded.", pGuild->GetName());

	// remove all related guild data
	Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '{}'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '{}'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_RANKS_TABLE, "WHERE GuildID = '{}'", pGuild->GetID());
	Database->Execute<DB::REMOVE>(TW_GUILDS_TABLE, "WHERE ID = '{}'", pGuild->GetID());

	// erase guild from server
	delete pGuild;
	CGuild::Data().erase(std::find(CGuild::Data().begin(), CGuild::Data().end(), pGuild));
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
	const auto ExpNeed = computeExperience(pGuild->GetLevel());
	const int MemberUsedSlots = (int)pGuild->GetMembers()->GetContainer().size();
	const int MemberMaxSlots = pGuild->GetUpgrades().getRef<int>((int)GuildUpgrade::AvailableSlots);

	// Guild information
	VoteWrapper VInfo(ClientID, VWF_ALIGN_TITLE|VWF_SEPARATE|VWF_STYLE_STRICT_BOLD, "{}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VInfo.Add("Level: {} Exp: {}/{}", pGuild->GetLevel(), pGuild->GetExperience(), ExpNeed);
	VInfo.Add("Members: {} of {}", MemberUsedSlots, MemberMaxSlots);
	VInfo.Add("Bank: {$} gold", pGuild->GetBankManager()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// Guild management
	VoteWrapper VManagement(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "\u262B Guild Management");
	VManagement.Add("Your: {$} | Bank: {$} gold", pPlayer->Account()->GetTotalGold(), pGuild->GetBankManager()->Get());
	VManagement.AddOption("GUILD_DEPOSIT_GOLD", "Deposit. (Amount in a reason)");
	VManagement.AddLine();
	VManagement.AddMenu(MENU_GUILD_UPGRADES, "Improvements & Upgrades");
	VManagement.AddMenu(MENU_GUILD_MEMBER_LIST, "Membership list");
	VManagement.AddMenu(MENU_GUILD_INVITATIONS, "Membership requests");
	VManagement.AddMenu(MENU_GUILD_RANK_LIST, "Rank management");
	VManagement.AddMenu(MENU_GUILD_LOGS, "Logs of activity");
	VManagement.AddMenu(MENU_GUILD_WARS, "Guild wars");
	VManagement.AddMenu(MENU_GUILD_DISBAND, "Disband");

	// Guild append house menu
	if(HasHouse)
	{
		// House management
		auto* pHouse = pGuild->GetHouse();
		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper VHouse(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "\u2302 House Management (rented for {# (day|days)})", pHouse->GetRentDays());
		VHouse.Add("Bank: {$} | Rent per day: {$} gold", pGuild->GetBankManager()->Get(), pHouse->GetRentPrice());
		VHouse.AddOption("GUILD_HOUSE_EXTEND_RENT", "Extend. (Amount in a reason)");
		VHouse.AddLine();
		VHouse.AddOption("GUILD_HOUSE_TELEPORT", "Teleport to house");
		VHouse.AddOption("GUILD_HOUSE_DECORATION", "Decoration editor");
		VHouse.AddMenu(MENU_GUILD_HOUSE_DOOR_LIST, "Doors");
		VHouse.AddMenu(MENU_GUILD_HOUSE_FARMZONE_LIST, "Farms");
		VHouse.AddMenu(MENU_GUILD_SELL_HOUSE, "Sell");
	}
}

void CGuildManager::ShowUpgrades(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	int ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE, "\u2324 Guild upgrades (Information)");
	VInfo.Add("All improvements are solely related to the guild itself.");
	VInfo.Add("Bank: {}", pGuild->GetBankManager()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// guild-related upgrades
	VoteWrapper VUpgr(ClientID, VWF_ALIGN_TITLE|VWF_STYLE_SIMPLE, "\u2730 Guild-related upgrades");
	for(int i = (int)GuildUpgrade::AvailableSlots; i < (int)GuildUpgrade::NumGuildUpgr; i++)
	{
		const int Price = pGuild->GetUpgradePrice(static_cast<GuildUpgrade>(i));
		const auto* pUpgradeField = &pGuild->GetUpgrades().getField<int>(i);

		VUpgr.AddOption("GUILD_UPGRADE", i, "Upgrade {} ({}) {$} gold",
			pUpgradeField->getDescription(), pUpgradeField->m_Value, Price);
	}
	VoteWrapper::AddEmptyline(ClientID);

	// house-related upgrades
	if(pGuild->HasHouse())
	{
		VoteWrapper VUpgrHouse(ClientID, VWF_ALIGN_TITLE|VWF_STYLE_SIMPLE, "\u2725 House-related upgrades");
		for(int i = (int)GuildUpgrade::ChairExperience; i < (int)GuildUpgrade::NumGuildHouseUpgr; i++)
		{
			int Price = pGuild->GetUpgradePrice(static_cast<GuildUpgrade>(i));
			const auto* pUpgrade = &pGuild->GetUpgrades().getField<int>(i);

			VUpgrHouse.AddOption("GUILD_UPGRADE", i, "Upgrade {} ({}) {$} gold",
				pUpgrade->getDescription(), pUpgrade->m_Value, Price);
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
	auto CurrentSlots = pGuild->GetMembers()->GetCurrentSlots();

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2324 Membership information", pGuild->GetName());
	VInfo.Add("Guild name: {}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VoteWrapper::AddEmptyline(ClientID);

	// list
	VoteWrapper VList(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "List of membership ({} of {})", CurrentSlots.first, CurrentSlots.second);
	for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
	{
		auto pMember = pIterMember.second;
		const int& UID = pMember->GetAccountID();
		const char* pNickname = Server()->GetAccountNickname(UID);
		VList.AddMenu(MENU_GUILD_MEMBER_SELECT, UID, "{}. {} {} Deposit: {}",
			VList.NextPos(), pMember->GetRank()->GetName(), pNickname, pMember->GetDeposit());
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

	// top-middle
	int ClientID = pPlayer->GetCID();
	VoteWrapper VTop(ClientID, VWF_ALIGN_TITLE | VWF_SEPARATE | VWF_STYLE_SIMPLE, "Editing member '{}'", Server()->GetAccountNickname(pMember->GetAccountID()));
	VTop.Add("Deposit: {}", pMember->GetDeposit());
	VTop.Add("Rank: {}", pMember->GetRank()->GetName());
	VTop.AddLine();
	if(!SelfSlot)
	{
		if(RightsLeader)
			VTop.AddOption("GUILD_SET_LEADER", "Give Leader (in reason 134)");
		if(RightsInviteKick)
			VTop.AddOption("GUILD_KICK_MEMBER", AccountID, "Kick");
	}
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
	int CurrentRanksNum = (int)pPlayer->Account()->GetGuild()->GetRanks()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE, "\u2324 Rank management (Information)");
	VInfo.Add("Maximal {} ranks for one guild", MaxRanksNum);
	VInfo.Add("Guild leader ignores rank rights");
	VInfo.Add("Use the reason as a text field.");
	VoteWrapper::AddEmptyline(ClientID);

	// top-middle
	VoteWrapper VTop(ClientID, VWF_ALIGN_TITLE|VWF_SEPARATE|VWF_STYLE_SIMPLE, "Management");
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
		VList.MarkList().AddMenu(MENU_GUILD_RANK_SELECT, ID, "{} {}", pRank->GetName(), StrAppendRankInfo.c_str());
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

	// top-middle
	const int ClientID = pPlayer->GetCID();
	VoteWrapper VTop(ClientID, VWF_ALIGN_TITLE|VWF_SEPARATE|VWF_STYLE_SIMPLE, "Editing rank '{}'", pRank->GetName());
	VTop.Add("Rigths: {}", pRank->GetRightsName());
	VTop.AddLine();
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

void CGuildManager::ShowFarmzonesControl(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pHouse = pGuild->GetHouse();
	if(!pHouse)
		return;

	int ClientID = pPlayer->GetCID();
	int FarmzonesNum = (int)pHouse->GetFarmzonesManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Farm zones information");
	VInfo.Add("You can control your farm zones in the house");
	VInfo.Add("Your home has: {} farm zones.", FarmzonesNum);
	VoteWrapper::AddEmptyline(ClientID);

	// farm zones control
	VoteWrapper VFarmzones(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "\u2743 Farm zone's control");
	for(auto& [ID, Farmzone] : pHouse->GetFarmzonesManager()->GetContainer())
		VFarmzones.AddMenu(MENU_GUILD_HOUSE_FARMZONE_SELECT, ID, "Farm {} zone / {}", Farmzone.GetName(), GS()->GetItemInfo(Farmzone.GetItemID())->GetName());

	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowFarmzoneEdit(CPlayer* pPlayer, int FarmzoneID) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	auto* pHouse = pGuild->GetHouse();
	if(!pHouse)
		return;

	auto* pFarmzone = pHouse->GetFarmzonesManager()->GetFarmzoneByID(FarmzoneID);
	if(!pFarmzone)
		return;

	const auto ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VInfo(ClientID, VWF_ALIGN_TITLE|VWF_SEPARATE|VWF_STYLE_STRICT_BOLD, "\u2741 Farm {} zone", pFarmzone->GetName());
	VInfo.Add("You can grow a plant on the property");
	VInfo.Add("Chance: {}%", s_GuildChancePlanting);
	VoteWrapper::AddEmptyline(ClientID);

	// planted list
	for(auto& Elem : pFarmzone->GetNode().m_vItems)
	{
		auto ItemID = Elem.Element;
		auto* pItemInfo = GS()->GetItemInfo(ItemID);
		VoteWrapper VPlanted(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "{} - chance {~.2}%", pItemInfo->GetName(), Elem.Chance);
		VPlanted.AddOption("GUILD_HOUSE_FARM_ZONE_REMOVE_PLANT", FarmzoneID, ItemID, "Remove {} from plant", pItemInfo->GetName());
	}
	VoteWrapper::AddEmptyline(ClientID);

	// items list availables can be planted
	auto vItems = CInventoryManager::GetItemIDsCollectionByType(ItemType::ResourceHarvestable);
	VoteWrapper VPossiblePlanting(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "\u2741 Possible items for planting");
	for(auto& ID : vItems)
	{
		bool AllowPlant = true;
		for(auto& Elem : pFarmzone->GetNode().m_vItems)
		{
			if(Elem.Element == ID)
			{
				AllowPlant = false;
				break;
			}
		}

		auto* pPlayerItem = pPlayer->GetItem(ID);
		if(AllowPlant && pPlayerItem->HasItem())
			VPossiblePlanting.AddOption("GUILD_HOUSE_FARM_ZONE_TRY_PLANT", FarmzoneID, ID, "Try plant {} (has {})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
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
			VList.AddMenu(MENU_GUILD_FINDER_SELECT, pGuild->GetID(), "{} (leader {})", pGuild->GetName(), Server()->GetAccountNickname(OwnerUID));
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
	int ClientID = pPlayer->GetCID();
	auto CurrentSlots = pGuild->GetMembers()->GetCurrentSlots();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Information about {}", pGuild->GetName());
	VInfo.Add("Leader: {}", Server()->GetAccountNickname(pGuild->GetLeaderUID()));
	VInfo.Add("Members: {} of {}", CurrentSlots.first, CurrentSlots.second);
	VInfo.Add("Has house: {}", pGuild->HasHouse() ? "Yes" : "No");
	VInfo.Add("Bank: {$} gold", pGuild->GetBankManager()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// Memberlist
	VoteWrapper VList(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "List of membership");
	for(auto& pIterMember : pGuild->GetMembers()->GetContainer())
	{
		auto pMember = pIterMember.second;
		VList.Add("{}. {} {} Deposit: {}", VList.NextPos(), pMember->GetRank()->GetName(), Server()->GetAccountNickname(pMember->GetAccountID()), pMember->GetDeposit());
	}

	// buttom send
	if(!pPlayer->Account()->HasGuild())
		VList.AddOption("GUILD_SEND_REQUEST", pGuild->GetID(), pPlayer->Account()->GetID(), "Send request to join");
	VoteWrapper::AddEmptyline(ClientID);
}

void CGuildManager::ShowDetail(CPlayer* pPlayer, CGuildHouse* pHouse) const
{
	if(!pHouse || !pPlayer)
		return;

	// initialze variables
	const auto ClientID = pPlayer->GetCID();
	const auto HouseID = pHouse->GetID();

	// house detail purchease
	MotdMenu MHouseDetail(ClientID, MTFLAG_CLOSE_BUTTON, "Buying a house you will need to constantly the Treasury. In the intervals of time will be paid house.");
	MHouseDetail.AddText("House HID:[{}]", HouseID);
	MHouseDetail.AddText("Farm {(zone|zones): #}", (int)pHouse->GetFarmzonesManager()->GetContainer().size());
	MHouseDetail.AddText("{(Door|Doors): #}", (int)pHouse->GetDoorManager()->GetContainer().size());
	MHouseDetail.AddText("Rent: {$}", pHouse->GetRentPrice());
	MHouseDetail.AddSeparateLine();
	if(!pHouse->IsPurchased())
	{
		auto* pGuild = pPlayer->Account()->GetGuild();
		MHouseDetail.AddText("Price: {$}", pHouse->GetInitialFee());

		if(pGuild && pPlayer->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_LEADER))
		{
			MHouseDetail.AddText("Bank: {$}", pGuild->GetBankManager()->Get());
			MHouseDetail.AddOption("GUILD_HOUSE_BUY", "Purchase").Pack(pHouse);
		}
	}
	else
	{
		MHouseDetail.AddText("Owner: {}", pHouse->GetOwnerName());
	}
	MHouseDetail.Send(MOTD_MENU_GUILD_HOUSE_DETAIL);
}

void CGuildManager::ShowDeclareWarMenu(int ClientID) const
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
	VWar.Add("Cooldown: {# (minute|minutes)}", 10);
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
				VWarList.AddOption("GUILD_DECLARE_WAR", p->GetID(), "{} (online {# (player|players)})", p->GetName(), p->GetMembers()->GetOnlineCount());
		}
	}

	// Add the votes backpage for the player
	VoteWrapper::AddBackpage(ClientID);
}

// Function to show guild logs for a specific player
void CGuildManager::ShowLogsMenu(CPlayer* pPlayer) const
{
	auto* pGuild = pPlayer->Account()->GetGuild();
	if(!pGuild)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	auto* pLogger = pGuild->GetLogger();
	auto flagStatus = [&](int flag) { return pLogger->IsActivityFlagSet(flag) ? "[\u2714]" : "[\u2715]"; };

	// logger settings
	VoteWrapper VLogger(ClientID, VWF_ALIGN_TITLE|VWF_SEPARATE|VWF_STYLE_SIMPLE, "Activity log settings");
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

CGuild* CGuildManager::GetGuildByID(GuildIdentifier ID) const
{
	auto itGuild = std::ranges::find_if(CGuild::Data(), [&ID](CGuild* p)
	{
		return p->GetID() == ID;
	});


	return itGuild != CGuild::Data().end() ? (*itGuild) : nullptr;
}

CGuild* CGuildManager::GetGuildByName(const char* pGuildname) const
{
	auto itGuild = std::ranges::find_if(CGuild::Data(), [&pGuildname](CGuild* p)
	{
		return str_comp_nocase(p->GetName(), pGuildname) == 0;
	});

	return itGuild != CGuild::Data().end() ? (*itGuild) : nullptr;
}

CGuildHouse* CGuildManager::GetHouseByID(const GuildHouseIdentifier& ID) const
{
	auto pHouse = std::ranges::find_if(CGuildHouse::Data(), [&ID](const CGuildHouse* p)
	{
		return ID == p->GetID();
	});

	return pHouse != CGuildHouse::Data().end() ? *pHouse : nullptr;
}

CGuildHouse* CGuildManager::GetHouseByPos(vec2 Pos) const
{
	const auto switchNumber = GS()->Collision()->GetSwitchTileNumberAtIndex(Pos, TILE_SW_HOUSE_ZONE);
	if(!switchNumber)
		return nullptr;

	auto pHouse = std::ranges::find_if(CGuildHouse::Data(), [&](auto& p)
	{
		return *switchNumber == p->GetID();
	});

	return pHouse != CGuildHouse::Data().end() ? *pHouse : nullptr;
}

CFarmzone* CGuildManager::GetHouseFarmzoneByPos(vec2 Pos) const
{
	const auto switchNumber = GS()->Collision()->GetSwitchTileNumberAtIndex(Pos, TILE_SW_HOUSE_ZONE);
	if(!switchNumber)
		return nullptr;

	for(auto& pHouse : CGuildHouse::Data())
	{
		if(*switchNumber != pHouse->GetID())
			continue;

		for(auto& Farmzone : pHouse->GetFarmzonesManager()->GetContainer())
		{
			if(distance(Pos, Farmzone.second.GetPos()) < Farmzone.second.GetRadius())
				return &Farmzone.second;
		}
	}

	return nullptr;
}