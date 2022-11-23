/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/GameEntities/decoration_houses.h>
#include "Entities/GuildDoor.h"

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

#include <cstdarg>

void GuildCore::LoadGuildRank(int GuildID)
{
	// rank loading
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE ID > '0' AND GuildID = '%d'", GuildID);
	while(pRes->next())
	{
		int ID = pRes->getInt("ID");
		CGuildRankData::ms_aRankGuild[ID].m_GuildID = GuildID;
		CGuildRankData::ms_aRankGuild[ID].m_Access = pRes->getInt("Access");
		str_copy(CGuildRankData::ms_aRankGuild[ID].m_aRank, pRes->getString("Name").c_str(), sizeof(CGuildRankData::ms_aRankGuild[ID].m_aRank));
	}
}

void GuildCore::OnInit()
{
	const auto InitGuilds = Database->Prepare<DB::SELECT>("*", "tw_guilds");
	InitGuilds->AtExecute([this](ResultPtr pRes)
	{
		while (pRes->next())
		{
			int GuildID = pRes->getInt("ID");
			CGuildData::ms_aGuild[GuildID].m_UserID = pRes->getInt("UserID");
			CGuildData::ms_aGuild[GuildID].m_Level = pRes->getInt("Level");
			CGuildData::ms_aGuild[GuildID].m_Exp = pRes->getInt("Experience");
			CGuildData::ms_aGuild[GuildID].m_Bank = pRes->getInt("Bank");
			CGuildData::ms_aGuild[GuildID].m_Score = pRes->getInt("Score");
			str_copy(CGuildData::ms_aGuild[GuildID].m_aName, pRes->getString("Name").c_str(), sizeof(CGuildData::ms_aGuild[GuildID].m_aName));

			CGuildData::ms_aGuild[GuildID].m_UpgradeData.initFields(&pRes);
			LoadGuildRank(GuildID);
		}
		Job()->ShowLoadingProgress("Guilds", CGuildData::ms_aGuild.size());
	});
}

void GuildCore::OnInitWorld(const char* pWhereLocalWorld)
{
	// load houses
	const auto InitGuildHouses = Database->Prepare<DB::SELECT>("*", "tw_guilds_houses", pWhereLocalWorld);
	InitGuildHouses->AtExecute([this](ResultPtr pRes)
	{
		while (pRes->next())
		{
			int HouseID = pRes->getInt("ID");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_DoorX = pRes->getInt("DoorX");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_DoorY = pRes->getInt("DoorY");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_GuildID = pRes->getInt("GuildID");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_PosX = pRes->getInt("PosX");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_PosY = pRes->getInt("PosY");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_Price = pRes->getInt("Price");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_WorldID = pRes->getInt("WorldID");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_TextX = pRes->getInt("TextX");
			CGuildHouseData::ms_aHouseGuild[HouseID].m_TextY = pRes->getInt("TextY");
			if (CGuildHouseData::ms_aHouseGuild[HouseID].m_GuildID > 0 && !CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor)
			{
				CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor = 0;
				CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor = new GuildDoor(&GS()->m_World, vec2(CGuildHouseData::ms_aHouseGuild[HouseID].m_DoorX, CGuildHouseData::ms_aHouseGuild[HouseID].m_DoorY), CGuildHouseData::ms_aHouseGuild[HouseID].m_GuildID);
			}
		}
		Job()->ShowLoadingProgress("Houses", CGuildHouseData::ms_aHouseGuild.size());
	});

	const auto InitGuildHouseDecorations = Database->Prepare<DB::SELECT>("*", "tw_guilds_decorations", pWhereLocalWorld);
	InitGuildHouseDecorations->AtExecute([this](ResultPtr pRes)
	{
		while (pRes->next())
		{
			const int DecoID = pRes->getInt("ID");
			m_DecorationHouse[DecoID] = new CDecorationHouses(&GS()->m_World, vec2(pRes->getInt("PosX"),
				pRes->getInt("PosY")), pRes->getInt("HouseID"), pRes->getInt("DecoID"));
		}
		Job()->ShowLoadingProgress("Guilds Houses Decorations", m_DecorationHouse.size());
	});
}
void GuildCore::OnTick()
{
	TickHousingText();
}

bool GuildCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_GUILD_HOUSE))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		GS()->UpdateVotes(ClientID, MENU_MAIN);
		return true;
	}
	if(pChr->GetHelper()->TileExit(IndexCollision, TILE_GUILD_HOUSE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		GS()->UpdateVotes(ClientID, MENU_MAIN);
		return true;
	}

	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_GUILD_CHAIRS))
	{
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_GUILD_CHAIRS))
	{
		return true;
	}
	if(pChr->GetHelper()->BoolIndex(TILE_GUILD_CHAIRS))
	{
		if(Server()->Tick() % (Server()->TickSpeed() * 5) == 0)
		{
			const int HouseID = GetPosHouseID(pChr->m_Core.m_Pos);
			const int GuildID = GetHouseGuildID(HouseID);
			if(HouseID <= 0 || GuildID <= 0)
				return true;

			const int Exp = CGuildData::ms_aGuild[GuildID].m_UpgradeData(CGuildData::CHAIR_EXPERIENCE, 0).m_Value;
			pPlayer->AddExp(Exp);
		}
		return true;
	}

	return false;
}

/*
	TODO: We have to process checks in functions, they exist for something.
*/
bool GuildCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// -------------------------------------
	// ACCESS RANK: Leader functions
	if(PPSTR(CMD, "MLEADER") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER) || Get != 134)
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}
		const int SelectedUserID = VoteID;
		if(pPlayer->Acc().m_UserID == SelectedUserID)
		{
			GS()->Chat(ClientID, "You can't give the rights to yourself!");
			return true;
		}

		CGuildData::ms_aGuild[GuildID].m_UserID = SelectedUserID;
		Database->Execute<DB::UPDATE>("tw_guilds", "UserID = '%d' WHERE ID = '%d'", SelectedUserID, GuildID);
		AddHistoryGuild(GuildID, "New guild leader '%s'.", Job()->PlayerName(SelectedUserID));
		GS()->ChatGuild(GuildID, "Change leader {STR}->{STR}", Server()->ClientName(ClientID), Job()->PlayerName(SelectedUserID));
		GS()->StrongUpdateVotesForAll(MENU_GUILD_VIEW_PLAYERS);
		return true;
	}

	if(PPSTR(CMD, "BUYMEMBERHOUSE") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		BuyGuildHouse(GuildID, VoteID);
		GS()->StrongUpdateVotesForAll(MENU_MAIN);
		return true;
	}

	if(PPSTR(CMD, "MHOUSESELL") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER) || Get != 7177)
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		SellGuildHouse(GuildID);
		GS()->StrongUpdateVotesForAll(MENU_GUILD);
		return true;
	}

	if(PPSTR(CMD, "MDISBAND") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER) || Get != 55428)
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		DisbandGuild(GuildID);
		GS()->StrongUpdateVotesForAll(MENU_GUILD);
		return true;
	}

	if(PPSTR(CMD, "MRANKCREATE") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int LengthRank = str_length(pPlayer->GetTempData().m_aRankGuildBuf);
		if(LengthRank < 2 || LengthRank > 16)
		{
			GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16.");
			return true;
		}

		AddRank(GuildID, pPlayer->GetTempData().m_aRankGuildBuf);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	if(PPSTR(CMD, "MRANKSET") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int LengthRank = str_length(pPlayer->GetTempData().m_aRankGuildBuf);
		if(LengthRank < 2 || LengthRank > 16)
		{
			GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16.");
			return true;
		}

		const int RankID = VoteID;
		ChangeRank(RankID, GuildID, pPlayer->GetTempData().m_aRankGuildBuf);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	if(PPSTR(CMD, "MRANKDELETE") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int RankID = VoteID;
		DeleteRank(RankID, GuildID);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	if(PPSTR(CMD, "MRANKACCESS") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int RankID = VoteID;
		ChangeRankAccess(RankID);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	if(PPSTR(CMD, "MRANKCHANGE") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		// change rank and clear the menu
		ChangePlayerRank(VoteID, VoteID2);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_VIEW_PLAYERS);
		return true;
	}


	// -------------------------------------
	// ACCESS RANK: Invite and kick functions
	if(PPSTR(CMD, "MKICK") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		ExitGuild(VoteID);
		GS()->StrongUpdateVotesForAll(MENU_GUILD_VIEW_PLAYERS);
		return true;
	}

	if(PPSTR(CMD, "MINVITEACCEPT") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int SenderID = VoteID;
		if(JoinGuild(SenderID, GuildID))
		{
			Database->Execute<DB::REMOVE>("tw_guilds_invites", "WHERE GuildID = '%d' AND UserID = '%d'", GuildID, SenderID);
			GS()->SendInbox(Server()->ClientName(ClientID),SenderID, CGuildData::ms_aGuild[GuildID].m_aName, "You were accepted to join guild");
			GS()->StrongUpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
			GS()->StrongUpdateVotesForAll(MENU_GUILD_VIEW_PLAYERS);
			return true;
		}
		GS()->Chat(ClientID, "You can't accept (there are no free slot or he is already in Guild).");
		return true;
	}

	if(PPSTR(CMD, "MINVITEREJECT") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_INVITE_KICK))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int SenderID = VoteID;
		GS()->Chat(ClientID, "You reject invite.");
		Database->Execute<DB::REMOVE>("tw_guilds_invites", "WHERE GuildID = '%d' AND UserID = '%d'", GuildID, SenderID);
		GS()->SendInbox(Server()->ClientName(ClientID), SenderID, CGuildData::ms_aGuild[GuildID].m_aName, "You were denied join guild");
		GS()->UpdateVotes(ClientID, MENU_GUILD);
		return true;
	}


	// -------------------------------------
	// ACCESS RANK: Upgrade house functions
	if(PPSTR(CMD, "MDOOR") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_UPGRADE_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		if(ChangeStateDoor(GuildID))
			GS()->StrongUpdateVotesForAll(MENU_GUILD);
		return true;
	}

	if(PPSTR(CMD, "MUPGRADE") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_UPGRADE_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int UpgradeID = VoteID;
		if(UpgradeGuild(GuildID, UpgradeID))
		{
			const int GuildValue = CGuildData::ms_aGuild[GuildID].m_UpgradeData(UpgradeID, 0).m_Value;
			const char* pUpgradeName = CGuildData::ms_aGuild[GuildID].m_UpgradeData(UpgradeID, 0).getDescription();
			GS()->ChatGuild(GuildID, "Improved to {VAL} {STR} in {STR}!", GuildValue, pUpgradeName, CGuildData::ms_aGuild[GuildID].m_aName);
			AddHistoryGuild(GuildID, "'%s' level up to '%d'.", pUpgradeName, GuildValue);
			GS()->StrongUpdateVotes(ClientID, MENU_GUILD);
			return true;
		}
		GS()->Chat(ClientID, "You don't have that much money in the Bank.");
		return true;
	}

	if(PPSTR(CMD, "DECOGUILDSTART") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		const int HouseID = GetGuildHouseID(GuildID);
		if(GuildID <= 0 || HouseID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_UPGRADE_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const vec2 PositionHouse = GetPositionHouse(GuildID);
		if(distance(PositionHouse, pPlayer->GetCharacter()->m_Core.m_Pos) > 600)
		{
			GS()->Chat(ClientID, "Maximum distance between your home 600!");
			return true;
		}

		GS()->StartCustomVotes(ClientID, MENU_INVENTORY);
		GS()->AV(ClientID, "null", "Please close vote and press Left Mouse,");
		GS()->AV(ClientID, "null", "on position where add decoration!");
		GS()->AddVotesBackpage(ClientID);
		GS()->EndCustomVotes(ClientID);

		const int DecoItemID = VoteID;
		pPlayer->GetTempData().m_TempDecoractionID = DecoItemID;
		pPlayer->GetTempData().m_TempDecorationType = DECORATIONS_GUILD_HOUSE;
		return true;
	}

	if(PPSTR(CMD, "DECOGUILDDELETE") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		const int HouseID = GetGuildHouseID(GuildID);
		if(GuildID <= 0 || HouseID <= 0 || !CheckMemberAccess(pPlayer, ACCESS_UPGRADE_HOUSE))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int DecoID = VoteID;
		const int DecoItemID = VoteID2;
		if(DeleteDecorationHouse(DecoID))
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(DecoItemID);
			GS()->Chat(ClientID, "You back to the backpack {STR}!", pPlayerItem->Info()->GetName());
			pPlayerItem->Add(1);
		}
		GS()->StrongUpdateVotes(ClientID, MENU_GUILD_HOUSE_DECORATION);
		return true;
	}


	// -------------------------------------
	// ACCESS RANK: Full access functions
	if(PPSTR(CMD, "MSPAWN") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		const int HouseID = GetGuildHouseID(GuildID);
		if(GuildID <= 0 || HouseID <= 0)
		{
			GS()->Chat(ClientID, "You may not have a home, or you may not be in the guild.");
			return true;
		}

		vec2 Position = GetPositionHouse(GuildID);
		const int WorldID = CGuildHouseData::ms_aHouseGuild[HouseID].m_WorldID;
		if(!GS()->IsPlayerEqualWorld(ClientID, WorldID))
		{
			pPlayer->GetTempData().m_TempTeleportPos = Position;
			pPlayer->ChangeWorld(WorldID);
			return true;
		}
		pPlayer->GetCharacter()->ChangePosition(Position);
		return true;
	}
	if(PPSTR(CMD, "MMONEY") == 0)
	{
		const int GuildID = pPlayer->Acc().m_GuildID;
		if(GuildID <= 0)
		{
			GS()->Chat(ClientID, "You are not a member of the guild.");
			return true;
		}
		if(Get < 100)
		{
			GS()->Chat(ClientID, "Minimum number is 100 gold.");
			return true;
		}

		if(pPlayer->SpendCurrency(Get))
		{
			AddMoneyBank(GuildID, Get);
			Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildDeposit = GuildDeposit + '%d' WHERE ID = '%d'", Get, pPlayer->Acc().m_UserID);
			GS()->ChatGuild(GuildID, "{STR} deposit in treasury {VAL}gold.", Server()->ClientName(ClientID), Get);
			AddHistoryGuild(GuildID, "'%s' added to bank %dgold.", Server()->ClientName(ClientID), Get);
			GS()->StrongUpdateVotes(ClientID, MENU_GUILD);
		}
		return true;
	}
	if(PPSTR(CMD, "MRANKNAME") == 0)
	{
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Use please another name.");
			return true;
		}

		str_copy(pPlayer->GetTempData().m_aRankGuildBuf, GetText, sizeof(pPlayer->GetTempData().m_aRankGuildBuf));
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	// -------------------------------------
	// Functions outside the guild
	if(PPSTR(CMD, "MINVITENAME") == 0)
	{
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Use please another name.");
			return true;
		}

		str_copy(pPlayer->GetTempData().m_aGuildSearchBuf, GetText, sizeof(pPlayer->GetTempData().m_aGuildSearchBuf));
		GS()->StrongUpdateVotes(ClientID, MENU_GUILD_FINDER);
		return true;
	}

	if(PPSTR(CMD, "MINVITESEND") == 0)
	{
		SendInviteGuild(VoteID, pPlayer);
		return true;
	}
	
	return false;
}

bool GuildCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			return false;

		if(pChr->GetHelper()->BoolIndex(TILE_GUILD_HOUSE))
		{
			const int GuildHouseID = GetPosHouseID(pChr->m_Core.m_Pos);
			Job()->Member()->ShowBuyHouse(pPlayer, GuildHouseID);
			return true;
		}
		return false;
	}

	if(Menulist == MENU_GUILD_FINDER)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;
		ShowFinderGuilds(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD_FINDER_VIEW_PLAYERS)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD_FINDER;
		ShowGuildPlayers(pPlayer, pPlayer->m_TempMenuValue);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_GUILD)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;
		ShowMenuGuild(pPlayer);
		return true;
	}

	if(Menulist == MENU_GUILD_VIEW_PLAYERS)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowGuildPlayers(pPlayer, pPlayer->Acc().m_GuildID);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}


	if(Menulist == MENU_GUILD_HISTORY)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowHistoryGuild(ClientID, pPlayer->Acc().m_GuildID);
		return true;
	}

	if(Menulist == MENU_GUILD_RANK)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowMenuRank(pPlayer);
		return true;
	}

	if(Menulist == MENU_GUILD_INVITES)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		ShowInvitesGuilds(ClientID, pPlayer->Acc().m_GuildID);
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

		Job()->Item()->ListInventory(ClientID, ItemType::TYPE_DECORATION);
		GS()->AV(ClientID, "null");
		ShowDecorationList(pPlayer);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}
	return false;
}

/* #########################################################################
	BASED GUILDS
######################################################################### */
void GuildCore::TickHousingText()
{
	if (Server()->Tick() % (Server()->TickSpeed() * 2) != 0)
		return;

	for (const auto& [ID, House] : CGuildHouseData::ms_aHouseGuild)
	{
		if (House.m_WorldID == GS()->GetWorldID())
		{
			const int LifeTime = (Server()->TickSpeed() * 2) - 5;
			const int GuildID = House.m_GuildID;
			GS()->CreateText(nullptr, false, vec2(House.m_TextX, House.m_TextY), vec2(0, 0), LifeTime, (GuildID > 0 ? GuildName(GuildID) : "GUILD HOUSE"));
		}
	}
}

/* #########################################################################
	GET CHECK MEMBER
######################################################################### */
int GuildCore::SearchGuildByName(const char* pGuildName) const
{
	std::string SearchName(pGuildName); // I do not trust him with such important things xd
	auto pItem = std::find_if(CGuildData::ms_aGuild.begin(), CGuildData::ms_aGuild.end(), [SearchName](std::pair< int, CGuildData> pCheck)
	{
		std::string RealName(pCheck.second.m_aName);
		return SearchName.compare(RealName) == 0;
	});
	return pItem != CGuildData::ms_aGuild.end() ? pItem->first : -1;
}

const char *GuildCore::GuildName(int GuildID) const
{
	if(CGuildData::ms_aGuild.find(GuildID) != CGuildData::ms_aGuild.end())
		return CGuildData::ms_aGuild[GuildID].m_aName;
	return "invalid";
}

int GuildCore::GetMemberAccess(CPlayer* pPlayer) const
{
	if(pPlayer->Acc().IsGuild() && CGuildRankData::ms_aRankGuild.find(pPlayer->Acc().m_GuildRank) != CGuildRankData::ms_aRankGuild.end())
		return CGuildRankData::ms_aRankGuild[pPlayer->Acc().m_GuildRank].m_Access;
	return 0;
}

bool GuildCore::CheckMemberAccess(CPlayer *pPlayer, int Access) const
{
	const int GuildID = pPlayer->Acc().m_GuildID;
	if(GuildID > 0 && CGuildData::ms_aGuild.find(GuildID) != CGuildData::ms_aGuild.end() &&
		(CGuildData::ms_aGuild[GuildID].m_UserID == pPlayer->Acc().m_UserID ||
			(CGuildRankData::ms_aRankGuild.find(pPlayer->Acc().m_GuildRank) != CGuildRankData::ms_aRankGuild.end() &&
				(CGuildRankData::ms_aRankGuild[pPlayer->Acc().m_GuildRank].m_Access == Access || CGuildRankData::ms_aRankGuild[pPlayer->Acc().m_GuildRank].m_Access ==
					ACCESS_FULL))))
		return true;
	return false;
}

int GuildCore::GetMemberChairBonus(int GuildID, int Field) const
{
	if(GuildID > 0 && CGuildData::ms_aGuild.find(GuildID) != CGuildData::ms_aGuild.end())
		return CGuildData::ms_aGuild[GuildID].m_UpgradeData(Field, 0).m_Value;
	return -1;
}

/* #########################################################################
	FUNCTIONS HOUSES DECORATION
######################################################################### */
bool GuildCore::AddDecorationHouse(int DecoID, int GuildID, vec2 Position)
{
	if (CGuildData::ms_aGuild.find(GuildID) == CGuildData::ms_aGuild.end())
		return false;

	vec2 PositionHouse = GetPositionHouse(GuildID);
	if (distance(PositionHouse, Position) > 600)
		return false;

	int HouseID = GetGuildHouseID(GuildID);
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds_decorations", "WHERE HouseID = '%d'", HouseID);
	if ((int)pRes->rowsCount() >= g_Config.m_SvLimitDecoration)
		return false;

	ResultPtr pRes2 = Database->Execute<DB::SELECT>("ID", "tw_guilds_decorations", "ORDER BY ID DESC LIMIT 1");
	int InitID = (pRes2->next() ? pRes2->getInt("ID") + 1 : 1);
	Database->Execute<DB::INSERT>("tw_guilds_decorations", "(ID, DecoID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')",
		InitID, DecoID, HouseID, (int)Position.x, (int)Position.y, GS()->GetWorldID());
	m_DecorationHouse[InitID] = new CDecorationHouses(&GS()->m_World, Position, HouseID, DecoID);
	return true;
}

bool GuildCore::DeleteDecorationHouse(int ID)
{
	if (m_DecorationHouse.find(ID) == m_DecorationHouse.end())
		return false;

	if (m_DecorationHouse.at(ID))
	{
		delete m_DecorationHouse.at(ID);
		m_DecorationHouse.at(ID) = 0;
	}
	m_DecorationHouse.erase(ID);
	Database->Execute<DB::REMOVE>("tw_guilds_decorations", "WHERE ID = '%d'", ID);
	return true;
}

void GuildCore::ShowDecorationList(CPlayer* pPlayer)
{
	int ClientID = pPlayer->GetCID();
	int GuildID = pPlayer->Acc().m_GuildID;
	int HouseID = GetGuildHouseID(GuildID);
	for (auto & deco : m_DecorationHouse)
	{
		if (deco.second && deco.second->m_HouseID == HouseID)
		{
			GS()->AVD(ClientID, "DECOGUILDDELETE", deco.first, deco.second->m_DecoID, 1, "{STR}:{INT} back to the inventory",
				GS()->GetItemInfo(deco.second->m_DecoID)->GetName(), deco.first);
		}
	}
}

/* #########################################################################
	FUNCTIONS MEMBER MEMBER
######################################################################### */
void GuildCore::CreateGuild(CPlayer *pPlayer, const char *pGuildName)
{
	// check whether we are already in the guild
	const int ClientID = pPlayer->GetCID();
	if(pPlayer->Acc().m_GuildID > 0)
	{
		GS()->Chat(ClientID, "You already in guild group!");
		return;
	}

	// we check the availability of the guild's name
	CSqlString<64> GuildName(pGuildName);
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds", "WHERE Name = '%s'", GuildName.cstr());
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
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID")+1 : 1; // TODO: thread save ? hm need for table all time auto increment = 1; NEED FIX IT -- use some kind of uuid

	// initialize the guild
	str_copy(CGuildData::ms_aGuild[InitID].m_aName, GuildName.cstr(), sizeof(CGuildData::ms_aGuild[InitID].m_aName));
	CGuildData::ms_aGuild[InitID].m_UserID = pPlayer->Acc().m_UserID;
	CGuildData::ms_aGuild[InitID].m_Level = 1;
	CGuildData::ms_aGuild[InitID].m_Exp = 0;
	CGuildData::ms_aGuild[InitID].m_Bank = 0;
	CGuildData::ms_aGuild[InitID].m_Score = 0;
	CGuildData::ms_aGuild[InitID].m_UpgradeData(CGuildData::AVAILABLE_SLOTS, 0).m_Value = 2;
	CGuildData::ms_aGuild[InitID].m_UpgradeData(CGuildData::CHAIR_EXPERIENCE, 0).m_Value = 1;
	pPlayer->Acc().m_GuildID = InitID;

	// we create a guild in the table
	Database->Execute<DB::INSERT>("tw_guilds", "(ID, Name, UserID) VALUES ('%d', '%s', '%d')", InitID, GuildName.cstr(), pPlayer->Acc().m_UserID);
	Database->Execute<DB::UPDATE, 1000>("tw_accounts_data", "GuildID = '%d' WHERE ID = '%d'", InitID, pPlayer->Acc().m_UserID);
	GS()->Chat(-1, "New guilds [{STR}] have been created!", GuildName.cstr());
	GS()->StrongUpdateVotes(ClientID, MENU_MAIN);
}

void GuildCore::DisbandGuild(int GuildID)
{
	ResultPtr pResCheck = Database->Execute<DB::SELECT>("ID", "tw_guilds", "WHERE ID = '%d'", GuildID);
	if(!pResCheck)
	{
		dbg_msg("Guild", "The guild is disassembled with identifier %d, but it was not found in the Database->", GuildID);
		return;
	}

	const int HouseID = GetGuildHouseID(GuildID);
	const int LeaderUID = CGuildData::ms_aGuild[GuildID].m_UserID;
	const int ReturnsGold = max(1, CGuildData::ms_aGuild[GuildID].m_Bank);

	if(HouseID > 0)
		SellGuildHouse(GuildID);

	GS()->SendInbox("System", LeaderUID, "Your guild was disbanded.", "We returned some gold from your guild.", itGold, ReturnsGold);
	Database->Execute<DB::REMOVE>("tw_guilds", "WHERE ID = '%d'", GuildID);
	GS()->Chat(-1, "The {STR} Guild has been disbanded.", CGuildData::ms_aGuild[GuildID].m_aName);

	// clear guild data
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GS()->GetPlayer(i, true);
		if(!pPlayer || pPlayer->Acc().m_GuildID != GuildID)
			continue;

		pPlayer->Acc().m_GuildID = 0;
		pPlayer->Acc().m_GuildRank = 0;
		GS()->UpdateVotes(i, MENU_MAIN);
	}
	Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildID = NULL, GuildRank = NULL, GuildDeposit = '0' WHERE GuildID = '%d'", GuildID);
	CGuildData::ms_aGuild.erase(GuildID);
}

bool GuildCore::JoinGuild(int AccountID, int GuildID)
{
	const char *pPlayerName = Job()->PlayerName(AccountID);
	ResultPtr pResCheckJoin = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE ID = '%d' AND GuildID IS NOT NULL", AccountID);
	if(pResCheckJoin->next())
	{
		GS()->ChatAccount(AccountID, "You already in guild group!");
		GS()->ChatGuild(GuildID, "{STR} already joined your or another guilds", pPlayerName);
		return false;
	}

	// check the number of slots available
	ResultPtr pResCheckSlot = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE GuildID = '%d'", GuildID);
	if((int)pResCheckSlot->rowsCount() >= CGuildData::ms_aGuild[GuildID].m_UpgradeData(CGuildData::AVAILABLE_SLOTS, 0).m_Value)
	{
		GS()->ChatAccount(AccountID, "You don't joined [No slots for join]");
		GS()->ChatGuild(GuildID, "{STR} don't joined [No slots for join]", pPlayerName);
		return false;
	}

	// we update and get the data
	CPlayer *pPlayer = GS()->GetPlayerFromUserID(AccountID);
	if(pPlayer)
	{
		pPlayer->Acc().m_GuildID = GuildID;
		pPlayer->Acc().m_GuildRank = 0;
		GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
	}
	Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildID = '%d', GuildRank = NULL WHERE ID = '%d'", GuildID, AccountID);
	GS()->ChatGuild(GuildID, "Player {STR} join in your guild!", pPlayerName);
	return true;
}

void GuildCore::ExitGuild(int AccountID)
{
	// we check if the clan leader leaves
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds", "WHERE UserID = '%d'", AccountID);
	if (pRes->next())
	{
		GS()->ChatAccount(AccountID, "A leader cannot leave his guild group!");
		return;
	}

	// we check the account and its guild
	ResultPtr pResExit = Database->Execute<DB::SELECT>("GuildID", "tw_accounts_data", "WHERE ID = '%d'", AccountID);
	if (pResExit->next())
	{
		// we write to the guild that the player has left the guild
		const int GuildID = pResExit->getInt("GuildID");
		GS()->ChatGuild(GuildID, "{STR} left the Guild!", Job()->PlayerName(AccountID));
		AddHistoryGuild(GuildID, "'%s' exit or kicked.", Job()->PlayerName(AccountID));

		// we update the player's information
		CPlayer *pPlayer = GS()->GetPlayerFromUserID(AccountID);
		if(pPlayer)
		{
			pPlayer->Acc().m_GuildID = 0;
			GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
		}
		Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildID = NULL, GuildRank = NULL, GuildDeposit = '0' WHERE ID = '%d'", AccountID);
	}
}

void GuildCore::ShowMenuGuild(CPlayer *pPlayer) const
{
	if(!pPlayer->Acc().IsGuild())
		return;

	// if you haven't found a guild like this, then 'Search for guilds'
	const int ClientID = pPlayer->GetCID();
	const int GuildID = pPlayer->Acc().m_GuildID;
	const int GuildHouse = GetGuildHouseID(GuildID);
	const int ExpNeed = computeExperience(CGuildData::ms_aGuild[GuildID].m_Level);
	GS()->AVH(ClientID, TAB_GUILD_STAT, "Name: {STR} : Leader {STR}", CGuildData::ms_aGuild[GuildID].m_aName, Job()->PlayerName(CGuildData::ms_aGuild[GuildID].m_UserID));
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Level: {INT} Experience: {INT}/{INT}", CGuildData::ms_aGuild[GuildID].m_Level, CGuildData::ms_aGuild[GuildID].m_Exp, ExpNeed);
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Maximal available player count: {INT}", CGuildData::ms_aGuild[GuildID].m_UpgradeData(CGuildData::AVAILABLE_SLOTS, 0).m_Value);
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Guild Bank: {VAL}gold", CGuildData::ms_aGuild[GuildID].m_Bank);
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "◍ Your gold: {VAL}gold", pPlayer->GetItem(itGold)->GetValue());
	GS()->AVL(ClientID, "MMONEY", "Add gold guild bank. (Amount in a reason)", CGuildData::ms_aGuild[GuildID].m_aName);
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "▤ Guild system");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_VIEW_PLAYERS, NOPE, "List of players");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_INVITES, NOPE, "Requests membership");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_HISTORY, NOPE, "History of activity");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_RANK, NOPE, "Rank settings");
	if (GuildHouse > 0)
	{
		GS()->AV(ClientID, "null");
		GS()->AVL(ClientID, "null", "⌂ Housing system");
		GS()->AVM(ClientID, "MENU", MENU_GUILD_HOUSE_DECORATION, NOPE, "Settings Decoration(s)");
		GS()->AVL(ClientID, "MDOOR", "Change state (\"{STR}\")", GetGuildDoor(GuildID) ? "OPEN" : "CLOSED");
		GS()->AVL(ClientID, "MSPAWN", "Teleport to guild house");
		GS()->AVL(ClientID, "MHOUSESELL", "Sell your guild house (in reason 7177)");
	}
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "✖ Disband guild");
	GS()->AVL(ClientID, "null", "Gold spent on upgrades will not be refunded");
	GS()->AVL(ClientID, "null", "All gold will be returned to the leader only");
	GS()->AVL(ClientID, "MDISBAND", "Disband guild (in reason 55428)");
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "☆ Guild upgrades");
	if (GuildHouse > 0)
	{
		for(int i = CGuildData::CHAIR_EXPERIENCE; i < CGuildData::NUM_GUILD_UPGRADES; i++)
		{
			const char* pUpgradeName = CGuildData::ms_aGuild[GuildID].m_UpgradeData(i, 0).getDescription();
			const int PriceUpgrade = CGuildData::ms_aGuild[GuildID].m_UpgradeData(i, 0).m_Value * g_Config.m_SvPriceUpgradeGuildAnother;
			GS()->AVM(ClientID, "MUPGRADE", i, NOPE, "Upgrade {STR} ({INT}) {VAL}gold", pUpgradeName, CGuildData::ms_aGuild[GuildID].m_UpgradeData(i, 0).m_Value, PriceUpgrade);
		}
	}

	const char* pUpgradeName = CGuildData::ms_aGuild[GuildID].m_UpgradeData(CGuildData::AVAILABLE_SLOTS, 0).getDescription();
	const int UpgradeValue = CGuildData::ms_aGuild[GuildID].m_UpgradeData(CGuildData::AVAILABLE_SLOTS, 0).m_Value;
	const int PriceUpgrade = UpgradeValue * g_Config.m_SvPriceUpgradeGuildSlot;
	GS()->AVM(ClientID, "MUPGRADE", CGuildData::AVAILABLE_SLOTS, NOPE, "Upgrade {STR} ({INT}) {VAL}gold", pUpgradeName, UpgradeValue, PriceUpgrade);
	GS()->AddVotesBackpage(ClientID);
	return;
}

void GuildCore::ShowGuildPlayers(CPlayer* pPlayer, int GuildID)
{
	const int ClientID = pPlayer->GetCID();
	const bool SelfGuild = pPlayer->Acc().m_GuildID == GuildID;
	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1000;

	GS()->AVL(ClientID, "null", "List players of {STR}", CGuildData::ms_aGuild[GuildID].m_aName);

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Nick, GuildRank, GuildDeposit", "tw_accounts_data", "WHERE GuildID = '%d'", GuildID);
	while (pRes->next())
	{
		bool AllowedInteractiveWithPlayers = false;
		const int PlayerAccountID = pRes->getInt("ID");
		const int PlayerRankID = pRes->getInt("GuildRank");
		const int PlayerDeposit = pRes->getInt("GuildDeposit");
		CSqlString<32> PlayerNickname(pRes->getString("Nick").c_str());

		// without access
		if(!SelfGuild)
		{
			GS()->AVL(ClientID, "null",  "{STR} {STR} Deposit: {VAL}", GetGuildRank(GuildID, PlayerRankID), PlayerNickname.cstr(), PlayerDeposit);
			continue;
		}

		// with access for interactives with players
		GS()->AVH(ClientID, HideID, "{STR} {STR} Deposit: {VAL}", GetGuildRank(GuildID, PlayerRankID), PlayerNickname.cstr(), PlayerDeposit);
		if(CheckMemberAccess(pPlayer, ACCESS_LEADER))
		{
			for(auto& pRank : CGuildRankData::ms_aRankGuild)
			{
				if(GuildID == pRank.second.m_GuildID && PlayerRankID != pRank.first)
					GS()->AVD(ClientID, "MRANKCHANGE", PlayerAccountID, pRank.first, HideID, "Change Rank to: {STR}{STR}", pRank.second.m_aRank, pRank.second.m_Access > 0 ? "*" : "");
			}
			GS()->AVM(ClientID, "MLEADER", PlayerAccountID, HideID, "Give Leader (in reason 134)");
			AllowedInteractiveWithPlayers = true;
		}
		if(CheckMemberAccess(pPlayer, ACCESS_INVITE_KICK))
		{
			GS()->AVM(ClientID, "MKICK", PlayerAccountID, HideID, "Kick");
			AllowedInteractiveWithPlayers = true;
		}

		if(!AllowedInteractiveWithPlayers)
			GS()->AVM(ClientID, "null", PlayerAccountID, HideID, "You don't have rights to interact");
		HideID++;
	}
}

void GuildCore::AddExperience(int GuildID)
{
	CGuildData::ms_aGuild[GuildID].m_Exp += 1;

	bool UpdateTable = false;
	int ExperienceNeed = computeExperience(CGuildData::ms_aGuild[GuildID].m_Level);
	for( ; CGuildData::ms_aGuild[GuildID].m_Exp >= ExperienceNeed; )
	{
		CGuildData::ms_aGuild[GuildID].m_Exp -= ExperienceNeed;
		CGuildData::ms_aGuild[GuildID].m_Level++;

		ExperienceNeed = computeExperience(CGuildData::ms_aGuild[GuildID].m_Level);
		if(CGuildData::ms_aGuild[GuildID].m_Exp < ExperienceNeed)
			UpdateTable = true;

		GS()->Chat(-1, "Guild {STR} raised the level up to {INT}", CGuildData::ms_aGuild[GuildID].m_aName, CGuildData::ms_aGuild[GuildID].m_Level);
		GS()->ChatDiscord(DC_SERVER_INFO, "Information", "Guild {STR} raised the level up to {INT}", CGuildData::ms_aGuild[GuildID].m_aName, CGuildData::ms_aGuild[GuildID].m_Level);
		AddHistoryGuild(GuildID, "Guild raised level to '%d'.", CGuildData::ms_aGuild[GuildID].m_Level);
	}

	if(random_int()%10 == 2 || UpdateTable)
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '%d', Experience = '%d' WHERE ID = '%d'", CGuildData::ms_aGuild[GuildID].m_Level, CGuildData::ms_aGuild[GuildID].m_Exp, GuildID);
}

bool GuildCore::AddMoneyBank(int GuildID, int Money)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Bank", "tw_guilds", "WHERE ID = '%d'", GuildID);
	if(!pRes->next())
		return false;

	// add money
	CGuildData::ms_aGuild[GuildID].m_Bank = pRes->getInt("Bank") + Money;
	Database->Execute<DB::UPDATE>("tw_guilds", "Bank = '%d' WHERE ID = '%d'", CGuildData::ms_aGuild[GuildID].m_Bank, GuildID);
	return true;
}

bool GuildCore::RemoveMoneyBank(int GuildID, int Money)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Bank", "tw_guilds", "WHERE ID = '%d'", GuildID);
	if(!pRes->next())
		return false;

	// check if the bank has enough to pay
	CGuildData::ms_aGuild[GuildID].m_Bank = pRes->getInt("Bank");
	if(Money > CGuildData::ms_aGuild[GuildID].m_Bank)
		return false;

	// payment
	CGuildData::ms_aGuild[GuildID].m_Bank -= Money;
	Database->Execute<DB::UPDATE>("tw_guilds", "Bank = '%d' WHERE ID = '%d'", CGuildData::ms_aGuild[GuildID].m_Bank, GuildID);
	return true;
}

// purchase of upgrade maximum number of slots
bool GuildCore::UpgradeGuild(int GuildID, int Field)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "WHERE ID = '%d'", GuildID);
	if(pRes->next())
	{
		const char* pFieldName = CGuildData::ms_aGuild[GuildID].m_UpgradeData(Field, 0).getFieldName();
		CGuildData::ms_aGuild[GuildID].m_Bank = pRes->getInt("Bank");
		CGuildData::ms_aGuild[GuildID].m_UpgradeData(Field, 0).m_Value = pRes->getInt(pFieldName);

		const int UpgradePrice = (Field == CGuildData::AVAILABLE_SLOTS ? g_Config.m_SvPriceUpgradeGuildSlot : g_Config.m_SvPriceUpgradeGuildAnother);
		const int PriceAvailable = CGuildData::ms_aGuild[GuildID].m_UpgradeData(Field, 0).m_Value * UpgradePrice;
		if(PriceAvailable > CGuildData::ms_aGuild[GuildID].m_Bank)
			return false;

		CGuildData::ms_aGuild[GuildID].m_UpgradeData(Field, 0).m_Value++;
		CGuildData::ms_aGuild[GuildID].m_Bank -= PriceAvailable;
		Database->Execute<DB::UPDATE>("tw_guilds", "Bank = '%d', %s = '%d' WHERE ID = '%d'", CGuildData::ms_aGuild[GuildID].m_Bank, pFieldName, CGuildData::ms_aGuild[GuildID].m_UpgradeData(Field, 0).m_Value, GuildID);
		return true;
	}
	return false;
}

/* #########################################################################
	GET CHECK MEMBER RANK MEMBER
######################################################################### */
// access name
const char *GuildCore::AccessNames(int Access)
{
	switch(Access)
	{
		default: return "No Access";
		case ACCESS_INVITE_KICK: return "Access Invite Kick";
		case ACCESS_UPGRADE_HOUSE: return "Access Upgrades & House";
		case ACCESS_FULL: return "Full Access";
	}
}

// get a rank name
const char *GuildCore::GetGuildRank(int GuildID, int RankID)
{
	if(CGuildRankData::ms_aRankGuild.find(RankID) != CGuildRankData::ms_aRankGuild.end() && GuildID == CGuildRankData::ms_aRankGuild[RankID].m_GuildID)
		return CGuildRankData::ms_aRankGuild[RankID].m_aRank;
	return "Member";
}

// find rank by name and organization
int GuildCore::FindGuildRank(int GuildID, const char *Rank) const
{
	for(auto& pRank : CGuildRankData::ms_aRankGuild)
	{
		if(GuildID == pRank.second.m_GuildID && str_comp(Rank, pRank.second.m_aRank) == 0)
			return pRank.first;
	}
	return -1;
}

/* #########################################################################
	FUNCTIONS MEMBER RANK MEMBER
######################################################################### */
// add rank
void GuildCore::AddRank(int GuildID, const char *Rank)
{
	const int FindRank = FindGuildRank(GuildID, Rank);
	if(CGuildRankData::ms_aRankGuild.find(FindRank) != CGuildRankData::ms_aRankGuild.end())
		return GS()->ChatGuild(GuildID, "Found this rank in your table, change name");

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "WHERE GuildID = '%d'", GuildID);
	if(pRes->rowsCount() >= 5) return;

	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID")+1 : 1; // thread save ? hm need for table all time auto increment = 1; NEED FIX IT

	CSqlString<64> cGuildRank = CSqlString<64>(Rank);
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, GuildID, Name) VALUES ('%d', '%d', '%s')", InitID, GuildID, cGuildRank.cstr());
	GS()->ChatGuild(GuildID, "Creates new rank [{STR}]!", Rank);
	AddHistoryGuild(GuildID, "Added new rank '%s'.", Rank);

	CGuildRankData::ms_aRankGuild[InitID].m_GuildID = GuildID;
	str_copy(CGuildRankData::ms_aRankGuild[InitID].m_aRank, Rank, sizeof(CGuildRankData::ms_aRankGuild[InitID].m_aRank));
}

// unrank
void GuildCore::DeleteRank(int RankID, int GuildID)
{
	if(CGuildRankData::ms_aRankGuild.find(RankID) != CGuildRankData::ms_aRankGuild.end())
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildRank = NULL WHERE GuildRank = '%d' AND GuildID = '%d'", RankID, GuildID);
		Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '%d' AND GuildID = '%d'", RankID, GuildID);
		GS()->ChatGuild(GuildID, "Rank [{STR}] succesful delete", CGuildRankData::ms_aRankGuild[RankID].m_aRank);
		AddHistoryGuild(GuildID, "Deleted rank '%s'.", CGuildRankData::ms_aRankGuild[RankID].m_aRank);
		CGuildRankData::ms_aRankGuild.erase(RankID);
	}
}

// change rank
void GuildCore::ChangeRank(int RankID, int GuildID, const char *NewRank)
{
	const int FindRank = FindGuildRank(GuildID, NewRank);
	if(CGuildRankData::ms_aRankGuild.find(FindRank) != CGuildRankData::ms_aRankGuild.end())
		return GS()->ChatGuild(GuildID, "Found this rank name in your table, change name");

	if(CGuildRankData::ms_aRankGuild.find(RankID) != CGuildRankData::ms_aRankGuild.end())
	{
		CSqlString<64> cGuildRank = CSqlString<64>(NewRank);
		Database->Execute<DB::UPDATE>("tw_guilds_ranks", "Name = '%s' WHERE ID = '%d' AND GuildID = '%d'",
			cGuildRank.cstr(), RankID, GuildID);
		GS()->ChatGuild(GuildID, "Rank [{STR}] changes to [{STR}]", CGuildRankData::ms_aRankGuild[RankID].m_aRank, NewRank);
		AddHistoryGuild(GuildID, "Rank '%s' changes to '%s'.", CGuildRankData::ms_aRankGuild[RankID].m_aRank, NewRank);
		str_copy(CGuildRankData::ms_aRankGuild[RankID].m_aRank, NewRank, sizeof(CGuildRankData::ms_aRankGuild[RankID].m_aRank));
	}
}

// change access rank
void GuildCore::ChangeRankAccess(int RankID)
{
	if(CGuildRankData::ms_aRankGuild.find(RankID) != CGuildRankData::ms_aRankGuild.end())
	{
		CGuildRankData::ms_aRankGuild[RankID].m_Access++;
		if(CGuildRankData::ms_aRankGuild[RankID].m_Access > ACCESS_FULL)
			CGuildRankData::ms_aRankGuild[RankID].m_Access = ACCESS_NO;

		const int GuildID = CGuildRankData::ms_aRankGuild[RankID].m_GuildID;
		AddHistoryGuild(GuildID, "Rank '%s' access updated to '%s'.", CGuildRankData::ms_aRankGuild[RankID].m_aRank, AccessNames(CGuildRankData::ms_aRankGuild[RankID].m_Access));
		Database->Execute<DB::UPDATE>("tw_guilds_ranks", "Access = '%d' WHERE ID = '%d' AND GuildID = '%d'", CGuildRankData::ms_aRankGuild[RankID].m_Access, RankID, GuildID);
		GS()->ChatGuild(GuildID, "Rank [{STR}] changes [{STR}]!", CGuildRankData::ms_aRankGuild[RankID].m_aRank, AccessNames(CGuildRankData::ms_aRankGuild[RankID].m_Access));
	}
}

// change player rank
void GuildCore::ChangePlayerRank(int AccountID, int RankID)
{
	CPlayer* pPlayer = GS()->GetPlayerFromUserID(AccountID);
	if(pPlayer)
		pPlayer->Acc().m_GuildRank = RankID;

	Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildRank = '%d' WHERE ID = '%d'", RankID, AccountID);
}

// rank menu display
void GuildCore::ShowMenuRank(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1300;
	pPlayer->m_LastVoteMenu = MENU_GUILD;

	GS()->AV(ClientID, "null", "Use reason how enter Value, Click fields!");
	GS()->AV(ClientID, "null", "Example: Name rank: [], in reason name, and use this");
	GS()->AV(ClientID, "null", "For leader access full, ignored ranks");
	GS()->AV(ClientID, "null", "- - - - - - - - - -");
	GS()->AV(ClientID, "null", "- Maximal 5 ranks for one guild");
	GS()->AVM(ClientID, "MRANKNAME", 1, NOPE, "Name rank: {STR}", pPlayer->GetTempData().m_aRankGuildBuf);
	GS()->AVM(ClientID, "MRANKCREATE", 1, NOPE, "Create new rank");
	GS()->AV(ClientID, "null");

	const int GuildID = pPlayer->Acc().m_GuildID;
	for(auto mr: CGuildRankData::ms_aRankGuild)
	{
		if(GuildID != mr.second.m_GuildID)
			continue;

		HideID += mr.first;
		GS()->AVH(ClientID, HideID, "Rank [{STR}]", mr.second.m_aRank);
		GS()->AVM(ClientID, "MRANKSET", mr.first, HideID, "Change rank name to ({STR})", pPlayer->GetTempData().m_aRankGuildBuf);
		GS()->AVM(ClientID, "MRANKACCESS", mr.first, HideID, "Access rank ({STR})", AccessNames(mr.second.m_Access));
		GS()->AVM(ClientID, "MRANKDELETE", mr.first, HideID, "Delete this rank");
	}
	GS()->AddVotesBackpage(ClientID);
}

/* #########################################################################
	GET CHECK MEMBER INVITE MEMBER
######################################################################### */
int GuildCore::GetGuildPlayerValue(int GuildID)
{
	int MemberPlayers = -1;
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE GuildID = '%d'", GuildID);
		MemberPlayers = pRes->rowsCount();
	return MemberPlayers;
}

/* #########################################################################
	FUNCTIONS MEMBER INVITE MEMBER
######################################################################### */
// add a player to the guild
void GuildCore::SendInviteGuild(int GuildID, CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(pPlayer->Acc().IsGuild())
	{
		GS()->Chat(ClientID, "You are already in the guild {STR}.", GuildName(pPlayer->Acc().m_GuildID));
		return;
	}

	const int UserID = pPlayer->Acc().m_UserID;
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds_invites", "WHERE GuildID = '%d' AND UserID = '%d'",  GuildID, UserID);
	if(pRes->rowsCount() >= 1)
	{
		GS()->Chat(ClientID, "You have already sent a request to join this guild.");
		return;
	}

	Database->Execute<DB::INSERT>("tw_guilds_invites", "(GuildID, UserID) VALUES ('%d', '%d')", GuildID, UserID);
	GS()->ChatGuild(GuildID, "{STR} send invites to join our guilds", Job()->PlayerName(UserID));
	GS()->Chat(ClientID, "You sent a request to join the guild.");
}

// show the invitation sheet to our guild
void GuildCore::ShowInvitesGuilds(int ClientID, int GuildID)
{
	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1900;
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_invites", "WHERE GuildID = '%d'", GuildID);
	while(pRes->next())
	{
		const int SenderID = pRes->getInt("UserID");
		const char *PlayerName = Job()->PlayerName(SenderID);
		GS()->AVH(ClientID, HideID, "Sender {STR} to join guilds", PlayerName);
		{
			GS()->AVM(ClientID, "MINVITEACCEPT", SenderID, HideID, "Accept {STR} to guild", PlayerName);
			GS()->AVM(ClientID, "MINVITEREJECT", SenderID, HideID, "Reject {STR} to guild", PlayerName);
		}
		HideID++;
	}
	GS()->AddVotesBackpage(ClientID);
}

// show the guild's top and call on them
void GuildCore::ShowFinderGuilds(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	GS()->AVL(ClientID, "null", "You are not in guild!");
	GS()->AV(ClientID, "null", "Use reason how Value.");
	GS()->AV(ClientID, "null", "Example: Find guild: [], in reason name.");
	GS()->AV(ClientID, "null");
	GS()->AVM(ClientID, "MINVITENAME", 1, NOPE, "Find guild: {STR}", pPlayer->GetTempData().m_aGuildSearchBuf);

	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1800;
	CSqlString<64> cGuildName = CSqlString<64>(pPlayer->GetTempData().m_aGuildSearchBuf);
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "WHERE Name LIKE '%%%s%%'", cGuildName.cstr());
	while(pRes->next())
	{
		const int GuildID = pRes->getInt("ID");
		const int AvailableSlot = pRes->getInt("AvailableSlots");
		const int PlayersCount = GetGuildPlayerValue(GuildID);
		cGuildName = pRes->getString("Name").c_str();
		GS()->AVH(ClientID, HideID, "{STR} : Leader {STR} : Players [{INT}/{INT}]",
			cGuildName.cstr(), Job()->PlayerName(CGuildData::ms_aGuild[GuildID].m_UserID), PlayersCount, AvailableSlot);
		GS()->AVM(ClientID, "null", NOPE, HideID, "House: {STR} | Bank: {VAL} gold", (GetGuildHouseID(GuildID) <= 0 ? "No" : "Yes"), CGuildData::ms_aGuild[GuildID].m_Bank);

		GS()->AVD(ClientID, "MENU", MENU_GUILD_FINDER_VIEW_PLAYERS, GuildID, HideID, "View player list");

		GS()->AVM(ClientID, "MINVITESEND", GuildID, HideID, "Send request to join {STR}", cGuildName.cstr());
		HideID++;
	}
	GS()->AddVotesBackpage(ClientID);
}

/* #########################################################################
	FUNCTIONS MEMBER HISTORY MEMBER
######################################################################### */
// list of stories
void GuildCore::ShowHistoryGuild(int ClientID, int GuildID)
{
	// looking for the entire history of the guild in the database
	char aBuf[128];
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_history", "WHERE GuildID = '%d' ORDER BY ID DESC LIMIT 20", GuildID);
	while(pRes->next())
	{
		str_format(aBuf, sizeof(aBuf), "[%s] %s", pRes->getString("Time").c_str(), pRes->getString("Text").c_str());
		GS()->AVM(ClientID, "null", NOPE, NOPE, "{STR}", aBuf);
	}
	GS()->AddVotesBackpage(ClientID);
}

// add to the guild history
void GuildCore::AddHistoryGuild(int GuildID, const char *Buffer, ...)
{
	char aBuf[512];
	va_list VarArgs;
	va_start(VarArgs, Buffer);
	#if defined(CONF_FAMILY_WINDOWS)
		_vsnprintf(aBuf, sizeof(aBuf), Buffer, VarArgs);
	#else
		vsnprintf(aBuf, sizeof(aBuf), Buffer, VarArgs);
	#endif
	va_end(VarArgs);

	CSqlString<64> cBuf = CSqlString<64>(aBuf);
	Database->Execute<DB::INSERT>("tw_guilds_history", "(GuildID, Text) VALUES ('%d', '%s')", GuildID, cBuf.cstr());
}

/* #########################################################################
	GET CHECK MEMBER HOUSING MEMBER
######################################################################### */
// guild house
int GuildCore::GetHouseGuildID(int HouseID) const
{
	if(CGuildHouseData::ms_aHouseGuild.find(HouseID) != CGuildHouseData::ms_aHouseGuild.end())
		return CGuildHouseData::ms_aHouseGuild.at(HouseID).m_GuildID;
	return -1;
}

int GuildCore::GetHouseWorldID(int HouseID) const
{
	if (CGuildHouseData::ms_aHouseGuild.find(HouseID) != CGuildHouseData::ms_aHouseGuild.end())
		return CGuildHouseData::ms_aHouseGuild.at(HouseID).m_GuildID;
	return -1;
}

// search by position
int GuildCore::GetPosHouseID(vec2 Pos) const
{
	for(const auto& m: CGuildHouseData::ms_aHouseGuild)
	{
		if (m.second.m_WorldID != GS()->GetWorldID())
			continue;

		vec2 PositionHouse = vec2(m.second.m_PosX, m.second.m_PosY);
		if(distance(Pos, PositionHouse) < 1000)
			return m.first;
	}
	return -1;
}

bool GuildCore::GetGuildDoor(int GuildID) const
{
	const int HouseID = GetGuildHouseID(GuildID);
	if(HouseID > 0 && CGuildHouseData::ms_aHouseGuild.find(HouseID) != CGuildHouseData::ms_aHouseGuild.end())
		return (bool)CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor;
	return false;
}

vec2 GuildCore::GetPositionHouse(int GuildID) const
{
	const int HouseID = GetGuildHouseID(GuildID);
	if(HouseID > 0 && CGuildHouseData::ms_aHouseGuild.find(HouseID) != CGuildHouseData::ms_aHouseGuild.end())
		return vec2(CGuildHouseData::ms_aHouseGuild[HouseID].m_PosX, CGuildHouseData::ms_aHouseGuild[HouseID].m_PosY);
	return vec2(0, 0);
}

int GuildCore::GetGuildHouseID(int GuildID) const
{
	for(const auto& imh : CGuildHouseData::ms_aHouseGuild)
	{
		if(GuildID > 0 && GuildID == imh.second.m_GuildID)
			return imh.first;
	}
	return -1;
}

// buying a guild house
void GuildCore::BuyGuildHouse(int GuildID, int HouseID)
{
	// check if the guild has a house
	if(GetGuildHouseID(GuildID) > 0)
	{
		GS()->ChatGuild(GuildID, "Your Guild can't have 2 houses. Purchase canceled!");
		return;
	}

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_houses", "WHERE ID = '%d' AND GuildID IS NULL", HouseID);
	if(pRes->next())
	{
		const int Price = pRes->getInt("Price");
		if(CGuildData::ms_aGuild[GuildID].m_Bank < Price)
		{
			GS()->ChatGuild(GuildID, "This Guild house requires {VAL}gold!", Price);
			return;
		}
		CGuildData::ms_aGuild[GuildID].m_Bank -= Price;
		Database->Execute<DB::UPDATE>("tw_guilds", "Bank = '%d' WHERE ID = '%d'", CGuildData::ms_aGuild[GuildID].m_Bank, GuildID);

		CGuildHouseData::ms_aHouseGuild[HouseID].m_GuildID = GuildID;
		Database->Execute<DB::UPDATE>("tw_guilds_houses", "GuildID = '%d' WHERE ID = '%d'", GuildID, HouseID);

		const char* WorldName = Server()->GetWorldName(CGuildHouseData::ms_aHouseGuild[HouseID].m_WorldID);
		GS()->Chat(-1, "{STR} buyight guild house on {STR}!", GuildName(GuildID), WorldName);
		GS()->ChatDiscord(DC_SERVER_INFO, "Information", "{STR} bought guild house on {STR}!", GuildName(GuildID), WorldName);
		AddHistoryGuild(GuildID, "Bought a house on '%s'.", WorldName);
		return;
	}

	GS()->ChatGuild(GuildID, "House has already been purchased!");
}

// guild house sale
void GuildCore::SellGuildHouse(int GuildID)
{
	const int HouseID = GetGuildHouseID(GuildID);
	if(HouseID <= 0)
	{
		GS()->ChatGuild(GuildID, "Your Guild doesn't have a home!");
		return;
	}

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds_houses", "WHERE ID = '%d' AND GuildID IS NOT NULL", HouseID);
	if(pRes->next())
	{
		Database->Execute<DB::UPDATE>("tw_guilds_houses", "GuildID = NULL WHERE ID = '%d'", HouseID);

		const int ReturnedGold = CGuildHouseData::ms_aHouseGuild[HouseID].m_Price;
		GS()->SendInbox("System", CGuildData::ms_aGuild[GuildID].m_UserID, "Your guild house sold.", "We returned some gold from your guild.", itGold, ReturnedGold);

		GS()->ChatGuild(GuildID, "House sold, {VAL}gold returned to leader", ReturnedGold);
		AddHistoryGuild(GuildID, "Lost a house on '%s'.", Server()->GetWorldName(CGuildHouseData::ms_aHouseGuild[HouseID].m_WorldID));
	}

	if(CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor)
	{
		delete CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor;
		CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor = 0;
	}
	CGuildHouseData::ms_aHouseGuild[HouseID].m_GuildID = -1;
}

void GuildCore::ShowBuyHouse(CPlayer *pPlayer, int HouseID)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AVH(ClientID, TAB_INFO_GUILD_HOUSE, "Information Member Housing");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_GUILD_HOUSE, "Buying a house you will need to constantly the Treasury");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_GUILD_HOUSE, "In the intervals of time will be paid house");
	GS()->AV(ClientID, "null");

	// check player guild
	if(pPlayer->Acc().IsGuild())
	{
		const int GuildBank = CGuildData::ms_aGuild[pPlayer->Acc().m_GuildID].m_Bank;
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Your guild have {VAL} Gold", GuildBank);
	}

	// check valid house
	if(CGuildHouseData::ms_aHouseGuild.find(HouseID) == CGuildHouseData::ms_aHouseGuild.end())
	{
		GS()->AVL(ClientID, "null", "This house is not for sale yet");
	}
	else
	{
		const int GuildHouseOwner = CGuildHouseData::ms_aHouseGuild[HouseID].m_GuildID;
		if(GuildHouseOwner > 0)
			GS()->AVM(ClientID, "null", NOPE, NOPE, "Guild owner house: {STR}", CGuildData::ms_aGuild[GuildHouseOwner].m_aName);
		else
			GS()->AVM(ClientID, "BUYMEMBERHOUSE", HouseID, NOPE, "Buy this guild house! Price: {VAL}", CGuildHouseData::ms_aHouseGuild[HouseID].m_Price);
	}
	GS()->AV(ClientID, "null");
}

bool GuildCore::ChangeStateDoor(int GuildID)
{
	const int HouseID = GetGuildHouseID(GuildID);
	if(CGuildHouseData::ms_aHouseGuild.find(HouseID) == CGuildHouseData::ms_aHouseGuild.end())
		return false;

	if(CGuildHouseData::ms_aHouseGuild[HouseID].m_WorldID != GS()->GetWorldID())
	{
		GS()->ChatGuild(GuildID, "Change state door can only near your house.");
		return false;
	}

	if(CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor)
	{
		delete CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor;
		CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor = 0;
	}
	else
		CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor = new GuildDoor(&GS()->m_World, vec2(CGuildHouseData::ms_aHouseGuild[HouseID].m_DoorX, CGuildHouseData::ms_aHouseGuild[HouseID].m_DoorY), GuildID);

	const bool StateDoor = (bool)(CGuildHouseData::ms_aHouseGuild[HouseID].m_pDoor);
	GS()->ChatGuild(GuildID, "{STR} the house for others.", (StateDoor ? "closed" : "opened"));
	return true;
}
