/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mmo_controller.h"

#include <game/server/gamecontext.h>

#include "components/accounts/account_manager.h"
#include "components/achievements/achievement_manager.h"
#include "components/auction/auction_manager.h"
#include "components/aethernet/aethernet_manager.h"
#include "components/Bots/BotManager.h"
#include "components/crafting/craft_manager.h"
#include "components/dungeons/dungeon_manager.h"
#include "components/Eidolons/EidolonManager.h"
#include "components/groups/group_manager.h"
#include "components/guilds/guild_manager.h"
#include "components/houses/house_manager.h"
#include "components/Inventory/InventoryManager.h"
#include "components/mails/mailbox_manager.h"
#include "components/quests/quest_manager.h"
#include "components/skills/skill_manager.h"
#include "components/warehouse/warehouse_manager.h"
#include "components/wiki/wiki_manager.h"
#include "components/worlds/world_manager.h"
#include <teeother/components/localization.h>
#include "components/Inventory/inventory_listener.h"

CMmoController::CMmoController(CGS* pGameServer) : m_pGameServer(pGameServer)
{
	// order
	m_System.add(m_pAchievementManager = new CAchievementManager);
	m_System.add(m_pQuestManager = new CQuestManager);
	m_System.add(m_pInventoryManager = new CInventoryManager);
	m_System.add(m_pBotManager = new CBotManager);
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
	m_System.add(m_pAccountManager = new CAccountManager);
	m_System.add(m_pMailboxManager = new CMailboxManager);
	m_System.add(new CWikiManager);

}

void CMmoController::OnInit(IServer* pServer, IConsole* pConsole, IStorageEngine* pStorage)
{
	// initialize components
	const auto isLastInitializedWorld = m_pGameServer->GetWorldID() == (Instance::Server()->GetWorldsSize() - 1);
	for(auto& pComponent : m_System.m_vComponents)
	{
		pComponent->m_Core = this;
		pComponent->m_GameServer = m_pGameServer;
		pComponent->m_pServer = pServer;
		pComponent->m_pConsole = pConsole;
		pComponent->m_pStorage = pStorage;

		if(m_pGameServer->GetWorldID() == MAIN_WORLD_ID)
			pComponent->OnPreInit();

		const auto selectStr = fmt_default("WHERE WorldID = '{}'", m_pGameServer->GetWorldID());
		pComponent->OnInitWorld(selectStr);

		if(isLastInitializedWorld)
			pComponent->OnPostInit();
	}

	// log about listeners
	if(isLastInitializedWorld)
		g_EventListenerManager.LogRegisteredEvents();

	SyncLocalizations();
}

void CMmoController::OnConsoleInit(IConsole* pConsole) const
{
	for(auto& pComponent : m_System.m_vComponents)
	{
		pComponent->m_pConsole = pConsole;
		pComponent->OnConsoleInit();
	}
}

void CMmoController::OnTick() const
{
	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnTick();

	if(GS()->Server()->Tick() % ((GS()->Server()->TickSpeed() * 60) * g_Config.m_SvTimePeriodCheckTime) == 0)
		OnHandleTimePeriod();
}

bool CMmoController::OnClientMessage(int MsgID, void* pRawMsg, int ClientID) const
{
	if(!GS()->Server()->ClientIngame(ClientID) || !GS()->GetPlayer(ClientID))
		return false;

	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnClientMessage(MsgID, pRawMsg, ClientID))
			return true;
	}

	return false;
}

void CMmoController::OnPlayerLogin(CPlayer* pPlayer) const
{
	for(const auto& pComponent : m_System.m_vComponents)
		pComponent->OnPlayerLogin(pPlayer);
}

bool CMmoController::OnSendMenuMotd(CPlayer* pPlayer, int Menulist) const
{
	for(const auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnSendMenuMotd(pPlayer, Menulist))
			return true;
	}
	return false;
}

bool CMmoController::OnSendMenuVotes(CPlayer* pPlayer, int Menulist) const
{
	const auto ClientID = pPlayer->GetCID();

	// main menu
	if(Menulist == MENU_MAIN)
	{
		const auto pProfName = GetProfessionName(pPlayer->Account()->GetActiveProfessionID());
		const auto expForLevel = computeExperience(pPlayer->Account()->GetLevel());
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// Statistics menu
		VoteWrapper VStatistics(ClientID, VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE | VWF_SEPARATE, "Class profession: {}", pProfName);
		VStatistics.Add("Level {}, Exp {}/{}", pPlayer->Account()->GetLevel(), pPlayer->Account()->GetExperience(), expForLevel);
		VStatistics.Add("Gold: {$}, Bank: {$}", pPlayer->Account()->GetGold(), pPlayer->Account()->GetBankManager());
		VStatistics.AddMenu(MENU_ACCOUNT_DETAIL_INFO, "\u2698 Detail information");
		VoteWrapper::AddEmptyline(ClientID);

		// Personal Menu
		VoteWrapper VPersonal(ClientID, VWF_ALIGN_TITLE, "\u262A Personal Menu");
		VPersonal.AddMenu(MENU_UPGRADES, "\u2657 Upgrades & Professions ({}p)", pPlayer->Account()->GetTotalProfessionsUpgradePoints());
		VPersonal.AddMenu(MENU_ACHIEVEMENTS, "\u2654 Achievements");
		VPersonal.AddMenu(MENU_EQUIPMENT, "\u26B0 Equipments");
		VPersonal.AddMenu(MENU_MODULES, "\u26B1 Modules");
		VPersonal.AddMenu(MENU_INVENTORY, "\u205C Inventory");
		VoteWrapper::AddEmptyline(ClientID);

		// Group & Social
		VoteWrapper VGroup(ClientID, VWF_ALIGN_TITLE, "\u2600 Social & Group Menu");
		VGroup.AddMenu(MENU_DUNGEONS, "\u262C Dungeons");
		VGroup.AddMenu(MENU_GROUP, "\u2042 Group");
		VGroup.AddMenu(MENU_GUILD_FINDER, "\u20AA Guild finder");
		if(pPlayer->Account()->HasGuild())
		{
			VGroup.AddMenu(MENU_GUILD, "\u32E1 Guild");
		}
		if(pPlayer->Account()->HasHouse())
		{
			VPersonal.AddMenu(MENU_HOUSE, "\u2302 House");
		}
		VGroup.AddMenu(MENU_MAILBOX, "\u2709 Mailbox");
		VoteWrapper::AddEmptyline(ClientID);

		// Settings & Journal
		VoteWrapper VSettings(ClientID, VWF_ALIGN_TITLE, "\u2699 Settings & Journal");
		VSettings.AddMenu(MENU_SETTINGS, "\u2699 Settings");
		VSettings.AddMenu(MENU_JOURNAL_MAIN, "\u270D Journal");
		VoteWrapper::AddEmptyline(ClientID);

		// Information Menu
		VoteWrapper VInfo(ClientID, VWF_ALIGN_TITLE, "\u262A Information Menu");
		VInfo.AddMenu(MENU_LEADERBOARD, "\u21F0 Rankings: Guilds & Players");
		VoteWrapper::AddEmptyline(ClientID);

		return true;
	}

	// top list
	if(Menulist == MENU_LEADERBOARD)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// information
		VoteWrapper VTopInfo(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE, "Top List Information");
		VTopInfo.Add("You can view the top 10 players and guilds.");
		VoteWrapper::AddEmptyline(ClientID);

		// select type list
		VoteWrapper VTopSelect(ClientID, VWF_OPEN, "Select a type of ranking");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::PlayerExpert, "Top Specialists in the Realm");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::GuildLeveling, "Top 10 guilds by leveling");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::GuildWealthy, "Top 10 guilds by wealthy");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::PlayerRating, "Top 10 players by rank points");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::PlayerWealthy, "Top 10 players by wealthy");
		VoteWrapper::AddEmptyline(ClientID);

		// show top list
		VoteWrapper VTopList(ClientID, VWF_STYLE_SIMPLE|VWF_SEPARATE);
		if(const auto Toplist = pPlayer->m_VotesData.GetExtraID())
			ShowTopList(&VTopList, ClientID, (ToplistType)Toplist.value(), 10);

		// backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// ----------------------------------------
	// check append votes
	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnSendMenuVotes(pPlayer, Menulist))
			return true;
	}
	// ----------------------------------------

	return false;
}

void CMmoController::OnCharacterTile(CCharacter* pChr) const
{
	if(!pChr || !pChr->IsAlive())
		return;

	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnCharacterTile(pChr);
}

bool CMmoController::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd,
	const int ExtraValue1, const int ExtraValue2, int ReasonNumber, const char* pReason) const
{
	if(!pPlayer)
		return true;

	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnPlayerVoteCommand(pPlayer, pCmd, ExtraValue1, ExtraValue2, ReasonNumber, pReason))
			return true;
	}
	return false;
}

bool CMmoController::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) const
{
	if(!pPlayer)
		return true;

	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnPlayerMotdCommand(pPlayer, pMotdData, pCmd))
			return true;
	}
	return false;
}

void CMmoController::OnResetClientData(int ClientID) const
{
	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnClientReset(ClientID);
}

void CMmoController::OnHandleTimePeriod() const
{
	// Declare a current timestamp, byte array to store raw data
	ByteArray RawData {};
	time_t CurrentTimeStamp = time(nullptr);

	// Load the file "time_periods.cfg" and store the result in a variable
	mystd::file::result Result = mystd::file::load("time_periods.cfg", &RawData);
	if(Result == mystd::file::result::ERROR_FILE)
	{
		// Save the data to the file "time_periods.cfg"
		std::string Data = std::to_string(CurrentTimeStamp) + "\n" + std::to_string(CurrentTimeStamp) + "\n" + std::to_string(CurrentTimeStamp);
		mystd::file::save("time_periods.cfg", Data.data(), (unsigned)Data.size());
		return;
	}

	// Declare variables to store timestamps for daily, weekly, and monthly data
	time_t DailyStamp, WeekStamp, MonthStamp;
	std::string Data = std::string((char*)RawData.data(), RawData.size());
#ifdef WIN32 // time_t is llu on windows and ld on linux
	std::sscanf(Data.c_str(), "%llu\n%llu\n%llu", &DailyStamp, &WeekStamp, &MonthStamp);
#else
	std::sscanf(Data.c_str(), "%ld\n%ld\n%ld", &DailyStamp, &WeekStamp, &MonthStamp);
#endif

	// Set a flag indicating whether time periods have been updated
	std::vector<int> aPeriodsUpdated {};

	// Check if the current time is a new day
	if(time_is_new_day(DailyStamp, CurrentTimeStamp))
	{
		DailyStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(DAILY_STAMP);
	}

	// Check if the current time is a new week
	if(time_is_new_week(WeekStamp, CurrentTimeStamp))
	{
		WeekStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(WEEK_STAMP);
	}

	// Check if the current time is a new month
	if(time_is_new_month(MonthStamp, CurrentTimeStamp))
	{
		MonthStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(MONTH_STAMP);
	}

	// If any time period has been updated
	if(!aPeriodsUpdated.empty())
	{
		std::string Data = std::to_string(DailyStamp) + "\n" + std::to_string(WeekStamp) + "\n" + std::to_string(MonthStamp);
		mystd::file::save("time_periods.cfg", Data.data(), (unsigned)Data.size());

		// Check time periods for all components
		for(const auto& component : m_System.m_vComponents)
		{
			for(const auto& periods : aPeriodsUpdated)
			{
				ETimePeriod timePeriod = static_cast<ETimePeriod>(periods);
				component->OnTimePeriod(timePeriod);
			}
		}
	}
}

void CMmoController::OnHandlePlayerTimePeriod(CPlayer* pPlayer) const
{
	// Set a flag indicating whether time periods have been updated
	std::vector<int> aPeriodsUpdated {};

	// Get the current time
	time_t CurrentTimeStamp = time(nullptr);

	// Check if it is a new day and update the daily time period if necessary
	if(time_is_new_day(pPlayer->Account()->m_Periods.m_DailyStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_DailyStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(DAILY_STAMP);
	}

	// Check if it is a new week and update the weekly time period if necessary
	if(time_is_new_week(pPlayer->Account()->m_Periods.m_WeekStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_WeekStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(WEEK_STAMP);
	}

	// Check if it is a new month and update the monthly time period if necessary
	if(time_is_new_month(pPlayer->Account()->m_Periods.m_MonthStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_MonthStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(MONTH_STAMP);
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
				component->OnPlayerTimePeriod(pPlayer, static_cast<ETimePeriod>(periods));
			}
		}
	}
}

// saving account
void CMmoController::SaveAccount(CPlayer* pPlayer, int Table) const
{
	if(!pPlayer->IsAuthed())
		return;

	CAccountData* pAccount = pPlayer->Account();
	const auto AccountID = pAccount->GetID();

	// save account base
	if(Table == SAVE_STATS)
	{
		const auto Bank = pAccount->GetBankManager().to_string();
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Bank = '{}' WHERE ID = '{}'", Bank, AccountID);
	}

	// save social
	else if(Table == SAVE_SOCIAL)
	{
		const auto CrimeScore = pAccount->GetCrime();
		Database->Execute<DB::UPDATE>("tw_accounts_data", "CrimeScore = '{}' WHERE ID = '{}'", CrimeScore, AccountID);
	}

	// save profession
	else if(Table == SAVE_PROFESSION)
	{
		const auto ProfessionID = (int)pAccount->GetActiveProfessionID();
		Database->Execute<DB::UPDATE>("tw_accounts_data", "ProfessionID = '{}' WHERE ID = '{}'", ProfessionID, AccountID);
	}

	// save world position
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = AccountManager()->GetLastVisitedWorldID(pPlayer);
		Database->Execute<DB::UPDATE>("tw_accounts_data", "WorldID = '{}' WHERE ID = '{}'", LatestCorrectWorldID, AccountID);
	}

	// save time periods
	else if(Table == SAVE_TIME_PERIODS)
	{
		const auto Daily = pAccount->m_Periods.m_DailyStamp;
		const auto Week = pAccount->m_Periods.m_WeekStamp;
		const auto Month = pAccount->m_Periods.m_MonthStamp;
		Database->Execute<DB::UPDATE>("tw_accounts_data", "DailyStamp = '{}', WeekStamp = '{}', MonthStamp = '{}' WHERE ID = '{}'", Daily, Week, Month, AccountID);
	}

	// save achievements
	else if(Table == SAVE_ACHIEVEMENTS)
	{
		const auto AchievementsData = pAccount->GetAchievementsData().dump();
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Achievements = '{}' WHERE ID = '{}'", AchievementsData, AccountID);
	}

	// save language
	else if(Table == SAVE_LANGUAGE)
	{
		const char* pLanguage = pPlayer->GetLanguage();
		Database->Execute<DB::UPDATE>("tw_accounts", "Language = '{}' WHERE ID = '{}'", pLanguage, AccountID);
	}

	// main data
	else
	{
		const auto pLogin = pAccount->GetLogin();
		Database->Execute<DB::UPDATE>("tw_accounts", "Username = '{}' WHERE ID = '{}'", pLogin, AccountID);
	}
}

void CMmoController::ShowTopList(VoteWrapper* pWrapper, int ClientID, ToplistType Type, int Rows) const
{
	if(Type == ToplistType::GuildLeveling)
	{
		pWrapper->SetTitle("Top 10 guilds leveling");

		auto vResult = GetTopList(Type, Rows);
		for(auto& Elem : vResult)
		{
			const auto Rank = Elem.first;
			const auto Guildname = Elem.second.Name;
			const auto Level = Elem.second.Data["Level"].to_int();
			const auto Experience = Elem.second.Data["Exp"];
			pWrapper->Add("{}. {} :: Level {} : Exp {}", Rank, Guildname, Level, Experience);
		}
	}
	else if(Type == ToplistType::GuildWealthy)
	{
		pWrapper->SetTitle("Top 10 guilds wealthy");

		auto vResult = GetTopList(Type, Rows);
		for(auto& Elem : vResult)
		{
			const auto Rank = Elem.first;
			const auto Guildname = Elem.second.Name;
			const auto Bank = Elem.second.Data["Bank"];
			pWrapper->Add("{}. {} :: Bank {$}", Rank, Guildname, Bank);
		}
	}
	else if(Type == ToplistType::PlayerRating)
	{
		pWrapper->SetTitle("Top 10 players rank points");

		auto vResult = GetTopList(Type, Rows);
		for(auto& Elem : vResult)
		{
			const auto Rank = Elem.first;
			const auto Nickname = Elem.second.Name;
			const auto Rating = Elem.second.Data["Rating"].to_int();
			pWrapper->Add("{}. {} :: Rating {}", Rank, Nickname, Rating);
		}
	}
	else if(Type == ToplistType::PlayerWealthy)
	{
		pWrapper->SetTitle("Top 10 players wealthy");

		auto vResult = GetTopList(Type, Rows);
		for(auto& Elem : vResult)
		{
			const auto Rank = Elem.first;
			const auto Nickname = Elem.second.Name;
			const auto Bank = Elem.second.Data["Bank"];
			pWrapper->Add("{}. {} :: Wealthy(bank) {$} golds", Rank, Nickname, Bank);
		}
	}
	else if(Type == ToplistType::PlayerExpert)
	{
		pWrapper->SetTitle("Top Specialists in the Realm");

		auto vResult = GetTopList(Type, Rows);
		for(auto& [Iter, Top] : vResult)
		{
			auto* pNickname = Instance::Server()->GetAccountNickname(Top.Data["AccountID"].to_int());
			auto* pAttribute = GS()->GetAttributeInfo((AttributeIdentifier)Top.Data["ID"].to_int());
			auto Value = Top.Data["Value"].to_int();
			pWrapper->Add("{}: '{-} - {}({})'.", Top.Name, pNickname, Value, pAttribute->GetName());
		}
	}
}

std::map<int, CMmoController::TempTopData> CMmoController::GetTopList(ToplistType Type, int Rows) const
{
	std::map<int, TempTopData> vResult {};

	if(Type == ToplistType::GuildLeveling)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Level DESC, Exp DESC LIMIT {}", Rows);
		while(pRes->next())
		{
			const auto Rank = pRes->getRow();
			auto& field = vResult[Rank];
			field.Name = pRes->getString("Name");
			field.Data["Level"] = pRes->getInt("Level");
			field.Data["Exp"] = pRes->getUInt64("Exp");
		}
	}
	else if(Type == ToplistType::GuildWealthy)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Bank DESC LIMIT {}", Rows);
		while(pRes->next())
		{
			const auto Rank = pRes->getRow();
			auto& field = vResult[Rank];
			field.Name = pRes->getString("Name");
			field.Data["Bank"] = pRes->getBigInt("Bank");
		}
	}
	else if(Type == ToplistType::PlayerRating)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "ORDER BY Rating DESC LIMIT {}", Rows);
		while(pRes->next())
		{
			const auto Rank = pRes->getRow();
			auto& field = vResult[Rank];
			field.Name = pRes->getString("Nick");
			field.Data["ID"] = pRes->getInt("ID");
			field.Data["Rating"] = pRes->getInt("Rating");
		}
	}
	else if(Type == ToplistType::PlayerWealthy)
	{
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "ORDER BY Bank + 0 DESC LIMIT {}", Rows);
		while(pRes->next())
		{
			const auto Rank = pRes->getRow();
			auto& field = vResult[Rank];
			field.Name = pRes->getString("Nick");
			field.Data["ID"] = pRes->getInt("ID");
			field.Data["Bank"] = pRes->getBigInt("Bank");
		}
	}
	else if(Type == ToplistType::PlayerExpert)
	{
		auto BiggestTankOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::HP);
		auto BiggestHealerOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::MP);
		auto BiggestDPSOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::CritDMG);
		auto BiggestMinerOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::Efficiency);
		auto BiggestFarmerOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::Extraction);
		auto BiggestFisherOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::Patience);
		auto BiggestLoaderOpt = g_InventoryListener.AttributeTracker().GetTrackingData((int)AttributeIdentifier::ProductCapacity);

		std::array<std::tuple<std::string, int, std::optional<TrackingAttributeData>>, 7> topList = {
		{
			{GetProfessionName(ProfessionIdentifier::Tank), (int)AttributeIdentifier::HP, BiggestTankOpt},
			{GetProfessionName(ProfessionIdentifier::Healer), (int)AttributeIdentifier::MP, BiggestHealerOpt},
			{GetProfessionName(ProfessionIdentifier::Dps), (int)AttributeIdentifier::CritDMG, BiggestDPSOpt},
			{GetProfessionName(ProfessionIdentifier::Miner), (int)AttributeIdentifier::Efficiency, BiggestMinerOpt},
			{GetProfessionName(ProfessionIdentifier::Farmer), (int)AttributeIdentifier::Extraction, BiggestFarmerOpt},
			{GetProfessionName(ProfessionIdentifier::Fisherman), (int)AttributeIdentifier::Patience, BiggestFisherOpt},
			{GetProfessionName(ProfessionIdentifier::Loader), (int)AttributeIdentifier::ProductCapacity, BiggestLoaderOpt}
		}};

		int Iter = 0;
		for(const auto& [title, attributeID, biggestTrackData] : topList)
		{
			if(biggestTrackData)
			{
				auto& field = vResult[Iter];
				field.Name = title;
				field.Data["AccountID"] = biggestTrackData->AccountID;
				field.Data["ID"] = attributeID;
				field.Data["Value"] = biggestTrackData->Amount;
				Iter++;
			}
		}
	}

	return vResult;
}

void CMmoController::AsyncClientEnterMsgInfo(const std::string ClientName, int ClientID)
{
	CSqlString<MAX_NAME_LENGTH> PlayerName(ClientName.c_str());
	const auto AsyncEnterRes = Database->Prepare<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick = '{}'", PlayerName.cstr());

	AsyncEnterRes->AtExecute([PlayerName = std::string(PlayerName.cstr()), ClientID](ResultPtr pRes)
	{
		auto* pGS = (CGS*)Instance::Server()->GameServerPlayer(ClientID);

		if(!pRes->next())
		{
			pGS->Chat(ClientID, "You need to register using /register <login> <pass>!");
			pGS->Chat(-1, "Apparently, we have a new player, '{}'!", PlayerName.c_str());
			return;
		}

		pGS->Chat(ClientID, "You need to log in using /login <user> <pass>!");
	});
}

// dump dialogs for translate
void CMmoController::SyncLocalizations() const
{
	// check action state
	static std::mutex ms_mtxDump;
	if (!ms_mtxDump.try_lock())
	{
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Wait the last operation is in progress..");
		return;
	}

	// update language data TODO FIX
	for (int i = 0; i < GS()->Server()->Localization()->m_pLanguages.size(); i++)
	{
		// prepare
		auto* pLanguage = GS()->Server()->Localization()->m_pLanguages[i];
		pLanguage->Updater().Prepare();

		for (const auto& [ID, p] : QuestBotInfo::ms_aQuestBot)
		{
			int DialogNum = 0;
			for (const auto& pDialog : p.m_aDialogs)
			{
				pLanguage->Updater().Push(pDialog.GetText(), std::string("dialog_quest" + std::to_string(ID)).c_str(), DialogNum++);
			}
		}

		for (const auto& [ID, p] : NpcBotInfo::ms_aNpcBot)
		{
			int DialogNum = 0;
			for (const auto& pDialog : p.m_aDialogs)
			{
				pLanguage->Updater().Push(pDialog.GetText(), std::string("dialog_npc" + std::to_string(ID)).c_str(), DialogNum++);
			}
		}

		for (const auto& p : CAetherData::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "aether", p->GetID());
		}

		for (const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			pLanguage->Updater().Push(pAttribute->GetName(), "attribute", (int)ID);
		}

		for (const auto& [ID, p] : CItemDescription::Data())
		{
			pLanguage->Updater().Push(p.GetName(), "item_name", ID);
			pLanguage->Updater().Push(p.GetDescription(), "item_description", ID);
		}

		for (const auto& [ID, p] : CSkillDescription::Data())
		{
			pLanguage->Updater().Push(p.GetName(), "skill_name", ID);
			pLanguage->Updater().Push(p.GetDescription(), "skill_description", ID);
			pLanguage->Updater().Push(p.GetBoostName(), "skill_boost", ID);
		}

		for (const auto& [ID, p] : CQuestDescription::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "quest_name", ID);
		}

		for (const auto& p : CWarehouse::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "warehouse_name", p->GetID());
		}

		for (const auto& p : CHouse::Data())
		{
			pLanguage->Updater().Push(p->GetClassName(), "house_class_name", p->GetID());
		}

		// finish
		pLanguage->Updater().Finish();
	}

	ms_mtxDump.unlock();
}
