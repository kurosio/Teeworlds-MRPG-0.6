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
#include "components/duties/duties_manager.h"
#include "components/Eidolons/EidolonManager.h"
#include "components/groups/group_manager.h"
#include "components/guilds/guild_manager.h"
#include "components/houses/house_manager.h"
#include "components/inventory/inventory_manager.h"
#include "components/mails/mailbox_manager.h"
#include "components/quests/quest_manager.h"
#include "components/skills/skill_manager.h"
#include "components/warehouse/warehouse_manager.h"
#include "components/wiki/wiki_manager.h"
#include "components/worlds/world_manager.h"
#include "components/accounts/account_listener.h"
#include "components/inventory/inventory_listener.h"

#include <teeother/components/localization.h>

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
	m_System.add(m_pDutiesManager = new CDutiesManager);
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

		if(m_pGameServer->GetWorldID() == INITIALIZER_WORLD_ID)
			pComponent->OnPreInit();

		const auto selectStr = fmt_default("WHERE WorldID = '{}'", m_pGameServer->GetWorldID());
		pComponent->OnInitWorld(selectStr);

		if(isLastInitializedWorld)
			pComponent->OnPostInit();
	}

	// log about listeners
	if(isLastInitializedWorld)
	{
		g_EventListenerManager.LogRegisteredEvents();
		std::thread(&CMmoController::SyncLocalizations, this).detach();
	}
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

	// check time period
	if(GS()->GetWorldID() == INITIALIZER_WORLD_ID &&
		(GS()->Server()->Tick() % (GS()->Server()->TickSpeed() * g_Config.m_SvGlobalPeriodCheckInterval) == 0))
	{
		OnHandleGlobalTimePeriod();
	}

	// update player time period
	if(GS()->Server()->Tick() % (GS()->Server()->TickSpeed() * g_Config.m_SvPlayerPeriodCheckInterval) == 0)
	{
		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			auto* pPlayer = GS()->GetPlayer(i, true);
			if(pPlayer && GS()->IsPlayerInWorld(i))
				OnHandlePlayerTimePeriod(pPlayer);
		}
	}
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
		VStatistics.Add("Discord: ({-})", g_Config.m_SvDiscordInviteLink);
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
		VGroup.AddMenu(MENU_DUTIES_LIST, "\u262C Duties");
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

		// select type list
		VoteWrapper VTopSelect(ClientID, VWF_SEPARATE_OPEN|VWF_ALIGN_TITLE|VWF_STYLE_STRICT_BOLD, "Select a type of ranking");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::PlayerExpert, "Top Specialists in the Realm");
		VTopSelect.AddMenu(MENU_LEADERBOARD, (int)ToplistType::PlayerAttributes, "Top by attributes");
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

	// try send from other components
	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnSendMenuVotes(pPlayer, Menulist))
			return true;
	}

	return false;
}


void CMmoController::OnCharacterTile(CCharacter* pChr) const
{
	if(!pChr || !pChr->IsAlive())
		return;

	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnCharacterTile(pChr);
}


bool CMmoController::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int ExtraValue1, const int ExtraValue2, int ReasonNumber, const char* pReason) const
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


void CMmoController::OnHandleGlobalTimePeriod() const
{
	// initialize variables
	ByteArray RawData {};
	std::vector<int> aPeriodsUpdated {};
	time_t CurrentTimeStamp = time(nullptr);

	// try open file
	mystd::file::result Result = mystd::file::load("server_data/time_periods.cfg", &RawData);
	if(Result == mystd::file::result::ERROR_FILE)
	{
		const auto Data = fmt_default("{}\n{}\n{}", CurrentTimeStamp, CurrentTimeStamp, CurrentTimeStamp);
		mystd::file::save("server_data/time_periods.cfg", Data.data(), (unsigned)Data.size());
		return;
	}

	// load time stamps
	time_t DailyStamp, WeekStamp, MonthStamp;
	auto Data = std::string((char*)RawData.data(), RawData.size());
#ifdef WIN32
	std::sscanf(Data.c_str(), "%llu\n%llu\n%llu", &DailyStamp, &WeekStamp, &MonthStamp);
#else
	std::sscanf(Data.c_str(), "%ld\n%ld\n%ld", &DailyStamp, &WeekStamp, &MonthStamp);
#endif

	// check new days
	if(time_is_new_day(DailyStamp, CurrentTimeStamp))
	{
		DailyStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(DAILY_STAMP);
	}

	// check new week
	if(time_is_new_week(WeekStamp, CurrentTimeStamp))
	{
		WeekStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(WEEK_STAMP);
	}

	// check new month
	if(time_is_new_month(MonthStamp, CurrentTimeStamp))
	{
		MonthStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(MONTH_STAMP);
	}

	// if any time period has been updated
	if(!aPeriodsUpdated.empty())
	{
		Data = fmt_default("{}\n{}\n{}", DailyStamp, WeekStamp, MonthStamp);
		mystd::file::save("server_data/time_periods.cfg", Data.data(), (unsigned)Data.size());
		for(const auto& component : m_System.m_vComponents)
		{
			for(const auto& periods : aPeriodsUpdated)
			{
				auto timePeriod = static_cast<ETimePeriod>(periods);
				component->OnGlobalTimePeriod(timePeriod);
			}
		}
	}
}

void CMmoController::OnHandlePlayerTimePeriod(CPlayer* pPlayer) const
{
	// initialize variables
	std::vector<int> aPeriodsUpdated {};
	time_t CurrentTimeStamp = time(nullptr);

	// check new days
	if(time_is_new_day(pPlayer->Account()->m_Periods.m_DailyStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_DailyStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(DAILY_STAMP);
	}

	// check new week
	if(time_is_new_week(pPlayer->Account()->m_Periods.m_WeekStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_WeekStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(WEEK_STAMP);
	}

	// check new month
	if(time_is_new_month(pPlayer->Account()->m_Periods.m_MonthStamp, CurrentTimeStamp))
	{
		pPlayer->Account()->m_Periods.m_MonthStamp = CurrentTimeStamp;
		aPeriodsUpdated.push_back(MONTH_STAMP);
	}

	// if any time period has been updated
	if(!aPeriodsUpdated.empty())
	{
		SaveAccount(pPlayer, SAVE_TIME_PERIODS);
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
	auto vResult = GetTopList(Type, Rows);

	if(Type == ToplistType::GuildLeveling)
	{
		pWrapper->SetTitle("Top 10 guilds leveling");
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
		for(auto& [Iter, Top] : vResult)
		{
			const char* pNickname = Instance::Server()->GetAccountNickname(Top.Data["AccountID"].to_int());
			const auto Level = Top.Data["Level"].to_int();
			pWrapper->Add("{}: '{-} - {}LV'.", Top.Name, pNickname, Level);
		}
	}
	else if(Type == ToplistType::PlayerAttributes)
	{
		pWrapper->SetTitle("Top by attributes");
		for(auto& [Iter, Top] : vResult)
		{
			const char* pNickname = Instance::Server()->GetAccountNickname(Top.Data["AccountID"].to_int());
			const auto Value = Top.Data["Value"].to_int();
			pWrapper->Add("{}: '{-} - {}'.", Top.Name, pNickname, Value);
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
		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Bank + 0 DESC LIMIT {}", Rows);
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
		int Iter = 0;
		auto& vTrackings = g_AccountListener.LevelingTracker().GetTrackings();
		for(auto& [professionID, data] : vTrackings)
		{
			auto& field = vResult[Iter];
			field.Name = GetProfessionName((ProfessionIdentifier)professionID);
			field.Data["AccountID"] = data.AccountID;
			field.Data["Level"] = data.Level;
			Iter++;
		}
	}
	else if(Type == ToplistType::PlayerAttributes)
	{
		int Iter = 0;
		auto& vTrackings = g_InventoryListener.AttributeTracker().GetTrackings();
		for(auto& [attributeID, data] : vTrackings)
		{
			auto* pAttributeInfo = GS()->GetAttributeInfo((AttributeIdentifier)attributeID);
			if(pAttributeInfo)
			{
				auto& field = vResult[Iter];
				field.Name = pAttributeInfo->GetName();
				field.Data["AccountID"] = data.AccountID;
				field.Data["Value"] = data.Amount;
				Iter++;
			}
		}
	}

	return vResult;
}

std::map<int, CMmoController::TempTopData> CMmoController::GetDungeonTopList(int DungeonID, int Rows) const
{
	std::map<int, TempTopData> vResult {};

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_records", "WHERE DungeonID = '{}' ORDER BY Time DESC LIMIT {}", DungeonID, Rows);
	while(pRes->next())
	{
		const auto Rank = pRes->getRow();
		auto& field = vResult[Rank];
		field.Name = GS()->Server()->GetAccountNickname(pRes->getInt("UserID"));
		field.Data["Time"] = pRes->getInt("Time");
	}

	return vResult;
}


void CMmoController::AsyncClientEnterMsgInfo(std::string_view ClientName, int ClientID)
{
	CSqlString<MAX_NAME_LENGTH> Nickname(ClientName.data());
	const auto AsyncEnterRes = Database->Prepare<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick = '{}'", Nickname.cstr());

	AsyncEnterRes->AtExecute([CapturedNickname = std::string(Nickname.cstr()), ClientID](ResultPtr pRes)
	{
		auto* pGS = (CGS*)Instance::Server()->GameServerPlayer(ClientID);

		if(!pRes->next())
		{
			pGS->Chat(ClientID, "You need to register using /register <login> <pass>!");
			pGS->Chat(-1, "Apparently, we have a new player, '{}'!", CapturedNickname);
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
	if(!ms_mtxDump.try_lock())
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
