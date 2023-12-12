/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "MmoController.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include "Components/Accounts/AccountManager.h"
#include "Components/Accounts/AccountMinerManager.h"
#include "Components/Accounts/AccountPlantManager.h"
#include "Components/Auction/AuctionManager.h"
#include "Components/Aethers/AetherManager.h"
#include "Components/Bots/BotManager.h"
#include "Components/Crafts/CraftManager.h"
#include "Components/Dungeons/DungeonManager.h"
#include "Components/Eidolons/EidolonManager.h"
#include "Components/Groups/GroupManager.h"
#include "Components/Guilds/GuildManager.h"
#include "Components/Houses/HouseManager.h"
#include "Components/Inventory/InventoryManager.h"
#include "Components/Mails/MailBoxManager.h"
#include "Components/Quests/QuestManager.h"
#include "Components/Skills/SkillManager.h"
#include "Components/Tutorial/TutorialManager.h"
#include "Components/Warehouse/WarehouseManager.h"
#include "Components/Worlds/WorldManager.h"


MmoController::MmoController(CGS* pGameServer) : m_pGameServer(pGameServer)
{
	// order
	m_Components.add(m_pQuest = new CQuestManager);
	m_Components.add(m_pBotsInfo = new CBotManager);
	m_Components.add(m_pItemWork = new CInventoryManager);
	m_Components.add(m_pCraft = new CCraftManager);
	m_Components.add(m_pWarehouse = new CWarehouseManager);
	m_Components.add(new CAuctionManager);
	m_Components.add(m_pEidolon = new CEidolonManager);
	m_Components.add(m_pDungeon = new CDungeonManager);
	m_Components.add(new CAetherManager);
	m_Components.add(m_pWorldSwap = new CWorldManager);
	m_Components.add(m_pHouse = new CHouseManager);
	m_Components.add(m_pGuild = new CGuildManager);
	m_Components.add(m_pGroup = new CGroupManager);
	m_Components.add(m_pSkill = new CSkillManager);
	m_Components.add(m_pTutorial = new CTutorialManager);
	m_Components.add(m_pAccMain = new CAccountManager);
	m_Components.add(m_pAccMiner = new CAccountMinerManager);
	m_Components.add(m_pAccPlant = new CAccountPlantManager);
	m_Components.add(m_pMailBox = new CMailBoxManager);

	for(auto& pComponent : m_Components.m_paComponents)
	{
		pComponent->m_Job = this;
		pComponent->m_GameServer = pGameServer;
		pComponent->m_pServer = pGameServer->Server();

		if(m_pGameServer->GetWorldID() == MAIN_WORLD_ID)
			pComponent->OnInit();

		char aLocalSelect[64];
		str_format(aLocalSelect, sizeof(aLocalSelect), "WHERE WorldID = '%d'", m_pGameServer->GetWorldID());
		pComponent->OnInitWorld(aLocalSelect);
	}
}

MmoController::~MmoController()
{
	m_Components.free();
}

void MmoController::OnTick()
{
	for(auto& pComponent : m_Components.m_paComponents)
		pComponent->OnTick();
}

bool MmoController::OnMessage(int MsgID, void* pRawMsg, int ClientID)
{
	if(GS()->Server()->ClientIngame(ClientID) && GS()->GetPlayer(ClientID))
	{
		for(auto& pComponent : m_Components.m_paComponents)
		{
			if(pComponent->OnMessage(MsgID, pRawMsg, ClientID))
				return true;
		}
	}

	return false;
}

void MmoController::OnInitAccount(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	for(auto& pComponent : m_Components.m_paComponents)
		pComponent->OnInitAccount(pPlayer);
}

bool MmoController::OnPlayerHandleMainMenu(int ClientID, int Menulist)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return true;

	// ----------------------------------------
	// check replaced votes
	for(auto& pComponent : m_Components.m_paComponents)
	{
		// Check if the player's m_ZoneInvertMenu variable is true
		if(pPlayer->m_ZoneInvertMenu)
		{
			GS()->AVL(ClientID, "ZONE_INVERT_MENU", "▶▶▶▶ Back to zone menu. ▶▶▶▶");
			GS()->AV(ClientID, "null");
			break;
		}

		// Call the function OnHandleMenulist of the pComponent object with the parameters pPlayer and Menulist
		if(pComponent->OnHandleMenulist(pPlayer, Menulist, true))
		{
			// Display a notification to the client with the message "<<<< Back to player menu. <<<<"
			GS()->AV(ClientID, "null");
			GS()->AVL(ClientID, "ZONE_INVERT_MENU", "◀◀◀◀ Back to player menu. ◀◀◀◀");
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
		GS()->AVH(ClientID, TAB_STAT, "Hi, {STR} Last log in {STR}", GS()->Server()->ClientName(ClientID), pPlayer->Account()->GetLastLoginDate());
		GS()->AVM(ClientID, "null", NOPE, TAB_STAT, "Level {INT} : Exp {INT}/{INT}", pPlayer->Account()->GetLevel(), pPlayer->Account()->GetExperience(), ExpForLevel);
		GS()->AVM(ClientID, "null", NOPE, TAB_STAT, "Skill Point {INT}SP", pPlayer->GetItem(itSkillPoint)->GetValue());
		GS()->AVM(ClientID, "null", NOPE, TAB_STAT, "Gold: {VAL}", pPlayer->GetItem(itGold)->GetValue());
		GS()->AV(ClientID, "null");

		// personal menu
		GS()->AVH(ClientID, TAB_PERSONAL, "☪ SUB MENU PERSONAL");
		GS()->AVM(ClientID, "MENU", MENU_INVENTORY, TAB_PERSONAL, "Inventory");
		GS()->AVM(ClientID, "MENU", MENU_EQUIPMENT, TAB_PERSONAL, "Equipment");
		GS()->AVM(ClientID, "MENU", MENU_UPGRADES, TAB_PERSONAL, "Upgrades({INT}p)", pPlayer->Account()->m_Upgrade);
		GS()->AVM(ClientID, "MENU", MENU_EIDOLON_COLLECTION, TAB_PERSONAL, "Eidolon Collection");
		GS()->AVM(ClientID, "MENU", MENU_DUNGEONS, TAB_PERSONAL, "Dungeons");
		GS()->AVM(ClientID, "MENU", MENU_GROUP, TAB_PERSONAL, "Group");
		GS()->AVM(ClientID, "MENU", MENU_SETTINGS, TAB_PERSONAL, "Settings");
		GS()->AVM(ClientID, "MENU", MENU_INBOX, TAB_PERSONAL, "Mailbox");
		GS()->AVM(ClientID, "MENU", MENU_JOURNAL_MAIN, TAB_PERSONAL, "Journal");
		if(pPlayer->Account()->HasHouse())
		{
			GS()->AVM(ClientID, "MENU", MENU_HOUSE, TAB_PERSONAL, "House");
		}

		GS()->AVM(ClientID, "MENU", MENU_GUILD_FINDER, TAB_PERSONAL, "Guild finder");
		if(pPlayer->Account()->IsGuild())
		{
			GS()->AVM(ClientID, "MENU", MENU_GUILD, TAB_PERSONAL, "Guild");
		}
		GS()->AV(ClientID, "null");

		// info menu
		GS()->AVH(ClientID, TAB_INFORMATION, "√ SUB MENU INFORMATION");
		GS()->AVM(ClientID, "MENU", MENU_GUIDE_GRINDING, TAB_INFORMATION, "Wiki / Grinding Guide ");
		GS()->AVM(ClientID, "MENU", MENU_TOP_LIST, TAB_INFORMATION, "Ranking guilds and players");
		return true;
	}

	// player upgrades
	if(Menulist == MENU_UPGRADES)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		GS()->AVH(ClientID, TAB_INFO_UPGR, "Upgrades Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_UPGR, "Select upgrades type in Reason, write count.");
		GS()->AV(ClientID, "null");

		// show player stats
		GS()->ShowVotesPlayerStats(pPlayer);

		// lambda function for easy use
		auto ShowAttributeVote = [&](int HiddenID, AttributeType Type, std::function<void(int HiddenID)> pFunc)
		{
			pFunc(HiddenID);
			for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
			{
				if(pAttribute->IsType(Type) && pAttribute->HasDatabaseField())
					GS()->AVD(ClientID, "UPGRADE", (int)ID, pAttribute->GetUpgradePrice(), HiddenID, "{STR} {INT}P (Price {INT}P)",
						pAttribute->GetName(), pPlayer->Account()->m_aStats[ID], pAttribute->GetUpgradePrice());
			}
		};
		GS()->AV(ClientID, "null");

		// Disciple of War
		ShowAttributeVote(TAB_UPGR_DPS, AttributeType::Dps, [&](int HiddenID)
		{
			const int Range = pPlayer->GetTypeAttributesSize(AttributeType::Dps);
			GS()->AVH(ClientID, HiddenID, "Disciple of War. Level Power {INT}", Range);
		});
		GS()->AV(ClientID, "null");

		// Disciple of Tank
		ShowAttributeVote(TAB_UPGR_TANK, AttributeType::Tank, [&](int HiddenID)
		{
			const int Range = pPlayer->GetTypeAttributesSize(AttributeType::Tank);
			GS()->AVH(ClientID, HiddenID, "Disciple of Tank. Level Power {INT}", Range);
		});
		GS()->AV(ClientID, "null");

		// Disciple of Healer
		ShowAttributeVote(TAB_UPGR_HEALER, AttributeType::Healer, [&](int HiddenID)
		{
			const int Range = pPlayer->GetTypeAttributesSize(AttributeType::Healer);
			GS()->AVH(ClientID, HiddenID, "Disciple of Healer. Level Power {INT}", Range);
		});
		GS()->AV(ClientID, "null");

		// Upgrades Weapons and ammo
		ShowAttributeVote(TAB_UPGR_WEAPON, AttributeType::Weapon, [&](int HiddenID)
		{
			GS()->AVH(ClientID, HiddenID, "Upgrades Weapons / Ammo");
		});

		GS()->AddVotesBackpage(ClientID);
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

		GS()->AVH(ClientID, TAB_INFO_LOOT, "Grinding Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_LOOT, "You can look mobs, plants, and ores.");
		GS()->AV(ClientID, "null");
		GS()->AVL(ClientID, "null", "Discord: \"{STR}\"", g_Config.m_SvDiscordInviteLink);
		GS()->AV(ClientID, "null");

		// show all world's
		for(int ID = MAIN_WORLD_ID; ID < GS()->Server()->GetWorldsSize(); ID++)
		{
			GS()->AVM(ClientID, "SORTEDWIKIWORLD", ID, NOPE, GS()->Server()->GetWorldName(ID));
		}
		GS()->AV(ClientID, "null");

		// selected zone
		if(pPlayer->m_aSortTabs[SORT_GUIDE_WORLD] < 0)
		{
			GS()->AVL(ClientID, "null", "Select a zone to view information");
		}
		else
		{
			const int WorldID = pPlayer->m_aSortTabs[SORT_GUIDE_WORLD];
			const bool ActiveMob = BotsData()->ShowGuideDropByWorld(WorldID, pPlayer);
			const bool ActivePlant = PlantsAcc()->ShowGuideDropByWorld(WorldID, pPlayer);
			const bool ActiveOre = MinerAcc()->ShowGuideDropByWorld(WorldID, pPlayer);
			if(!ActiveMob && !ActivePlant && !ActiveOre)
				GS()->AVL(ClientID, "null", "There are no drops in the selected area.");
		}

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	// ----------------------------------------
	// check append votes
	for(auto& pComponent : m_Components.m_paComponents)
	{
		if(pComponent->OnHandleMenulist(pPlayer, Menulist, false))
			return true;
	}
	// ----------------------------------------

	return false;
}

bool MmoController::OnPlayerHandleTile(CCharacter* pChr, int IndexCollision)
{
	if(!pChr || !pChr->IsAlive())
		return true;

	for(auto& pComponent : m_Components.m_paComponents)
	{
		if(pComponent->OnHandleTile(pChr, IndexCollision))
			return true;
	}
	return false;
}

bool MmoController::OnParsingVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	if(!pPlayer)
		return true;

	for(auto& pComponent : m_Components.m_paComponents)
	{
		if(pComponent->OnHandleVoteCommands(pPlayer, CMD, VoteID, VoteID2, Get, GetText))
			return true;
	}
	return false;
}

void MmoController::ResetClientData(int ClientID)
{
	for(auto& pComponent : m_Components.m_paComponents)
		pComponent->OnResetClient(ClientID);
}

void MmoController::HandlePlayerTimePeriod(CPlayer* pPlayer)
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
		for(const auto& component : m_Components.m_paComponents)
		{
			for(const auto& periods : aPeriodsUpdated)
			{
				component->OnPlayerHandleTimePeriod(pPlayer, TIME_PERIOD(periods));
			}
		}
	}
}

// saving account
void MmoController::SaveAccount(CPlayer* pPlayer, int Table) const
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
		Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildID = '%d', GuildRank = '%d' WHERE ID = '%d'", pAcc->m_GuildID, pAcc->m_GuildRank, pAcc->GetID());
	}
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = Account()->GetHistoryLatestCorrectWorldID(pPlayer);
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

void MmoController::LoadLogicWorld() const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_logics_worlds", "WHERE WorldID = '%d'", GS()->GetWorldID());
	while(pRes->next())
	{
		const int Type = pRes->getInt("MobID"), Mode = pRes->getInt("Mode"), Health = pRes->getInt("ParseInt");
		const vec2 Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		GS()->m_pController->CreateLogic(Type, Mode, Position, Health);
	}
}

void MmoController::ShowLoadingProgress(const char* pLoading, int Size) const
{
	char aLoadingBuf[128];
	str_format(aLoadingBuf, sizeof(aLoadingBuf), "[Loaded %d %s] :: WorldID %d.", Size, pLoading, GS()->GetWorldID());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "LOAD DB", aLoadingBuf);
}

void MmoController::ShowTopList(int ClientID, ToplistType Type, bool ChatGlobalMode, int Limit) const
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

void MmoController::AsyncClientEnterMsgInfo(const std::string ClientName, int ClientID)
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
void MmoController::ConAsyncLinesForTranslate()
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

		for(auto& [ID, Aether] : CAether::Data())
		{
			PushingDialogs(JsonData, Aether.GetName(), "aeth", ID);
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