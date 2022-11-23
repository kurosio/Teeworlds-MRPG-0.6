/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "MmoController.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

#include "Components/Accounts/AccountCore.h"
#include "Components/Accounts/AccountMinerCore.h"
#include "Components/Accounts/AccountPlantCore.h"
#include "Components/Auction/AuctionCore.h"
#include "Components/Aethers/AetherCore.h"
#include "Components/Bots/BotCore.h"
#include "Components/Crafts/CraftCore.h"
#include "Components/Dungeons/DungeonCore.h"
#include "Components/Guilds/GuildCore.h"
#include "Components/Houses/HouseCore.h"
#include "Components/Inventory/InventoryCore.h"
#include "Components/Mails/MailBoxCore.h"
#include "Components/Quests/QuestCore.h"
#include "Components/Skills/SkillsCore.h"
#include "Components/Warehouse/WarehouseCore.h"
#include "Components/Worlds/WorldCore.h"


MmoController::MmoController(CGS *pGameServer) : m_pGameServer(pGameServer)
{
	// order
	m_Components.add(m_pBotsInfo = new CBotCore());
	m_Components.add(m_pItemWork = new CInventoryCore());
	m_Components.add(m_pCraftJob = new CCraftCore());
	m_Components.add(m_pWarehouse = new CWarehouseCore());
	m_Components.add(new CAuctionCore());
	m_Components.add(m_pQuest = new QuestCore());
	m_Components.add(m_pDungeonJob = new DungeonCore());
	m_Components.add(new CAetherCore());
	m_Components.add(m_pWorldSwapJob = new CWorldDataCore());
	m_Components.add(m_pHouseJob = new CHouseCore());
	m_Components.add(m_pGuildJob = new GuildCore());
	m_Components.add(m_pSkillJob = new CSkillsCore());
	m_Components.add(m_pAccMain = new CAccountCore());
	m_Components.add(m_pAccMiner = new CAccountMinerCore());
	m_Components.add(m_pAccPlant = new CAccountPlantCore());
	m_Components.add(m_pMailBoxJob = new CMailBoxCore());

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

void MmoController::OnInitAccount(int ClientID)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	for(auto& pComponent : m_Components.m_paComponents)
		pComponent->OnInitAccount(pPlayer);
}

bool MmoController::OnPlayerHandleMainMenu(int ClientID, int Menulist)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return true;

	// ----------------------------------------
	// check replaced votes
	for(auto& pComponent : m_Components.m_paComponents)
	{
		if(pComponent->OnHandleMenulist(pPlayer, Menulist, true))
		{
			GS()->AVL(ClientID, "null", "The main menu will return as soon as you leave this zone!");
			return true;
		}
	}

	// ----------------------------------------

	// main menu
	if(Menulist == MENU_MAIN)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		// statistics menu
		const int ExpForLevel = pPlayer->ExpNeed(pPlayer->Acc().m_Level);
		GS()->AVH(ClientID, TAB_STAT, "Hi, {STR} Last log in {STR}", GS()->Server()->ClientName(ClientID), pPlayer->Acc().m_aLastLogin);
		GS()->AVM(ClientID, "null", NOPE, TAB_STAT, "Level {INT} : Exp {INT}/{INT}", pPlayer->Acc().m_Level, pPlayer->Acc().m_Exp, ExpForLevel);
		GS()->AVM(ClientID, "null", NOPE, TAB_STAT, "Skill Point {INT}SP", pPlayer->GetItem(itSkillPoint)->GetValue());
		GS()->AVM(ClientID, "null", NOPE, TAB_STAT, "Gold: {VAL}", pPlayer->GetItem(itGold)->GetValue());
		GS()->AV(ClientID, "null");

		// personal menu
		GS()->AVH(ClientID, TAB_PERSONAL, "☪ SUB MENU PERSONAL");
		GS()->AVM(ClientID, "MENU", MENU_INVENTORY, TAB_PERSONAL, "Inventory");
		GS()->AVM(ClientID, "MENU", MENU_EQUIPMENT, TAB_PERSONAL, "Equipment");
		GS()->AVM(ClientID, "MENU", MENU_UPGRADES, TAB_PERSONAL, "Upgrades({INT}p)", pPlayer->Acc().m_Upgrade);
		GS()->AVM(ClientID, "MENU", MENU_DUNGEONS, TAB_PERSONAL, "Dungeons");
		GS()->AVM(ClientID, "MENU", MENU_SETTINGS, TAB_PERSONAL, "Settings");
		GS()->AVM(ClientID, "MENU", MENU_INBOX, TAB_PERSONAL, "Mailbox");
		GS()->AVM(ClientID, "MENU", MENU_JOURNAL_MAIN, TAB_PERSONAL, "Journal");
		if(GS()->Mmo()->House()->PlayerHouseID(pPlayer) > 0)
			GS()->AVM(ClientID, "MENU", MENU_HOUSE, TAB_PERSONAL, "House");
		GS()->AVM(ClientID, "MENU", MENU_GUILD_FINDER, TAB_PERSONAL, "Guild finder");
		if(pPlayer->Acc().IsGuild())
			GS()->AVM(ClientID, "MENU", MENU_GUILD, TAB_PERSONAL, "Guild");
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
				if(pAttribute->IsType(Type) && pAttribute->HasField())
					GS()->AVD(ClientID, "UPGRADE", (int)ID, pAttribute->GetUpgradePrice(), HiddenID, "{STR} {INT}P (Price {INT}P)", 
						pAttribute->GetName(), pPlayer->Acc().m_aStats[ID], pAttribute->GetUpgradePrice());
			}
		};

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

		GS()->AVM(ClientID, "SORTEDTOP", ToplistTypes::GUILDS_LEVELING, NOPE, "Top 10 guilds leveling");
		GS()->AVM(ClientID, "SORTEDTOP", ToplistTypes::GUILDS_WEALTHY, NOPE, "Top 10 guilds wealthy");
		GS()->AVM(ClientID, "SORTEDTOP", ToplistTypes::PLAYERS_LEVELING, NOPE, "Top 10 players leveling");
		GS()->AVM(ClientID, "SORTEDTOP", ToplistTypes::PLAYERS_WEALTHY, NOPE, "Top 10 players wealthy");

		if(pPlayer->m_aSortTabs[SORT_TOP] >= 0)
		{
			GS()->AV(ClientID, "null", "\0");
			ShowTopList(pPlayer, pPlayer->m_aSortTabs[SORT_TOP]);
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

bool MmoController::OnPlayerHandleTile(CCharacter *pChr, int IndexCollision)
{
	if(!pChr || !pChr->IsAlive())
		return true;

	for(auto & pComponent : m_Components.m_paComponents)
	{
		if(pComponent->OnHandleTile(pChr, IndexCollision))
			return true;
	}
	return false;
}

bool MmoController::OnParsingVoteCommands(CPlayer *pPlayer, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *GetText)
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
	for (auto& pComponent : m_Components.m_paComponents)
		pComponent->OnResetClient(ClientID);
}

// saving account
void MmoController::SaveAccount(CPlayer *pPlayer, int Table) const
{
	if(!pPlayer->IsAuthed())
		return;

	if(Table == SAVE_STATS)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Level = '%d', Exp = '%d' WHERE ID = '%d'", pPlayer->Acc().m_Level, pPlayer->Acc().m_Exp, pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_UPGRADES)
	{
		dynamic_string Buffer;
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasField())
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), ", %s = '%d' ", pAttribute->GetFieldName(), pPlayer->Acc().m_aStats[ID]);
				Buffer.append_at(Buffer.length(), aBuf);
			}
		}

		Database->Execute<DB::UPDATE>("tw_accounts_data", "Upgrade = '%d' %s WHERE ID = '%d'", pPlayer->Acc().m_Upgrade, Buffer.buffer(), pPlayer->Acc().m_UserID);
		Buffer.clear();
	}
	else if(Table == SAVE_PLANT_DATA)
	{
		std::string Fields = pPlayer->Acc().m_FarmingData.getUpdateField();
		Database->Execute<DB::UPDATE>("tw_accounts_farming", "%s WHERE UserID = '%d'", Fields.c_str(), pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_MINER_DATA)
	{
		std::string Fields = pPlayer->Acc().m_MiningData.getUpdateField();
		Database->Execute<DB::UPDATE>("tw_accounts_mining", "%s WHERE UserID = '%d'", Fields.c_str(), pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_GUILD_DATA)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "GuildID = '%d', GuildRank = '%d' WHERE ID = '%d'", pPlayer->Acc().m_GuildID, pPlayer->Acc().m_GuildRank, pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = Account()->GetHistoryLatestCorrectWorldID(pPlayer);
		Database->Execute<DB::UPDATE>("tw_accounts_data", "WorldID = '%d' WHERE ID = '%d'", LatestCorrectWorldID, pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_LANGUAGE)
	{
		Database->Execute<DB::UPDATE>("tw_accounts", "Language = '%s' WHERE ID = '%d'", pPlayer->GetLanguage(), pPlayer->Acc().m_UserID);
	}
	else
	{
		Database->Execute<DB::UPDATE>("tw_accounts", "Username = '%s' WHERE ID = '%d'", pPlayer->Acc().m_aLogin, pPlayer->Acc().m_UserID);
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

char SaveNick[32];
const char* MmoController::PlayerName(int AccountID)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("Nick", "tw_accounts_data", "WHERE ID = '%d'", AccountID);
	if(pRes->next())
	{
		str_copy(SaveNick, pRes->getString("Nick").c_str(), sizeof(SaveNick));
		return SaveNick;
	}
	return "No found!";
}

void MmoController::ShowLoadingProgress(const char* pLoading, int Size) const
{
	char aLoadingBuf[128];
	str_format(aLoadingBuf, sizeof(aLoadingBuf), "[Loaded %d %s] :: WorldID %d.", Size, pLoading, GS()->GetWorldID());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "LOAD DB", aLoadingBuf);
}

void MmoController::ShowTopList(CPlayer* pPlayer, int TypeID) const
{
	const int ClientID = pPlayer->GetCID();
	if(TypeID == GUILDS_LEVELING)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Level DESC, Experience DESC LIMIT 10");
		while (pRes->next())
		{
			char NameGuild[64];
			const int Rank = pRes->getRow();
			const int Level = pRes->getInt("Level");
			const int Experience = pRes->getInt("Experience");
			str_copy(NameGuild, pRes->getString("Name").c_str(), sizeof(NameGuild));
			GS()->AVL(ClientID, "null", "{INT}. {STR} :: Level {INT} : Exp {INT}", Rank, NameGuild, Level, Experience);
		}
	}
	else if (TypeID == GUILDS_WEALTHY)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Bank DESC LIMIT 10");
		while (pRes->next())
		{
			char NameGuild[64];
			const int Rank = pRes->getRow();
			const int Gold = pRes->getInt("Bank");
			str_copy(NameGuild, pRes->getString("Name").c_str(), sizeof(NameGuild));
			GS()->AVL(ClientID, "null", "{INT}. {STR} :: Gold {VAL}", Rank, NameGuild, Gold);
		}
	}
	else if (TypeID == PLAYERS_LEVELING)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "ORDER BY Level DESC, Exp DESC LIMIT 10");
		while (pRes->next())
		{
			char Nick[64];
			const int Rank = pRes->getRow();
			const int Level = pRes->getInt("Level");
			const int Experience = pRes->getInt("Exp");
			str_copy(Nick, pRes->getString("Nick").c_str(), sizeof(Nick));
			GS()->AVL(ClientID, "null", "{INT}. {STR} :: Level {INT} : Exp {INT}", Rank, Nick, Level, Experience);
		}
	}
	else if (TypeID == PLAYERS_WEALTHY)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE ItemID = '%d' ORDER BY Value DESC LIMIT 10", (ItemIdentifier)itGold);
		while (pRes->next())
		{
			char Nick[64];
			const int Rank = pRes->getRow();
			const int Gold = pRes->getInt("Value");
			const int UserID = pRes->getInt("UserID");
			str_copy(Nick, PlayerName(UserID), sizeof(Nick));
			GS()->AVL(ClientID, "null", "{INT}. {STR} :: Gold {VAL}", Rank, Nick, Gold);
		}
	}
}

void MmoController::AsyncClientEnterMsgInfo(std::string ClientName, int ClientID)
{
	CSqlString<MAX_NAME_LENGTH> PlayerName(ClientName.c_str());

	// create new thread
	const auto AsyncEnterRes = Database->Prepare<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick = '%s'", PlayerName.cstr());
	AsyncEnterRes->AtExecute([PlayerName, ClientID](ResultPtr pRes)
	{
		CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

		// send information : CPlayer checked by Chat() : PlayerName and ClientID getter by copy. 
		pGS->Chat(ClientID, "Welcome! A list of commands can be found using /cmdlist.");

		if(!pRes->next())
		{
			pGS->Chat(ClientID, "You need to register using /register <login> <pass>.", PlayerName.cstr());
			pGS->Chat(-1, "Apparently we have a new player {STR}!", PlayerName.cstr());
			return;
		}

		pGS->Chat(ClientID, "You need to login using /login <user> <pass>.", PlayerName.cstr());
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
			pJson["translation"].push_back({ { "key", pTextKey }, { "value", pTextKey }, { "hash", StrHash(HashingStr) }});
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
		IOHANDLE File = io_open(aDirLanguageFile, IOFLAG_READ);
		if(!File)
			continue;

		const int FileSize = (int)io_length(File) + 1;
		char* pFileData = (char*)malloc(FileSize);
		mem_zero(pFileData, FileSize);
		io_read(File, pFileData, FileSize);

		// close and clear
		nlohmann::json JsonData = nlohmann::json::parse(pFileData);
		mem_free(pFileData);
		io_close(File);

		// insert database lines
		for(auto& pItem : QuestBotInfo::ms_aQuestBot)
		{
			int DialogNum = 0;
			std::string UniqueID("diaqu" + std::to_string(pItem.first));
			for(auto& pDialog : pItem.second.m_aDialogs)
				for(auto& pVariant : pDialog.GetArrayText())
					PushingDialogs(JsonData, pVariant.m_Text.c_str(), UniqueID.c_str(), DialogNum++);
		}

		for(auto& pItem : NpcBotInfo::ms_aNpcBot)
		{
			int DialogNum = 0;
			std::string UniqueID("dianp" + std::to_string(pItem.first));
			for(auto& pDialog : pItem.second.m_aDialogs)
				for(auto& pVariant : pDialog.GetArrayText())
					PushingDialogs(JsonData, pVariant.m_Text.c_str(), UniqueID.c_str(), DialogNum++);
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

		for(auto& pItem : CQuestDataInfo::ms_aDataQuests)
		{
			PushingDialogs(JsonData, pItem.second.m_aName, "qudn", pItem.first);
			PushingDialogs(JsonData, pItem.second.m_aStoryLine, "qusn", pItem.first);
		}

		for(auto& pItem : CWarehouse::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "stnm", pItem.first);
		}

		for(auto& pItem : CHouseData::ms_aHouse)
		{
			PushingDialogs(JsonData, pItem.second.m_aClass, "hmnm", pItem.first);
		}

		// order non updated translated to up
		std::sort(JsonData["translation"].begin(), JsonData["translation"].end(), [](nlohmann::json& pA, nlohmann::json& pB) 
		{ return pA["key"] == pA["value"] && pB["key"] != pB["value"]; });

		// save file
		File = io_open(aDirLanguageFile, IOFLAG_WRITE);
		if(!File)
			continue;

		std::string Data = JsonData.dump(4);
		io_write(File, Data.c_str(), Data.length());
		io_close(File);
	}

	// end transaction
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Completed successfully!");
	ms_mtxDump.unlock();
}