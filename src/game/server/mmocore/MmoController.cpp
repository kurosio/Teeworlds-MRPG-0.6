/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "MmoController.h"

#include <engine/shared/datafile.h>
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
#include "Components/Worlds/WorldSwapCore.h"

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
	m_Components.add(m_pWorldSwapJob = new CWorldSwapCore());
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

bool MmoController::OnPlayerHandleMainMenu(int ClientID, int Menulist, bool ReplaceMenu)
{
	CPlayer *pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return true;

	for(auto& pComponent : m_Components.m_paComponents)
	{
		if(pComponent->OnHandleMenulist(pPlayer, Menulist, ReplaceMenu))
			return true;
	}
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
		Sqlpool.Execute<DB::UPDATE>("tw_accounts_data", "Level = '%d', Exp = '%d' WHERE ID = '%d'", pPlayer->Acc().m_Level, pPlayer->Acc().m_Exp, pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_UPGRADES)
	{
		dynamic_string Buffer;
		for(const auto& [ID, Attribute] : CAttributeDescription::Data())
		{
			if(Attribute.HasField())
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), ", %s = '%d' ", Attribute.GetFieldName(), pPlayer->Acc().m_aStats[ID]);
				Buffer.append_at(Buffer.length(), aBuf);
			}
		}

		Sqlpool.Execute<DB::UPDATE>("tw_accounts_data", "Upgrade = '%d' %s WHERE ID = '%d'", pPlayer->Acc().m_Upgrade, Buffer.buffer(), pPlayer->Acc().m_UserID);
		Buffer.clear();
	}
	else if(Table == SAVE_PLANT_DATA)
	{
		char aBuf[64];
		dynamic_string Buffer;
		for(int i = 0; i < NUM_JOB_ACCOUNTS_STATS; i++)
		{
			const char *pFieldName = pPlayer->Acc().m_aFarming[i].getFieldName();
			const int JobValue = pPlayer->Acc().m_aFarming[i].m_Value;
			str_format(aBuf, sizeof(aBuf), "%s = '%d' %s", pFieldName, JobValue, (i == NUM_JOB_ACCOUNTS_STATS-1 ? "" : ", "));
			Buffer.append_at(Buffer.length(), aBuf);
		}

		Sqlpool.Execute<DB::UPDATE>("tw_accounts_farming", "%s WHERE UserID = '%d'", Buffer.buffer(), pPlayer->Acc().m_UserID);
		Buffer.clear();
	}
	else if(Table == SAVE_MINER_DATA)
	{
		char aBuf[64];
		dynamic_string Buffer;
		for(int i = 0; i < NUM_JOB_ACCOUNTS_STATS; i++)
		{
			const char* pFieldName = pPlayer->Acc().m_aMining[i].getFieldName();
			const int JobValue = pPlayer->Acc().m_aMining[i].m_Value;
			str_format(aBuf, sizeof(aBuf), "%s = '%d' %s", pFieldName, JobValue, (i == NUM_JOB_ACCOUNTS_STATS-1 ? "" : ", "));
			Buffer.append_at(Buffer.length(), aBuf);
		}

		Sqlpool.Execute<DB::UPDATE>("tw_accounts_mining", "%s WHERE UserID = '%d'", Buffer.buffer(), pPlayer->Acc().m_UserID);
		Buffer.clear();
	}
	else if(Table == SAVE_GUILD_DATA)
	{
		Sqlpool.Execute<DB::UPDATE>("tw_accounts_data", "GuildID = '%d', GuildRank = '%d' WHERE ID = '%d'", pPlayer->Acc().m_GuildID, pPlayer->Acc().m_GuildRank, pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = Account()->GetHistoryLatestCorrectWorldID(pPlayer);
		Sqlpool.Execute<DB::UPDATE>("tw_accounts_data", "WorldID = '%d' WHERE ID = '%d'", LatestCorrectWorldID, pPlayer->Acc().m_UserID);
	}
	else if(Table == SAVE_LANGUAGE)
	{
		Sqlpool.Execute<DB::UPDATE>("tw_accounts", "Language = '%s' WHERE ID = '%d'", pPlayer->GetLanguage(), pPlayer->Acc().m_UserID);
	}
	else
	{
		Sqlpool.Execute<DB::UPDATE>("tw_accounts", "Username = '%s' WHERE ID = '%d'", pPlayer->Acc().m_aLogin, pPlayer->Acc().m_UserID);
	}
}

void MmoController::LoadLogicWorld() const
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_logics_worlds", "WHERE WorldID = '%d'", GS()->GetWorldID());
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
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("Nick", "tw_accounts_data", "WHERE ID = '%d'", AccountID);
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
		ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Level DESC, Experience DESC LIMIT 10");
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
		ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Bank DESC LIMIT 10");
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
		ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_accounts_data", "ORDER BY Level DESC, Exp DESC LIMIT 10");
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
		ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE ItemID = '%d' ORDER BY Value DESC LIMIT 10", (ItemIdentifier)itGold);
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

// dump dialogs for translate
void MmoController::ConSyncLinesForTranslate()
{
	static std::mutex ms_MutexDump;
	if(!ms_MutexDump.try_lock())
	{
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Wait the last operation is in progress..");
		return;
	}
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Start of thread data collection for translation!");

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
			for(auto& pDialog : pItem.second.m_aDialog)
				PushingDialogs(JsonData, pDialog.m_aText, UniqueID.c_str(), DialogNum++);
		}
		for(auto& pItem : NpcBotInfo::ms_aNpcBot)
		{
			int DialogNum = 0;
			std::string UniqueID("dianp" + std::to_string(pItem.first));
			for(auto& pDialog : pItem.second.m_aDialog)
				PushingDialogs(JsonData, pDialog.m_aText, UniqueID.c_str(), DialogNum++);
		}
		for(auto& pItem : CAetherData::ms_aTeleport)
		{
			PushingDialogs(JsonData, pItem.second.m_aName, "aeth", pItem.first);
		}
		for(auto& [ID, Item] : CAttributeDescription::Data())
		{
			PushingDialogs(JsonData, Item.GetName(), "attb", (int)ID);
		}
		for(auto& pItem : CItemDescription::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "ittm", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetDesc(), "itdc", pItem.first);
		}
		for(auto& pItem : CSkillDataInfo::Data())
		{
			PushingDialogs(JsonData, pItem.second.GetName(), "sknm", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetDesc(), "skds", pItem.first);
			PushingDialogs(JsonData, pItem.second.GetBonusName(), "skbn", pItem.first);
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

	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Completed successfully!");
	ms_MutexDump.unlock();
}