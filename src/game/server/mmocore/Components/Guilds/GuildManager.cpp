/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/GameEntities/decoration_houses.h>
#include "Entities/GuildDoor.h"

#include <game/server/mmocore/Components/Inventory/InventoryManager.h>

#include <cstdarg>

void CGuildManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds");
	while(pRes->next())
	{
			GuildIdentifier ID = pRes->getInt("ID");
			std::string Name = pRes->getString("Name").c_str();
			std::string MembersData = pRes->getString("Members").c_str();
			GuildRankIdentifier DefaultRankID = pRes->getInt("DefaultRankID");
			int OwnerUID = pRes->getInt("UserID");
			int Level = pRes->getInt("Level");
			int Experience = pRes->getInt("Experience");
			int Bank = pRes->getInt("Bank");
			int Score = pRes->getInt("Score");

			CGuildData::CreateElement(ID)->Init(Name, std::move(MembersData), DefaultRankID, Level, Experience, Score, OwnerUID, Bank);
	}

	Job()->ShowLoadingProgress("Guilds", CGuildData::Data().size());
}

void CGuildManager::OnInitWorld(const char* pWhereLocalWorld)
{
	// load houses
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_houses");
	while(pRes->next())
	{
		GuildHouseIdentifier ID = pRes->getInt("ID");
		GuildIdentifier GuildID = pRes->getInt("GuildID");
		int WorldID = pRes->getInt("WorldID");
		int Price = pRes->getInt("Price");
		vec2 Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		vec2 TextPosition = vec2(pRes->getInt("TextX"), pRes->getInt("TextY"));
		std::string JsonDoorsData = pRes->getString("JsonDoorsData");
		auto IterGuild = std::find_if(CGuildData::Data().begin(), CGuildData::Data().end(), [&GuildID](const GuildDataPtr p){ return p->GetID() == GuildID; });
		CGuildData* pGuild = IterGuild != CGuildData::Data().end() ? IterGuild->get() : nullptr;

		CGuildHouseData::CreateElement(ID)->Init(pGuild, Price, Position, TextPosition, WorldID, std::move(JsonDoorsData));
	}
	Job()->ShowLoadingProgress("Houses", CGuildHouseData::Data().size());
}
void CGuildManager::OnTick()
{
	//TickHousingText();
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

			//const int Exp = CGuildData::ms_aGuild[GuildID].m_UpgradeData(CGuildData::CHAIR_EXPERIENCE, 0).m_Value;
			//pPlayer->Account()->AddExperience(Exp);
		}
		return true;
	}

	return false;
}

/*
	TODO: We have to process checks in functions, they exist for something.
*/
bool CGuildManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// -------------------------------------
	// ACCESS RANK: Leader functions
	if(PPSTR(CMD, "MLEADER") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "BUYMEMBERHOUSE") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "MHOUSESELL") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "MDISBAND") == 0)
	{
		return true;
	}


	if(PPSTR(CMD, "MRANKDELETE") == 0)
	{
		return true;
	}


	if(PPSTR(CMD, "MRANKCHANGE") == 0)
	{
		return true;
	}


	// -------------------------------------
	// ACCESS RANK: Invite and kick functions
	if(PPSTR(CMD, "MKICK") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "MINVITEACCEPT") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "MINVITEREJECT") == 0)
	{
		return true;
	}


	// -------------------------------------
	// ACCESS RANK: Upgrade house functions
	if(PPSTR(CMD, "MDOOR") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "MUPGRADE") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "DECOGUILDSTART") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "DECOGUILDDELETE") == 0)
	{
		return true;
	}


	// -------------------------------------
	// ACCESS RANK: Full access functions
	if(PPSTR(CMD, "MSPAWN") == 0)
	{
		return true;
	}
	if(PPSTR(CMD, "MMONEY") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "RANK_NAME_FIELD") == 0)
	{
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Please use a different name.");
			return true;
		}

		str_copy(pPlayer->GetTempData().m_aRankGuildBuf, GetText, sizeof(pPlayer->GetTempData().m_aRankGuildBuf));
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	if(PPSTR(CMD, "RANK_CREATE") == 0)
	{
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildAccountSlot()->GetRank()->CheckAccess(pPlayer, RIGHTS_LEADER))
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

		GUILD_RANK_RESULT Result = pPlayer->Account()->GetGuild()->GetRanks()->Add(pPlayer->GetTempData().m_aRankGuildBuf);
		if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->Chat(ClientID, "The rank '{STR}' has been successfully added!", pPlayer->GetTempData().m_aRankGuildBuf);
			GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		}
		else if(Result == GUILD_RANK_RESULT::ADD_ALREADY_EXISTS)
		{
			GS()->Chat(ClientID, "The rank name already exists");
		}
		else if(Result == GUILD_RANK_RESULT::ADD_LIMIT_HAS_REACHED)
		{
			GS()->Chat(ClientID, "Rank limit reached, {INT} out of {INT}", (int)MAX_GUILD_RANK_NUM, (int)MAX_GUILD_RANK_NUM);
		}

		return true;
	}

	if(PPSTR(CMD, "RANK_RENAME") == 0)
	{
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildAccountSlot()->GetRank()->CheckAccess(pPlayer, RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int RankID = VoteID;
		const int LengthRank = str_length(pPlayer->GetTempData().m_aRankGuildBuf);
		if(LengthRank < 2 || LengthRank > 16)
		{
			GS()->Chat(ClientID, "Minimum number of characters 2, maximum 16.");
			return true;
		}

		GUILD_RANK_RESULT Result = pPlayer->Account()->GetGuild()->GetRanks()->Get(RankID)->ChangeName(pPlayer->GetTempData().m_aRankGuildBuf);
		if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		}
		else if(Result == GUILD_RANK_RESULT::RENAME_ALREADY_NAME_EXISTS)
		{
			GS()->Chat(ClientID, "The name is already in use by another rank");
		}

		return true;
	}

	if(PPSTR(CMD, "RANK_REMOVE") == 0)
	{
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildAccountSlot()->GetRank()->CheckAccess(pPlayer, RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int RankID = VoteID;
		CGuildRankData* pRank = pPlayer->Account()->GetGuild()->GetRanks()->Get(RankID);
		GUILD_RANK_RESULT Result = pPlayer->Account()->GetGuild()->GetRanks()->Remove(pRank->GetName());
		if(Result == GUILD_RANK_RESULT::SUCCESSFUL)
		{
			GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		}
		else if(Result == GUILD_RANK_RESULT::REMOVE_RANK_IS_DEFAULT)
		{
			GS()->Chat(ClientID, "You can't remove default rank");
		}
		else if(Result == GUILD_RANK_RESULT::REMOVE_RANK_DOES_NOT_EXIST)
		{
			GS()->Chat(ClientID, "There is no such rank");
		}

		return true;
	}

	if(PPSTR(CMD, "RANK_ACCESS") == 0)
	{
		if(!pPlayer->Account()->HasGuild() || !pPlayer->Account()->GetGuildAccountSlot()->GetRank()->CheckAccess(pPlayer, RIGHTS_LEADER))
		{
			GS()->Chat(ClientID, "You have no access, or you are not a member of the guild.");
			return true;
		}

		const int RankID = VoteID;
		CGuildRankData* pRank = pPlayer->Account()->GetGuild()->GetRanks()->Get(RankID);
		pRank->ChangeAccess();
		GS()->StrongUpdateVotesForAll(MENU_GUILD_RANK);
		return true;
	}

	// -------------------------------------
	// Functions outside the guild
	if(PPSTR(CMD, "MINVITENAME") == 0)
	{
		return true;
	}

	if(PPSTR(CMD, "MINVITESEND") == 0)
	{
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
			//const int GuildHouseID = GetPosHouseID(pChr->m_Core.m_Pos);
			//Job()->Member()->ShowBuyHouse(pPlayer, GuildHouseID);
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
		//ShowGuildPlayers(pPlayer, pPlayer->m_TempMenuValue);
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
		//ShowGuildPlayers(pPlayer, pPlayer->Account()->m_GuildID);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}


	if(Menulist == MENU_GUILD_HISTORY)
	{
		pPlayer->m_LastVoteMenu = MENU_GUILD;
		//ShowHistoryGuild(ClientID, pPlayer->Account()->m_GuildID);
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
		//ShowInvitesGuilds(ClientID, pPlayer->Account()->m_GuildID);
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
		//ShowDecorationList(pPlayer);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}
	return false;
}

/* #########################################################################
	FUNCTIONS MEMBER MEMBER
######################################################################### */
void CGuildManager::CreateGuild(CPlayer *pPlayer, const char *pGuildName)
{
	if(!pPlayer)
	{
		return;
	}

	// check whether we are already in the guild
	const int ClientID = pPlayer->GetCID();
	if(pPlayer->Account()->HasGuild())
	{
		GS()->Chat(ClientID, "You already in guild group!");
		return;
	}

	// we check the availability of the guild's name
	CSqlString<64> GuildName(pGuildName);
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", TW_GUILD_TABLE, "WHERE Name = '%s'", GuildName.cstr());
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
	GuildDataPtr pGuild = CGuildData::CreateElement(InitID);
	std::string MembersData = R"({"members":[{"id":)" + std::to_string(pPlayer->Account()->GetID()) + R"(,"rank_id":0,"deposit":0}]})";
	pGuild->Init(GuildName.cstr(), std::forward<std::string>(MembersData), -1, 1, 0, 0, pPlayer->Account()->GetID(), 0);
	pPlayer->Account()->ReinitializeGuild();

	// we create a guild in the table
	Database->Execute<DB::INSERT>("tw_guilds", "(ID, Name, UserID, Members) VALUES ('%d', '%s', '%d', '%s')", InitID, GuildName.cstr(), pPlayer->Account()->GetID(), MembersData.c_str());
	GS()->Chat(-1, "New guilds [{STR}] have been created!", GuildName.cstr());
	GS()->StrongUpdateVotes(ClientID, MENU_MAIN);
}

void CGuildManager::ShowMenuGuild(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	int ClientID = pPlayer->GetCID();
	int GuildID = pGuild->GetID();
	bool HasHouse = pGuild->HasHouse();
	int ExpNeed = computeExperience(pGuild->GetLevel());

	GS()->AVH(ClientID, TAB_GUILD_STAT, "Name: {STR} : Leader {STR}", pGuild->GetName(), Server()->GetAccountNickname(pGuild->GetOwnerUID()));
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Level: {INT} Experience: {INT}/{INT}", pGuild->GetLevel(), pGuild->GetExperience(), ExpNeed);
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Maximal available player count: {INT}", pGuild->GetUpgrades()(CGuildData::AVAILABLE_SLOTS, 0).m_Value);
	GS()->AVM(ClientID, "null", NOPE, TAB_GUILD_STAT, "Guild Bank: {VAL}gold", pGuild->GetBank()->Get());
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "◍ Your gold: {VAL}gold", pPlayer->GetItem(itGold)->GetValue());
	GS()->AVL(ClientID, "MMONEY", "Add gold guild bank. (Amount in a reason)", pGuild->GetName());
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "▤ Guild system");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_VIEW_PLAYERS, NOPE, "List of players");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_INVITES, NOPE, "Requests membership");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_HISTORY, NOPE, "History of activity");
	GS()->AVM(ClientID, "MENU", MENU_GUILD_RANK, NOPE, "Rank settings");
	if(HasHouse)
	{
		GS()->AV(ClientID, "null");
		GS()->AVL(ClientID, "null", "⌂ Housing system");
		GS()->AVM(ClientID, "MENU", MENU_GUILD_HOUSE_DECORATION, NOPE, "Settings Decoration(s)");
		//GS()->AVL(ClientID, "MDOOR", "Change state (\"{STR}\")", GetGuildDoor(GuildID) ? "OPEN" : "CLOSED");
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
	if(HasHouse)
	{
		for(int i = CGuildData::CHAIR_EXPERIENCE; i < CGuildData::NUM_GUILD_UPGRADES; i++)
		{
			const char* pUpgradeName = pGuild->GetUpgrades()(i, 0).getDescription();
			const int PriceUpgrade = pGuild->GetUpgrades()(i, 0).m_Value * g_Config.m_SvPriceUpgradeGuildAnother;
			GS()->AVM(ClientID, "MUPGRADE", i, NOPE, "Upgrade {STR} ({INT}) {VAL}gold", pUpgradeName, pGuild->GetUpgrades()(i, 0).m_Value, PriceUpgrade);
		}
	}

	const char* pUpgradeName = pGuild->GetUpgrades()(CGuildData::AVAILABLE_SLOTS, 0).getDescription();
	const int UpgradeValue = pGuild->GetUpgrades()(CGuildData::AVAILABLE_SLOTS, 0).m_Value;
	const int PriceUpgrade = UpgradeValue * g_Config.m_SvPriceUpgradeGuildSlot;
	GS()->AVM(ClientID, "MUPGRADE", CGuildData::AVAILABLE_SLOTS, NOPE, "Upgrade {STR} ({INT}) {VAL}gold", pUpgradeName, UpgradeValue, PriceUpgrade);
	GS()->AddVotesBackpage(ClientID);
}

/* #########################################################################
	GET CHECK MEMBER RANK MEMBER
######################################################################### */
// rank menu display
void CGuildManager::ShowMenuRank(CPlayer *pPlayer)
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
	GS()->AVM(ClientID, "RANK_NAME_FIELD", 1, NOPE, "Name rank: {STR}", pPlayer->GetTempData().m_aRankGuildBuf);
	GS()->AVM(ClientID, "RANK_CREATE", 1, NOPE, "Create new rank");
	GS()->AV(ClientID, "null");

	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1300;
	for(auto pRank : pPlayer->Account()->GetGuild()->GetRanks()->GetContainer())
	{
		GuildRankIdentifier ID = pRank->GetID();
		bool IsDefaultRank = pRank == pPlayer->Account()->GetGuild()->GetRanks()->GetDefaultRank();

		GS()->AVH(ClientID, HideID, "Rank [{STR}] {STR}", pRank->GetName(), IsDefaultRank ? " - Default" : "\0");
		GS()->AVM(ClientID, "RANK_RENAME", ID, HideID, "Rename to ({STR})", pPlayer->GetTempData().m_aRankGuildBuf);

		if(!IsDefaultRank)
		{
			GS()->AVM(ClientID, "RANK_ACCESS", ID, HideID, "Access ({STR})", pRank->GetAccessName());
			GS()->AVM(ClientID, "RANK_REMOVE", ID, HideID, "Remove");
		}
		HideID++;
	}

	GS()->AddVotesBackpage(ClientID);
}

/* #########################################################################
	FUNCTIONS MEMBER INVITE MEMBER
######################################################################### */
// add a player to the guild
void CGuildManager::SendInviteGuild(int GuildID, CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(pPlayer->Account()->HasGuild())
	{
		GS()->Chat(ClientID, "You are already in the guild.");
		return;
	}

	const int UserID = pPlayer->Account()->GetID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_guilds_invites", "WHERE GuildID = '%d' AND UserID = '%d'",  GuildID, UserID);
	if(pRes->rowsCount() >= 1)
	{
		GS()->Chat(ClientID, "You have already sent a request to join this guild.");
		return;
	}

	Database->Execute<DB::INSERT>("tw_guilds_invites", "(GuildID, UserID) VALUES ('%d', '%d')", GuildID, UserID);
	GS()->ChatGuild(GuildID, "{STR} send invites to join our guilds", Server()->GetAccountNickname(UserID));
	GS()->Chat(ClientID, "You sent a request to join the guild.");
}

// show the invitation sheet to our guild
void CGuildManager::ShowInvitesGuilds(int ClientID, int GuildID)
{
	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1900;
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_invites", "WHERE GuildID = '%d'", GuildID);
	while(pRes->next())
	{
		const int SenderID = pRes->getInt("UserID");
		const char *PlayerName = Server()->GetAccountNickname(SenderID);
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
void CGuildManager::ShowFinderGuilds(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(pPlayer->Account()->HasGuild())
	{
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		GS()->AVL(ClientID, "null", "\u02DA\u029A\u2665\u025E\u02DA You already in guild '{STR}'!", pGuild->GetName());
		GS()->AV(ClientID, "null");
	}

	GS()->AV(ClientID, "null", "Use reason how Value.");
    GS()->AV(ClientID, "null", "Example: Find guild: \u300E\u300F, in reason name.");
    GS()->AV(ClientID, "null");
	GS()->AV(ClientID, "null", "\u270E Search for a guild by name.");
    GS()->AVM(ClientID, "MINVITENAME", 1, NOPE, "Find guild: \u300E{STR}\u300F", pPlayer->GetTempData().m_aGuildSearchBuf);
	GS()->AV(ClientID, "null");

    int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 1800;
	for(auto& pGuild : CGuildData::Data())
	{
		int AvailableSlots = 20; // TODO
		int PlayersNum = pGuild->GetMembers()->GetContainer().size();
		int OwnerUID = pGuild->GetOwnerUID();

		GS()->AVH(ClientID, HideID, "{STR} : Leader {STR} ({INT} of {INT} players)", pGuild->GetName(), Server()->GetAccountNickname(OwnerUID), PlayersNum, AvailableSlots);
		if(pGuild->HasHouse())
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "* The guild has its own house");
		}
		else
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "* The guild doesn't have its own house");
		}
		GS()->AVM(ClientID, "null", NOPE, HideID, "* Accumulations are: {VAL} gold's", pGuild->GetBank()->Get());

		GS()->AVD(ClientID, "MENU", MENU_GUILD_FINDER_VIEW_PLAYERS, pGuild->GetID(), HideID, "View player list");
		GS()->AVM(ClientID, "MINVITESEND", pGuild->GetID(), HideID, "Send request to join {STR}", pGuild->GetName());
		HideID++;
	}

    GS()->AddVotesBackpage(ClientID);
}

/* #########################################################################
	FUNCTIONS MEMBER HISTORY MEMBER
######################################################################### */
// list of stories
void CGuildManager::ShowHistoryGuild(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->Account()->HasGuild())
		return;

	char aBuf[128];
	CGuildData* pGuild = pPlayer->Account()->GetGuild();
	GuildHistoryContainer aHistory = std::move(pGuild->GetHistory()->GetLogs());
	for(auto& pLog : aHistory)
	{
		str_format(aBuf, sizeof(aBuf), "[%s] %s", pLog.m_Time.c_str(), pLog.m_Log.c_str());
		GS()->AVM(ClientID, "null", NOPE, NOPE, "{STR}", aBuf);
	}

	GS()->AddVotesBackpage(ClientID);
}

/* #########################################################################
	GET CHECK MEMBER HOUSING MEMBER
######################################################################### */
CGuildHouseData* CGuildManager::GetGuildHouseByPos(vec2 Pos) const
{
	auto itHouse = std::find_if(CGuildHouseData::Data().begin(), CGuildHouseData::Data().end(), [&Pos, this](GuildHouseDataPtr p)
	{
		return GS()->GetWorldID() == p->GetWorldID() && distance(Pos, p->GetPos()) < 360.f;
	});

	return itHouse != CGuildHouseData::Data().end() ? itHouse->get() : nullptr;
}

CGuildData* CGuildManager::GetGuildByID(GuildIdentifier ID) const
{
	auto itGuild = std::find_if(CGuildData::Data().begin(), CGuildData::Data().end(), [&ID](GuildDataPtr p)
	{
		return p->GetID() == ID;
	});

	return itGuild != CGuildData::Data().end() ? itGuild->get() : nullptr;
}

void CGuildManager::ShowBuyHouse(CPlayer *pPlayer, CGuildHouseData* pHouse)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AVH(ClientID, TAB_INFO_GUILD_HOUSE, "Information Member Housing");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_GUILD_HOUSE, "Buying a house you will need to constantly the Treasury");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_GUILD_HOUSE, "In the intervals of time will be paid house");
	GS()->AV(ClientID, "null");

	if(pPlayer->Account()->HasGuild())
	{
		CGuildData* pGuild = pPlayer->Account()->GetGuild();
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Your guild have {VAL} Gold", pGuild->GetBank()->Get());
	}

	if(!pHouse)
	{
		GS()->AVL(ClientID, "null", "This house is not for sale yet");
		return;
	}

	if(pHouse->IsPurchased())
	{
		CGuildData* pGuild = pHouse->GetGuild();
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Guild owner house: {STR}", pGuild->GetName());
		return;
	}

	GS()->AVM(ClientID, "BUYMEMBERHOUSE", pHouse->GetID(), NOPE, "Buy this guild house! Price: {VAL}", pHouse->GetPrice());
}
