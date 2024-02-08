/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mmo_controller.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include "components/Accounts/AccountManager.h"
#include "components/Accounts/AccountMinerManager.h"
#include "components/Accounts/AccountPlantManager.h"
#include "components/Auction/AuctionManager.h"
#include "components/aethernet/aethernet_manager.h"
#include "components/Bots/BotManager.h"
#include "components/Crafts/CraftManager.h"
#include "components/Dungeons/DungeonManager.h"
#include "components/Eidolons/EidolonManager.h"
#include "components/Groups/GroupManager.h"
#include "components/Guilds/GuildManager.h"
#include "components/Houses/HouseManager.h"
#include "components/Inventory/InventoryManager.h"
#include "components/Mails/MailBoxManager.h"
#include "components/Quests/QuestManager.h"
#include "components/Skills/SkillManager.h"
#include "components/Tutorial/TutorialManager.h"
#include "components/Warehouse/WarehouseManager.h"
#include "components/Worlds/WorldManager.h"


CMmoController::CMmoController(CGS* pGameServer) : m_pGameServer(pGameServer)
{
	// order
	m_System.add(m_pQuestManager = new CQuestManager);
	m_System.add(m_pBotManager = new CBotManager);
	m_System.add(m_pInventoryManager = new CInventoryManager);
	m_System.add(m_pCraftManager = new CCraftManager);
	m_System.add(m_pWarehouseManager = new CWarehouseManager);
	m_System.add(new CAuctionManager);
	m_System.add(m_pEidolonManager = new CEidolonManager);
	m_System.add(m_pDungeonManager = new CDungeonManager);
	m_System.add(new CAethernetManager);
	m_System.add(m_pWorldManager = new CWorldManager);
	m_System.add(m_pHouseManager = new CHouseManager);
	m_System.add(m_pGuildManager = new CGuildManager);
	m_System.add(m_pGroupManager = new CGroupManager);
	m_System.add(m_pSkillManager = new CSkillManager);
	m_System.add(m_pTutorialManager = new CTutorialManager);
	m_System.add(m_pAccountManager = new CAccountManager);
	m_System.add(m_pAccountMinerManager = new CAccountMinerManager);
	m_System.add(m_pAccountPlantManager = new CAccountPlantManager);
	m_System.add(m_pMailboxManager = new CMailboxManager);

	for(auto& pComponent : m_System.m_vComponents)
	{
		pComponent->m_Core = this;
		pComponent->m_GameServer = pGameServer;
		pComponent->m_pServer = pGameServer->Server();

		if(m_pGameServer->GetWorldID() == MAIN_WORLD_ID)
			pComponent->OnInit();

		char aLocalSelect[64];
		str_format(aLocalSelect, sizeof(aLocalSelect), "WHERE WorldID = '%d'", m_pGameServer->GetWorldID());
		pComponent->OnInitWorld(aLocalSelect);
	}
}

CMmoController::~CMmoController()
{
	m_System.free();
}

void CMmoController::OnTick()
{
	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnTick();

	// Check if the current tick is a multiple of the time period check time
	if(GS()->Server()->Tick() % ((GS()->Server()->TickSpeed() * 60) * g_Config.m_SvTimePeriodCheckTime) == 0)
		HandleTimePeriod();
}

bool CMmoController::OnMessage(int MsgID, void* pRawMsg, int ClientID)
{
	if(GS()->Server()->ClientIngame(ClientID) && GS()->GetPlayer(ClientID))
	{
		for(auto& pComponent : m_System.m_vComponents)
		{
			if(pComponent->OnMessage(MsgID, pRawMsg, ClientID))
				return true;
		}
	}

	return false;
}

void CMmoController::OnInitAccount(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnInitAccount(pPlayer);
}

bool CMmoController::OnPlayerHandleMainMenu(int ClientID, int Menulist)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return true;

	// ----------------------------------------
	// check replaced votes
	for(auto& pComponent : m_System.m_vComponents)
	{
		// Check if the player's m_ZoneInvertMenu variable is true
		if(pPlayer->m_ZoneInvertMenu)
		{
			CVoteWrapper(ClientID).AddOption("ZONE_INVERT_MENU", "▶▶▶▶ Back to zone menu. ▶▶▶▶");
			CVoteWrapper::AddLine(ClientID);
			break;
		}

		// Call the function OnHandleMenulist of the pComponent object with the parameters pPlayer and Menulist
		if(pComponent->OnHandleMenulist(pPlayer, Menulist, true))
		{
			// Display a notification to the client with the message "<<<< Back to player menu. <<<<"
			CVoteWrapper::AddLine(ClientID);
			CVoteWrapper(ClientID).AddOption("ZONE_INVERT_MENU", "◀◀◀◀ Back to player menu. ◀◀◀◀");
			return true;
		}
	}

	// ----------------------------------------

	// main menu
	if(Menulist == MENU_MAIN)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		// statistics menu
		const int ExpForLevel = computeExperience(pPlayer->Account()->GetLevel());
		CVoteWrapper VMain(ClientID, HIDE_DEFAULT_OPEN, "Hi, {STR} Last log in {STR}", GS()->Server()->ClientName(ClientID), pPlayer->Account()->GetLastLoginDate());
		VMain.Add("Level {INT} : Exp {INT}/{INT}", pPlayer->Account()->GetLevel(), pPlayer->Account()->GetExperience(), ExpForLevel);
		VMain.Add("Skill Point {INT}SP", pPlayer->GetItem(itSkillPoint)->GetValue());
		VMain.Add("Gold: {VAL}", pPlayer->GetItem(itGold)->GetValue());
		CVoteWrapper::AddLine(ClientID);

		// personal menu
		CVoteWrapper VPersonal(ClientID, HIDE_DEFAULT_OPEN, "☪ PERSONAL");
		VPersonal.Add(MENU_INVENTORY, "\u205C Inventory");
		VPersonal.Add(MENU_EQUIPMENT, "\u2604 Equipment");
		VPersonal.Add(MENU_UPGRADES, "\u2657 Upgrades({INT}p)", pPlayer->Account()->m_Upgrade);
		VPersonal.Add(MENU_EIDOLON_COLLECTION, "\u2727 Eidolon Collection");
		VPersonal.Add(MENU_DUNGEONS, "\u262C Dungeons");
		VPersonal.Add(MENU_GROUP, "\u2042 Group");
		VPersonal.Add(MENU_SETTINGS, "\u2692 Settings");
		VPersonal.Add(MENU_INBOX, "\u2709 Mailbox");
		VPersonal.Add(MENU_JOURNAL_MAIN, "\u270D Journal");
		VPersonal.AddIf(pPlayer->Account()->HasHouse(), MENU_HOUSE, "\u2302 House");
		VPersonal.Add(MENU_GUILD_FINDER, "\u20AA Guild finder");
		VPersonal.AddIf(pPlayer->Account()->HasGuild(), MENU_GUILD, "\u32E1 Guild");
		CVoteWrapper::AddLine(ClientID);
		CVoteWrapper::AddLine(ClientID);
		CVoteWrapper::AddLine(ClientID);

		// info menu
		CVoteWrapper VInfo(ClientID, HIDE_DEFAULT_OPEN, "☪ INFORMATION");
		VInfo.Add(MENU_GUIDE_GRINDING, "\u10D3 Wiki / Grinding Guide ");
		VInfo.Add(MENU_TOP_LIST, "\u21F0 Ranking guilds and players");
		return true;
	}

	// player upgrades
	if(Menulist == MENU_UPGRADES)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		// information
		CVoteWrapper VUpgradesInfo(ClientID, HIDE_DEFAULT_CLOSE, "Upgrades Information");
		VUpgradesInfo.Add("Select upgrades type in Reason, write count.");
		VUpgradesInfo.AddLine();

		// upgrade point
		CVoteWrapper::AddLine(ClientID);
		CVoteWrapper(ClientID).Add("Upgrade Point's: {INT}P", pPlayer->Account()->m_Upgrade);
		CVoteWrapper::AddLine(ClientID);

		// lambda function for easy use
		auto AddUpgradeGroupToWrapper([&](AttributeGroup Type, CVoteWrapper* pWrapper)
		{
			for(auto& [ID, pAttribute] : CAttributeDescription::Data())
			{
				if(pAttribute->IsGroup(Type) && pAttribute->HasDatabaseField())
				{
					// if upgrades are cheap, they have a division of statistics
					const int AttributeSize = pPlayer->GetAttributeSize(ID);

					// percent data TODO: extract percent attributes
					char aBuf[64]{};
					float Percent = pPlayer->GetAttributePercent(ID);
					if(Percent)
					{
						str_format(aBuf, sizeof(aBuf), "(%0.4f%%)", Percent);
					}
					pWrapper->Add("\u2508\u2508 {STR} - {INT}{STR}", pAttribute->GetName(), AttributeSize, aBuf);
				}
			}

			for(auto& [ID, pAttribute] : CAttributeDescription::Data())
			{
				if(pAttribute->IsGroup(Type) && pAttribute->HasDatabaseField())
					pWrapper->AddOption("UPGRADE", (int)ID, pAttribute->GetUpgradePrice(), "{STR} - {INT} (cost {INT} point)",
						pAttribute->GetName(), pPlayer->Account()->m_aStats[ID], pAttribute->GetUpgradePrice());
			}
		});

		// Disciple of War
		CVoteWrapper VUpgrDPS(ClientID, BORDER_STRICT_BOLD, "\u2694 Disciple of War : Strength {VAL}", pPlayer->GetTypeAttributesSize(AttributeGroup::Dps));
		AddUpgradeGroupToWrapper(AttributeGroup::Dps, &VUpgrDPS);
		VUpgrDPS.AddLine();

		// Disciple of Tank
		CVoteWrapper VUpgrTANK(ClientID, BORDER_STRICT_BOLD, "\u2699 Disciple of Tank : Endurance {VAL}", pPlayer->GetTypeAttributesSize(AttributeGroup::Tank));
		AddUpgradeGroupToWrapper(AttributeGroup::Tank, &VUpgrTANK);
		VUpgrTANK.AddLine();

		// Disciple of Healer
		CVoteWrapper VUpgrHEALER(ClientID, BORDER_STRICT_BOLD, "\u2696 Disciple of Healer : Power {VAL}", pPlayer->GetTypeAttributesSize(AttributeGroup::Healer));
		AddUpgradeGroupToWrapper(AttributeGroup::Healer, &VUpgrHEALER);
		VUpgrHEALER.AddLine();

		// Upgrades Weapons and ammo
		CVoteWrapper VUpgrWeapon(ClientID, BORDER_STRICT_BOLD, "\u2690 Upgrades Weapons / Ammo");
		AddUpgradeGroupToWrapper(AttributeGroup::Weapon, &VUpgrWeapon);
		VUpgrWeapon.AddLine();

		// Add back page
		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// top list
	if(Menulist == MENU_TOP_LIST)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		GS()->AVH(ClientID, TAB_INFO_TOP, "Ranking Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_TOP, "Here you can see top server Guilds, Players.");
		GS()->AV(ClientID, "null");

		GS()->AVM(ClientID, "SORTEDTOP", (int)ToplistType::GUILDS_LEVELING, NOPE, "Top 10 guilds leveling");
		GS()->AVM(ClientID, "SORTEDTOP", (int)ToplistType::GUILDS_WEALTHY, NOPE, "Top 10 guilds wealthy");
		GS()->AVM(ClientID, "SORTEDTOP", (int)ToplistType::PLAYERS_LEVELING, NOPE, "Top 10 players leveling");
		GS()->AVM(ClientID, "SORTEDTOP", (int)ToplistType::PLAYERS_WEALTHY, NOPE, "Top 10 players wealthy");

		if(pPlayer->m_aSortTabs[SORT_TOP] >= 0)
		{
			GS()->AV(ClientID, "null", "\0");
			ShowTopList(ClientID, (ToplistType)pPlayer->m_aSortTabs[SORT_TOP], false, 10);
		}

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	// grinding guide
	if(Menulist == MENU_GUIDE_GRINDING)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		// information
		CVoteWrapper VGrindingInfo(ClientID, HIDE_DEFAULT_CLOSE, "Grinding Information");
		VGrindingInfo.Add("You can look mobs, plants, and ores.");
		CVoteWrapper::AddLine(ClientID);

		CVoteWrapper VDiscordInfo(ClientID, BORDER_STRICT_BOLD);
		VDiscordInfo.Add("Discord: \"{STR}\"", g_Config.m_SvDiscordInviteLink);
		VDiscordInfo.AddLine();

		// show all world's
		CVoteWrapper VGrindingSelect(ClientID, HIDE_DEFAULT_OPEN, "Select a zone to view information");
		for(int ID = MAIN_WORLD_ID; ID < GS()->Server()->GetWorldsSize(); ID++)
			VGrindingSelect.Add(MENU_GUIDE_GRINDING_SELECT, ID, "{STR}", GS()->Server()->GetWorldName(ID));

		// add back page
		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// grinding guide selected
	if(Menulist == MENU_GUIDE_GRINDING_SELECT)
	{
		pPlayer->m_LastVoteMenu = MENU_GUIDE_GRINDING;

		const int WorldID = pPlayer->m_TempMenuValue;
		CVoteWrapper VGrinding(ClientID, BORDER_STRICT_BOLD);
		VGrinding.Add("Selected zone: {STR}", GS()->Server()->GetWorldName(WorldID));
		VGrinding.AddLine();

		CVoteWrapper VGrindingMobs(ClientID, HIDE_DEFAULT_OPEN, "Zone guides");
		const bool ActiveMob = BotManager()->ShowGuideDropByWorld(WorldID, pPlayer);
		const bool ActivePlant = AccountPlantManager()->ShowGuideDropByWorld(WorldID, pPlayer);
		const bool ActiveOre = AccountMinerManager()->ShowGuideDropByWorld(WorldID, pPlayer);
		if(!ActiveMob && !ActivePlant && !ActiveOre)
			VGrindingMobs.Add("There are no drops in the selected area.");

		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// ----------------------------------------
	// check append votes
	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnHandleMenulist(pPlayer, Menulist, false))
			return true;
	}
	// ----------------------------------------

	return false;
}

bool CMmoController::OnPlayerHandleTile(CCharacter* pChr, int IndexCollision)
{
	if(!pChr || !pChr->IsAlive())
		return true;

	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnHandleTile(pChr, IndexCollision))
			return true;
	}
	return false;
}

bool CMmoController::OnParsingVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	if(!pPlayer)
		return true;

	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnHandleVoteCommands(pPlayer, CMD, VoteID, VoteID2, Get, GetText))
			return true;
	}
	return false;
}

void CMmoController::ResetClientData(int ClientID)
{
	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnResetClient(ClientID);
}

void CMmoController::HandleTimePeriod() const
{
	// Declare a current timestamp, byte array to store raw data
	ByteArray RawData {};
	time_t CurrentTimeStamp = time(nullptr);

	// Load the file "time_periods.cfg" and store the result in a variable
	Tools::Files::Result Result = Tools::Files::loadFile("time_periods.cfg", &RawData);
	if(Result == Tools::Files::Result::ERROR_FILE)
	{
		// Save the data to the file "time_periods.cfg"
		std::string Data = std::to_string(CurrentTimeStamp) + "\n" + std::to_string(CurrentTimeStamp) + "\n" + std::to_string(CurrentTimeStamp);
		Tools::Files::saveFile("time_periods.cfg", Data.data(), (unsigned)Data.size());
		return;
	}

	// Declare variables to store timestamps for daily, weekly, and monthly data
	time_t DailyStamp, WeekStamp, MonthStamp;
	std::string Data = std::string((char*)RawData.data(), RawData.size());
	std::sscanf(Data.c_str(), "%llu\n%llu\n%llu", &DailyStamp, &WeekStamp, &MonthStamp);

	// Set a flag indicating whether time periods have been updated
	std::vector<int> aPeriodsUpdated {};

	// Check if the current time is a new day
	if(time_is_new_day(DailyStamp, CurrentTimeStamp))
	{
		DailyStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(TIME_PERIOD::DAILY_STAMP);
	}

	// Check if the current time is a new week
	if(time_is_new_week(WeekStamp, CurrentTimeStamp))
	{
		WeekStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(TIME_PERIOD::WEEK_STAMP);
	}

	// Check if the current time is a new month
	if(time_is_new_month(MonthStamp, CurrentTimeStamp))
	{
		MonthStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(TIME_PERIOD::MONTH_STAMP);
	}

	// If any time period has been updated
	if(!aPeriodsUpdated.empty())
	{
		std::string Data = std::to_string(DailyStamp) + "\n" + std::to_string(WeekStamp) + "\n" + std::to_string(MonthStamp);
		Tools::Files::saveFile("time_periods.cfg", Data.data(), (unsigned)Data.size());

		// Check time periods for all components
		for(const auto& component : m_System.m_vComponents)
		{
			for(const auto& periods : aPeriodsUpdated)
			{
				component->OnHandleTimePeriod(TIME_PERIOD(periods));
			}
		}
	}
}

void CMmoController::HandlePlayerTimePeriod(CPlayer* pPlayer)
{
	// Set a flag indicating whether time periods have been updated
	std::vector<int> aPeriodsUpdated {};

	// Get the current time
	time_t CurrentTimeStamp = time(nullptr);

	// Check if it is a new day and update the daily time period if necessary
	if(time_is_new_day(pPlayer->Account()->m_Periods.m_DailyStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_DailyStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(TIME_PERIOD::DAILY_STAMP);
	}

	// Check if it is a new week and update the weekly time period if necessary
	if(time_is_new_week(pPlayer->Account()->m_Periods.m_WeekStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_WeekStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(TIME_PERIOD::WEEK_STAMP);
	}

	// Check if it is a new month and update the monthly time period if necessary
	if(time_is_new_month(pPlayer->Account()->m_Periods.m_MonthStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_MonthStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(TIME_PERIOD::MONTH_STAMP);
	}

	// If any time period has been updated
	if(!aPeriodsUpdated.empty())
	{
		// Save the account with the updated time periods
		SaveAccount(pPlayer, SAVE_TIME_PERIODS);

		// Check time periods for all components
		for(const auto& component : m_System.m_vComponents)
		{
			for(const auto& periods : aPeriodsUpdated)
			{
				component->OnPlayerHandleTimePeriod(pPlayer, TIME_PERIOD(periods));
			}
		}
	}
}

// saving account
void CMmoController::SaveAccount(CPlayer* pPlayer, int Table) const
{
	if(!pPlayer->IsAuthed())
		return;

	CAccountData* pAcc = pPlayer->Account();

	if(Table == SAVE_STATS)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Level = '%d', Exp = '%d' WHERE ID = '%d'", pAcc->GetLevel(), pAcc->GetExperience(), pAcc->GetID());
	}
	else if(Table == SAVE_UPGRADES)
	{
		dynamic_string Buffer;
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasDatabaseField())
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), ", %s = '%d' ", pAttribute->GetFieldName(), pAcc->m_aStats[ID]);
				Buffer.append_at(Buffer.length(), aBuf);
			}
		}

		Database->Execute<DB::UPDATE>("tw_accounts_data", "Upgrade = '%d' %s WHERE ID = '%d'", pAcc->m_Upgrade, Buffer.buffer(), pAcc->GetID());
		Buffer.clear();
	}
	else if(Table == SAVE_PLANT_DATA)
	{
		std::string Fields = pAcc->m_FarmingData.getUpdateField();
		Database->Execute<DB::UPDATE>("tw_accounts_farming", "%s WHERE UserID = '%d'", Fields.c_str(), pAcc->GetID());
	}
	else if(Table == SAVE_MINER_DATA)
	{
		std::string Fields = pAcc->m_MiningData.getUpdateField();
		Database->Execute<DB::UPDATE>("tw_accounts_mining", "%s WHERE UserID = '%d'", Fields.c_str(), pAcc->GetID());
	}
	else if(Table == SAVE_SOCIAL_STATUS)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Relations = '%d', PrisonSeconds = '%d', DailyChairGolds = '%d' WHERE ID = '%d'",
			pAcc->GetRelations(), pAcc->m_PrisonSeconds, pAcc->GetCurrentDailyChairGolds(), pAcc->GetID());
	}
	else if(Table == SAVE_GUILD_DATA)
	{
		//
	}
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = AccountManager()->GetHistoryLatestCorrectWorldID(pPlayer);
		Database->Execute<DB::UPDATE>("tw_accounts_data", "WorldID = '%d' WHERE ID = '%d'", LatestCorrectWorldID, pAcc->GetID());
	}
	else if(Table == SAVE_TIME_PERIODS)
	{
		time_t Daily = pAcc->m_Periods.m_DailyStamp;
		time_t Week = pAcc->m_Periods.m_WeekStamp;
		time_t Month = pAcc->m_Periods.m_MonthStamp;
		Database->Execute<DB::UPDATE>("tw_accounts_data", "DailyStamp = '%llu', WeekStamp = '%llu', MonthStamp = '%llu' WHERE ID = '%d'", Daily, Week, Month, pAcc->GetID());
	}
	else if(Table == SAVE_LANGUAGE)
	{
		Database->Execute<DB::UPDATE>("tw_accounts", "Language = '%s' WHERE ID = '%d'", pPlayer->GetLanguage(), pAcc->GetID());
	}
	else
	{
		Database->Execute<DB::UPDATE>("tw_accounts", "Username = '%s' WHERE ID = '%d'", pAcc->GetLogin(), pAcc->GetID());
	}
}

void CMmoController::LoadLogicWorld() const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_logics_worlds", "WHERE WorldID = '%d'", GS()->GetWorldID());
	while(pRes->next())
	{
		const int Type = pRes->getInt("MobID"), Mode = pRes->getInt("Mode"), Health = pRes->getInt("ParseInt");
		const vec2 Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		GS()->m_pController->CreateLogic(Type, Mode, Position, Health);
	}
}

void CMmoController::ShowLoadingProgress(const char* pLoading, int Size) const
{
	char aLoadingBuf[128];
	str_format(aLoadingBuf, sizeof(aLoadingBuf), "[Loaded %d %s] :: WorldID %d.", Size, pLoading, GS()->GetWorldID());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "LOAD DB", aLoadingBuf);
}

void CMmoController::ShowTopList(int ClientID, ToplistType Type, bool ChatGlobalMode, int Limit) const
{
	if(Type == ToplistType::GUILDS_LEVELING)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Level DESC, Experience DESC LIMIT %d", Limit);
		while(pRes->next())
		{
			char NameGuild[64];
			const int Rank = pRes->getRow();
			const int Level = pRes->getInt("Level");
			const int Experience = pRes->getInt("Experience");
			str_copy(NameGuild, pRes->getString("Name").c_str(), sizeof(NameGuild));

			if(ChatGlobalMode)
				GS()->Chat(-1, "{INT}. {STR} :: Level {INT} : Exp {INT}", Rank, NameGuild, Level, Experience);
			else
				GS()->AVL(ClientID, "null", "{INT}. {STR} :: Level {INT} : Exp {INT}", Rank, NameGuild, Level, Experience);
		}
	}
	else if(Type == ToplistType::GUILDS_WEALTHY)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Bank DESC LIMIT %d", Limit);
		while(pRes->next())
		{
			char NameGuild[64];
			const int Rank = pRes->getRow();
			const int Gold = pRes->getInt("Bank");
			str_copy(NameGuild, pRes->getString("Name").c_str(), sizeof(NameGuild));

			if(ChatGlobalMode)
				GS()->Chat(-1, "{INT}. {STR} :: Gold {VAL}", Rank, NameGuild, Gold);
			else
				GS()->AVL(ClientID, "null", "{INT}. {STR} :: Gold {VAL}", Rank, NameGuild, Gold);
		}
	}
	else if(Type == ToplistType::PLAYERS_LEVELING)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "ORDER BY Level DESC, Exp DESC LIMIT %d", Limit);
		while(pRes->next())
		{
			char Nick[64];
			const int Rank = pRes->getRow();
			const int Level = pRes->getInt("Level");
			const int Experience = pRes->getInt("Exp");
			str_copy(Nick, pRes->getString("Nick").c_str(), sizeof(Nick));

			if(ChatGlobalMode)
				GS()->Chat(-1, "{INT}. {STR} :: Level {INT} : Exp {INT}", Rank, Nick, Level, Experience);
			else
				GS()->AVL(ClientID, "null", "{INT}. {STR} :: Level {INT} : Exp {INT}", Rank, Nick, Level, Experience);

		}
	}
	else if(Type == ToplistType::PLAYERS_WEALTHY)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE ItemID = '%d' ORDER BY Value DESC LIMIT %d", (ItemIdentifier)itGold, Limit);
		while(pRes->next())
		{
			char Nick[64];
			const int Rank = pRes->getRow();
			const int Gold = pRes->getInt("Value");
			const int UserID = pRes->getInt("UserID");
			str_copy(Nick, Instance::GetServer()->GetAccountNickname(UserID), sizeof(Nick));

			if(ChatGlobalMode)
				GS()->Chat(-1, "{INT}. {STR} :: Gold {VAL}", Rank, Nick, Gold);
			else
				GS()->AVL(ClientID, "null", "{INT}. {STR} :: Gold {VAL}", Rank, Nick, Gold);
		}
	}
}

void CMmoController::AsyncClientEnterMsgInfo(const std::string ClientName, int ClientID)
{
	CSqlString<MAX_NAME_LENGTH> PlayerName(ClientName.c_str());

	// create new thread
	const auto AsyncEnterRes = Database->Prepare<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick = '%s'", PlayerName.cstr());
	AsyncEnterRes->AtExecute([PlayerName = std::string(PlayerName.cstr()), ClientID](ResultPtr pRes)
	{
		CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

		if(!pRes->next())
		{
			pGS->Chat(ClientID, "You need to register using /register <login> <pass>!");
			pGS->Chat(-1, "Apparently, we have a new player, {STR}!", PlayerName.c_str());
			return;
		}

		pGS->Chat(ClientID, "You need to log in using /login <user> <pass>!");
	});
}

// dump dialogs for translate
void CMmoController::ConAsyncLinesForTranslate()
{
	static std::mutex ms_mtxDump;

	// check thread last action
	if(!ms_mtxDump.try_lock())
	{
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Wait the last operation is in progress..");
		return;
	}

	// start new action
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Start of thread data collection for translation!");

	// lambda function for easy parse and hashing
	auto PushingDialogs = [](nlohmann::json& pJson, const char* pTextKey, const char* UniqueStart, int UniqueID)
	{
		if(pTextKey[0] == '\0')
			return;

		const std::hash<std::string> StrHash;
		const std::string HashingStr(UniqueStart + std::to_string(UniqueID));
		try
		{
			for(auto& pKeys : pJson["translation"])
			{
				if(!pKeys["key"].is_string() || !pKeys["value"].is_string())
					continue;

				if((pKeys.find("hash") != pKeys.end() && !pKeys["hash"].is_null()) && pKeys.value<size_t>("hash", 0) == StrHash(HashingStr))
				{
					if(StrHash(pKeys.value("key", "0")) != StrHash(pTextKey))
						pKeys["key"] = pKeys["value"] = pTextKey;
					return;
				}
				if(StrHash(pKeys.value("key", "0")) == StrHash(pTextKey))
				{
					pKeys["hash"] = StrHash(HashingStr);
					return;
				}
			}
			pJson["translation"].push_back({ { "key", pTextKey }, { "value", pTextKey }, { "hash", StrHash(HashingStr) } });
		}
		catch(nlohmann::json::exception& e)
		{
			dbg_msg("sync_lines", "%s", e.what());
		}
	};

	// update and sorted by translate
	char aDirLanguageFile[256];
	for(int i = 0; i < GS()->Server()->Localization()->m_pLanguages.size(); i++)
	{
		str_format(aDirLanguageFile, sizeof(aDirLanguageFile), "server_lang/%s.json", GS()->Server()->Localization()->m_pLanguages[i]->GetFilename());

		// Check if a file is successfully loaded using the specified directory and store its content in the RawData variable
		ByteArray RawData;
		if(!Tools::Files::loadFile(aDirLanguageFile, &RawData))
			continue;

		// insert database lines
		nlohmann::json JsonData = nlohmann::json::parse((char*)RawData.data());
		for(auto& pItem : QuestBotInfo::ms_aQuestBot)
		{
			int DialogNum = 0;
			std::string UniqueID("diaqu" + std::to_string(pItem.first));
			for(auto& pDialog : pItem.second.m_aDialogs)
				PushingDialogs(JsonData, pDialog.GetText(), UniqueID.c_str(), DialogNum++);
		}

		for(auto& pItem : NpcBotInfo::ms_aNpcBot)
		{
			int DialogNum = 0;
			std::string UniqueID("dianp" + std::to_string(pItem.first));
			for(auto& pDialog : pItem.second.m_aDialogs)
				PushingDialogs(JsonData, pDialog.GetText(), UniqueID.c_str(), DialogNum++);
		}

		for(auto& pAether : CAetherData::Data())
		{
			PushingDialogs(JsonData, pAether->GetName(), "aeth", pAether->GetID());
		}

		for(auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			PushingDialogs(JsonData, pAttribute->GetName(), "attb", (int)ID);
		}

		for(auto& pItem : CItemDescription::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "ittm", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetDescription(), "itdc", pItem.first);
		}

		for(auto& pItem : CSkillDescription::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "sknm", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetDescription(), "skds", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetBoostName(), "skbn", pItem.first);
		}

		for(auto& pItem : CQuestDescription::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "qudn", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetStory(), "qusn", pItem.first);
		}

		for(auto& pItem : CWarehouse::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "stnm", pItem.first);
		}

		for(auto& pItem : CHouseData::Data())
		{
			PushingDialogs(JsonData, pItem->GetClassName(), "hmnm", pItem->GetID());
		}

		// order non updated translated to up
		std::sort(JsonData["translation"].begin(), JsonData["translation"].end(), [](nlohmann::json& pA, nlohmann::json& pB)
		{ return pA["key"] == pA["value"] && pB["key"] != pB["value"]; });

		// save file
		std::string Data = JsonData.dump(4);
		Tools::Files::saveFile(aDirLanguageFile, (void*)Data.data(), Data.size());
	}

	// end transaction
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Completed successfully!");
	ms_mtxDump.unlock();
}