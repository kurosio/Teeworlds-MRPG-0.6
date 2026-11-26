/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mmo_controller.h"
#include "mmo_db.h"

#include <engine/server/async.h>

#include <game/server/gamecontext.h>

#include "components/accounts/account_manager.h"
#include "components/accounts/account_auth.h"
#include "components/accounts/account_db.h"
#include "components/achievements/achievement_manager.h"
#include "components/auction/auction_manager.h"
#include "components/aethernet/aethernet_manager.h"
#include "components/bots/bot_manager.h"
#include "components/crafting/craft_manager.h"
#include "components/duties/duties_manager.h"
#include "components/eidolons/eidolon_manager.h"
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
#include "components/events/mini_events_manager.h"
#include "components/accounts/account_listener.h"

#include <engine/server/async_file.h>
#include <game/server/core/tools/db_async_context.h>
#include <teeother/components/localization.h>

namespace
{
	std::mutex gs_LocalizationSyncMutex;

	// The endpoints of the tables this file touches, resolved once at compile time.
}

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
	m_System.add(m_pMiniEventsManager = new CMiniEventsManager);

}

void CMmoController::OnInit(IServer* pServer, IConsole* pConsole, IStorageEngine* pStorage)
{
	// initialize cross-server synchronization: the event is applied by the same
	// component on every server, the payload comes from the relay
	Sync->SetEventCallback([this](const std::string& EventName, const CServerSync::SRequest& Req) 
	{
		if(Req.WorldID < 0 || Req.WorldID >= Instance::Server()->GetWorldsSize())
			return;

		auto* pGS = static_cast<CGS*>(Instance::GameServer(Req.WorldID));
		for(auto& pComponent : pGS->Core()->m_System.m_vComponents)
			pComponent->OnSyncEvent(EventName, Req);
	});

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

		pComponent->OnInitWorld(m_pGameServer->GetWorldID());

		if(isLastInitializedWorld)
			pComponent->OnPostInit();
	}

	// Load local links first. Managers create only the rows that are missing.
	if(isLastInitializedWorld)
	{
		auto Links = MmoDb::ListLinks();
		while(Links.Next())
		{
			const auto Type = Links.GetString("Type");
			const auto OwnerID = Links.GetInt64("OwnerID");
			const auto TargetID = Links.GetInt64("TargetID");
			const auto MetaJson = Links.GetString("MetaJson");

			for(auto* pComponent : m_System.m_vComponents)
				pComponent->OnInitLink(Type, OwnerID, TargetID, MetaJson);
		}
		
		for(auto* pComponent : m_System.m_vComponents)
			pComponent->OnLinksLoaded();

		g_EventListenerManager.LogRegisteredEvents();
		SyncLocalizationsAsync();
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

	// The leveling tracker is one global list, so its refresh runs here once and not in every world.
	if(GS()->GetWorldID() == INITIALIZER_WORLD_ID)
		g_AccountListener.OnTick();

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


void CMmoController::RegisterAccountLoadStages(CPlayer* pPlayer) const
{
	for(const auto& pComponent : m_System.m_vComponents)
		pComponent->OnRegisterAccountLoadStages(pPlayer);
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
		const auto spAmount = pPlayer->GetItem(itSkillPoint)->GetValue();
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// Statistics menu
		VoteWrapper VStatistics(ClientID, VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE | VWF_SEPARATE, "Class profession: {}", pProfName);
		VStatistics.Add("Discord: ({~})", g_Config.m_SvDiscordInviteLink);
		VStatistics.Add("Level {}, Exp {}/{}", pPlayer->Account()->GetLevel(), pPlayer->Account()->GetExperience(), expForLevel);
		VStatistics.Add("Gold: {$}, Bank: {$}", pPlayer->Account()->GetGold(), pPlayer->Account()->GetBankManager());
		VStatistics.AddMenu(MENU_ACCOUNT_DETAIL_INFO, "\u2698 Detail information");
		VoteWrapper::AddEmptyline(ClientID);

		// Personal Menu
		VoteWrapper VPersonal(ClientID, VWF_ALIGN_TITLE, "\u262A Personal Menu");
		VPersonal.AddMenu(MENU_UPGRADES, "\u2657 Upgrades & Professions ({}p)", pPlayer->Account()->GetTotalProfessionsUpgradePoints());
		VPersonal.AddMenu(MENU_SKILLS, "\u2654 Skills ({}sp)", spAmount);
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


bool CMmoController::OnPlayerVoteCommand(CPlayer *pPlayer, const char *pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char *pReason) const
{
    if(!pPlayer)
        return true;

    for(auto& pComponent : m_System.m_vComponents)
    {
        if(pComponent->OnPlayerVoteCommand(pPlayer, pCmd, Extras, ReasonNumber, pReason))
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
	// The period stamps live in a small file; it is read in the background and the
	// logic runs on the game thread when the content arrives. A second call while
	// the read is running is dropped, so the checks cannot pile up.
	if(m_TimePeriodPending)
		return;

	m_TimePeriodPending = true;
	AsyncFile::Read("server_data/time_periods.cfg", [this](AsyncFile::SReadResult&& Read)
	{
		m_TimePeriodPending = false;
		if(!Read.Ok() && !Read.Missing())
		{
			dbg_msg("time_period", "could not read time periods: %s", Read.m_Error.c_str());
			return;
		}
		ApplyGlobalTimePeriod(Read.m_Content);
	});
}

void CMmoController::ApplyGlobalTimePeriod(const std::string& Data) const
{
	// initialize variables
	std::vector<ETimePeriod> aPeriodsUpdated {};
	const time_t CurrentTimeStamp = time(nullptr);

	// no file yet: the periods start now
	if(Data.empty())
	{
		AsyncFile::Write("server_data/time_periods.cfg", fmt_default("{}\n{}\n{}", CurrentTimeStamp, CurrentTimeStamp, CurrentTimeStamp));
		return;
	}

	// load time stamps
	time_t DailyStamp = CurrentTimeStamp;
	time_t WeekStamp = CurrentTimeStamp;
	time_t MonthStamp = CurrentTimeStamp;

	int Parsed = 0;

#ifdef WIN32
	Parsed = std::sscanf(Data.c_str(), "%llu\n%llu\n%llu",
		&DailyStamp, &WeekStamp, &MonthStamp);
#else
	Parsed = std::sscanf(Data.c_str(), "%ld\n%ld\n%ld",
		&DailyStamp, &WeekStamp, &MonthStamp);
#endif

	if(Parsed != 3)
	{
		DailyStamp = WeekStamp = MonthStamp = CurrentTimeStamp;
	}

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
		AsyncFile::Write("server_data/time_periods.cfg", fmt_default("{}\n{}\n{}", DailyStamp, WeekStamp, MonthStamp));
		for(const auto& component : m_System.m_vComponents)
		{
			for(const auto& periods : aPeriodsUpdated)
			{
				auto timePeriod = periods;
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
		AccountDb::SaveBank(AccountID, Bank);
	}

	// save social
	else if(Table == SAVE_SOCIAL)
	{
		const auto CrimeScore = pAccount->GetCrime();
		AccountDb::SaveCrimeScore(AccountID, CrimeScore);
	}

	// save profession
	else if(Table == SAVE_PROFESSION)
	{
		const auto ProfessionID = (int)pAccount->GetActiveProfessionID();
		AccountDb::SaveProfessionID(AccountID, ProfessionID);
	}

	// save world position
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = AccountManager()->GetLastVisitedWorldID(pPlayer);
		AccountDb::SaveWorldID(AccountID, LatestCorrectWorldID);
	}

	// save time periods
	else if(Table == SAVE_TIME_PERIODS)
	{
		const auto Daily = pAccount->m_Periods.m_DailyStamp;
		const auto Week = pAccount->m_Periods.m_WeekStamp;
		const auto Month = pAccount->m_Periods.m_MonthStamp;
		AccountDb::SaveStamps(AccountID, Daily, Week, Month);
	}

	// save language
	else if(Table == SAVE_LANGUAGE)
	{
		AccountDb::SaveLanguage(AccountID, pPlayer->GetLanguage());
	}

	// main data
	else
	{
		AccountDb::SaveUsername(AccountID, pAccount->GetLogin());
	}
}


std::string CMmoController::TopUpdateTag(ToplistType Type)
{
	return fmt_default("top_{}", (int)Type);
}

std::string CMmoController::DungeonUpdateTag(int DungeonID)
{
	return fmt_default("dungeon_{}", DungeonID);
}

std::string CMmoController::RhythmUpdateTag(int WorldID, const std::string& Difficulty)
{
	return fmt_default("rhythm_{}_{}", WorldID, Difficulty);
}


void CMmoController::ShowTopList(VoteWrapper* pWrapper, int ClientID, ToplistType Type, int Rows) const
{
	const char* pTitle = "";
	switch(Type)
	{
		case ToplistType::GuildLeveling:   pTitle = "Top 10 guilds leveling";       break;
		case ToplistType::GuildWealthy:    pTitle = "Top 10 guilds wealthy";        break;
		case ToplistType::PlayerRating:    pTitle = "Top 10 players rank points";   break;
		case ToplistType::PlayerWealthy:   pTitle = "Top 10 players wealthy";       break;
		case ToplistType::PlayerExpert:    pTitle = "Top Specialists in the Realm"; break;
		case ToplistType::PlayerAttributes: pTitle = "Top by attributes";           break;
		default: break;
	}
	pWrapper->SetTitle(pTitle);

	const std::string Tag = TopUpdateTag(Type);
	const auto& vResult = GetTopList(Type, Rows);
	if(Type == ToplistType::PlayerExpert)
		(void)pWrapper->WaitLoad(CTrackingData::ProfessionTag(), 5);
	else if(Type != ToplistType::PlayerAttributes)
		(void)pWrapper->WaitLoad(Tag, 5);

	// An empty cache means either "still loading" or "nothing to rank"
	bool Loading = false;
	if(Type == ToplistType::PlayerExpert)
	{
		const auto& Tracker = g_AccountListener.TrackingData();
		Loading = !Tracker.IsProfessionLoaded() && !Db::HasTagCompleted(CTrackingData::ProfessionTag());
	}
	else if(Type != ToplistType::PlayerAttributes)
		Loading = !Db::HasTagCompleted(Tag);

	if(vResult.empty())
	{
		if(Loading)
			pWrapper->Add("Loading ...");
		else
			pWrapper->Add("No records yet.");
		return;
	}

	switch(Type)
	{
		case ToplistType::GuildLeveling:
			for(const auto& [Rank, Top] : vResult)
				pWrapper->Add("{}. {~} :: Level {} : Exp {}", Rank, Top.m_Name, Top.m_Value.to_int(), Top.m_Extra);
			break;
		case ToplistType::GuildWealthy:
			for(const auto& [Rank, Top] : vResult)
				pWrapper->Add("{}. {~} :: Bank {$}", Rank, Top.m_Name, Top.m_Value);
			break;
		case ToplistType::PlayerRating:
			for(const auto& [Rank, Top] : vResult)
				pWrapper->Add("{}. {~} :: Rating {}", Rank, Top.m_Name, Top.m_Value.to_int());
			break;
		case ToplistType::PlayerWealthy:
			for(const auto& [Rank, Top] : vResult)
				pWrapper->Add("{}. {~} :: Wealthy(bank) {$} golds", Rank, Top.m_Name, Top.m_Value);
			break;
		case ToplistType::PlayerExpert:
			for(const auto& [Rank, Top] : vResult)
				pWrapper->Add("{}: '{~} - {}LV'.", Top.m_Name, Instance::Server()->GetAccountNickname(Top.m_Extra.to_int()), Top.m_Value.to_int());
			break;
		case ToplistType::PlayerAttributes:
			for(const auto& [Rank, Top] : vResult)
				pWrapper->Add("{}: '{~} - {}'.", Top.m_Name, Instance::Server()->GetAccountNickname(Top.m_Extra.to_int()), Top.m_Value.to_int());
			break;
		default:
			break;
	}
}


bool CMmoController::NeedTopRows(const std::string& Key) const
{
	const STopRows& Cache = m_TopRows[Key];
	constexpr int LifetimeSeconds = 20;
	return !Cache.m_Loaded || GS()->Server()->Tick() - Cache.m_Stamp >= LifetimeSeconds * GS()->Server()->TickSpeed();
}

void CMmoController::SetTopRows(const std::string& Key, std::map<int, TempTopData>&& Rows) const
{
	STopRows& Cache = m_TopRows[Key];
	Cache.m_Rows = std::move(Rows);
	Cache.m_Stamp = GS()->Server()->Tick();
	Cache.m_Loaded = true;
}

const std::map<int, CMmoController::TempTopData>& CMmoController::GetTopList(ToplistType Type, int Rows) const
{
	const std::string Tag = TopUpdateTag(Type);

	if(Type == ToplistType::PlayerExpert || Type == ToplistType::PlayerAttributes)
		return BuildTrackingTopList(Type);

	if(!NeedTopRows(Tag) || Db::IsTagRunning(Tag))
		return TopRows(Tag);

	if(Type == ToplistType::GuildLeveling)
	{
		MmoDb::TopGuildsByLevel(Rows, Tag).Then([this, Tag](Db::CResultSet&& Guilds)
		{
			std::map<int, TempTopData> vResult {};
			while(Guilds.Next())
			{
				auto& Field = vResult[Guilds.Row()];
				Field.m_Name = Guilds.GetString("Name");
				Field.m_Value = Guilds.GetInt("Level");
				Field.m_Extra = Guilds.GetUInt64("Exp");
			}
			SetTopRows(Tag, std::move(vResult));
		});
	}
	else if(Type == ToplistType::GuildWealthy)
	{
		MmoDb::TopGuildsByBank(Rows, Tag).Then([this, Tag](Db::CResultSet&& Guilds)
		{
			std::map<int, TempTopData> vResult {};
			while(Guilds.Next())
			{
				auto& Field = vResult[Guilds.Row()];
				Field.m_Name = Guilds.GetString("Name");
				Field.m_Value = Guilds.GetBigInt("Bank");
			}
			SetTopRows(Tag, std::move(vResult));
		});
	}
	else if(Type == ToplistType::PlayerRating)
	{
		MmoDb::TopAccountsByRating(Rows, Tag).Then([this, Tag](Db::CResultSet&& Accounts)
		{
			std::map<int, TempTopData> vResult {};
			while(Accounts.Next())
			{
				auto& Field = vResult[Accounts.Row()];
				Field.m_Name = Accounts.GetString("Nick");
				Field.m_Value = Accounts.GetInt("Rating");
			}
			SetTopRows(Tag, std::move(vResult));
		});
	}
	else if(Type == ToplistType::PlayerWealthy)
	{
		MmoDb::TopAccountsByBank(Rows, Tag).Then([this, Tag](Db::CResultSet&& Accounts)
		{
			std::map<int, TempTopData> vResult {};
			while(Accounts.Next())
			{
				auto& Field = vResult[Accounts.Row()];
				Field.m_Name = Accounts.GetString("Nick");
				Field.m_Value = Accounts.GetBigInt("Bank");
			}
			SetTopRows(Tag, std::move(vResult));
		});
	}
	return TopRows(Tag);
}


const std::map<int, CMmoController::TempTopData>& CMmoController::BuildTrackingTopList(ToplistType Type) const
{
	const auto& Tracking = g_AccountListener.TrackingData();
	std::vector<TempTopData> Rows {};

	if(Type == ToplistType::PlayerExpert)
	{
		for(int ID = (int)ProfessionIdentifier::Tank; ID < (int)ProfessionIdentifier::NUM_PROFESSIONS; ++ID)
		{
			const auto* Leader = Tracking.GetProfessionLeader((ProfessionIdentifier)ID);
			if(!Leader)
				continue;

			TempTopData Row {};
			Row.m_Name = GetProfessionName((ProfessionIdentifier)ID);
			Row.m_Value = Leader->Level;
			Row.m_Extra = Leader->AccountID;
			Rows.push_back(std::move(Row));
		}
	}
	else
	{
		for(const auto& [ID, Leader] : Tracking.GetAttributes())
		{
			const auto* Info = GS()->GetAttributeInfo((AttributeIdentifier)ID);
			if(!Info || Leader.AccountID <= 0 || Leader.Amount <= 0)
				continue;

			TempTopData Row {};
			Row.m_Name = Info->GetName();
			Row.m_Value = Leader.Amount;
			Row.m_Extra = Leader.AccountID;
			Rows.push_back(std::move(Row));
		}
	}

	std::sort(Rows.begin(), Rows.end(), [](const TempTopData& Left, const TempTopData& Right)
	{
		if(Left.m_Value != Right.m_Value)
			return Left.m_Value > Right.m_Value;
		if(Left.m_Name != Right.m_Name)
			return Left.m_Name < Right.m_Name;
		return Left.m_Extra < Right.m_Extra;
	});

	std::map<int, TempTopData> Result {};
	for(int Rank = 0; Rank < (int)Rows.size(); ++Rank)
		Result[Rank] = std::move(Rows[Rank]);

	const auto Key = TopUpdateTag(Type);
	SetTopRows(Key, std::move(Result));
	return TopRows(Key);
}

const std::map<int, CMmoController::TempTopData>& CMmoController::GetDungeonTopList(int DungeonID, int Rows) const
{
	const std::string Tag = DungeonUpdateTag(DungeonID);
	if(!NeedTopRows(Tag) || Db::IsTagRunning(Tag))
		return TopRows(Tag);

	MmoDb::TopDungeonRecords(DungeonID, Rows, Tag).Then(
		[this, Tag](Db::CResultSet&& Records)
		{
			std::map<int, TempTopData> vResult {};
			while(Records.Next())
			{
				auto& Field = vResult[Records.Row()];
				Field.m_Name = Instance::Server()->GetAccountNickname(Records.GetInt("UserID"));
				Field.m_Value = Records.GetInt("Time");
			}
			SetTopRows(Tag, std::move(vResult));
		});

	return TopRows(Tag);
}

const std::map<int, CMmoController::TempTopData>& CMmoController::GetRhythmTopList(int WorldID, const std::string& Difficulty, int Rows) const
{
	const std::string Tag = RhythmUpdateTag(WorldID, Difficulty);
	if(!NeedTopRows(Tag) || Db::IsTagRunning(Tag))
		return TopRows(Tag);

	MmoDb::TopRhythmRecords(WorldID, Difficulty, Rows, Tag).Then(
		[this, Tag](Db::CResultSet&& Records)
		{
			std::map<int, TempTopData> vResult {};
			while(Records.Next())
			{
				auto& Field = vResult[Records.Row()];
				Field.m_Name = Instance::Server()->GetAccountNickname(Records.GetInt("UserID"));
				Field.m_Value = Records.GetInt("Score");
			}
			SetTopRows(Tag, std::move(vResult));
		});

	return TopRows(Tag);
}


void CMmoController::AsyncClientEnterMsgInfo(std::string_view ClientName, int ClientID)
{
	auto pContext = DbAsync::MakeContext(ClientID);
	AccountDb::FindAccountByNickname(ClientName)
		.Then([pContext, CapturedNickname = std::string(ClientName), ClientID](Db::CResultSet&& Result)
	{
		auto* pGS = (CGS*)Instance::Server()->GameServerPlayer(ClientID);
		if(!pGS)
			return;

		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		// first enter
		const bool HasAccount = Result.HasRows();
		const std::string GuestCredential = AccountAuth::BuildGuestCredential(CapturedNickname.c_str());
		pPlayer->GetSharedData().m_AuthType = AUTH_TYPE_NONE;
		if(!HasAccount)
		{
			pPlayer->GetSharedData().m_AuthType = AUTH_TYPE_REGISTRATION;
			pPlayer->GetSharedData().m_GuestLogin = GuestCredential;
			pGS->Chat(-1, "Apparently, we have a new player, '{~}'!", CapturedNickname);
			pGS->Chat(ClientID, "Register with /register <login> <pass>.");
			pGS->Chat(ClientID, "Play as a guest via 'Esc → Play' and save progress later.");
			return;
		}

		// initialize variables
		const auto Login = Result.GetString("Username");
		const auto TimeoutCode = Result.GetString("TimeoutCode");
		const bool IsGuest = AccountAuth::IsGuestCredentialForNickname(Login, CapturedNickname.c_str());
		const bool HasTimeoutCode = !TimeoutCode.empty();

		// guest or default auth
		if(IsGuest)
		{
			pPlayer->GetSharedData().m_GuestLogin = GuestCredential;
			pGS->Chat(ClientID, "Press ESC and click 'Play' to continue as guest.");
			if(HasTimeoutCode)
			{
				pPlayer->GetSharedData().m_AuthType = AUTH_TYPE_TIMEOUT;
				pGS->Chat(ClientID, "Guest account is protected by timeout code.");
			}
			else
				pPlayer->GetSharedData().m_AuthType = AUTH_TYPE_GUEST;
		}
		else
		{
			pPlayer->GetSharedData().m_AuthType = AUTH_TYPE_LOGIN;
			pGS->Chat(ClientID, "Use /login <login> <pass> to sign in.");
		}
	});
}

// dump dialogs for translate
void CMmoController::SyncLocalizationsAsync() const
{
	Async::Run([this] { SyncLocalizations(); });
}

void CMmoController::SyncLocalizations() const
{
	// check action states
	std::unique_lock Lock(gs_LocalizationSyncMutex, std::try_to_lock);
	if(!Lock.owns_lock())
	{
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Wait the last operation is in progress..");
		return;
	}

	const int UpdatedLanguages = SyncLocalizationFiles();
	GS()->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Localization sync finished for %d language files.", UpdatedLanguages);
}


int CMmoController::SyncLocalizationFiles() const
{
	int UpdatedLanguages = 0;

	// update language data
	for(int i = 0; i < GS()->Server()->Localization()->m_pLanguages.size(); i++)
	{
		// prepare
		auto* pLanguage = GS()->Server()->Localization()->m_pLanguages[i];
		if(!pLanguage->Updater().Prepare())
		{
			GS()->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "sync_lines", "Skip language '%s': file is not available.", pLanguage->GetFilename());
			continue;
		}

		for(const auto& [ID, p] : QuestBotInfo::ms_aQuestBot)
		{
			int DialogNum = 0;
			for(const auto& pDialog : p.m_aDialogs)
			{
				pLanguage->Updater().Push(pDialog.GetText(), std::string("dialog_quest" + std::to_string(ID)).c_str(), DialogNum++);
			}
		}

		for(const auto& [ID, p] : NpcBotInfo::ms_aNpcBot)
		{
			int DialogNum = 0;
			for(const auto& pDialog : p.m_aDialogs)
			{
				pLanguage->Updater().Push(pDialog.GetText(), std::string("dialog_npc" + std::to_string(ID)).c_str(), DialogNum++);
			}
		}

		for(const auto& p : CAetherData::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "aether", p->GetID());
		}

		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			pLanguage->Updater().Push(pAttribute->GetName(), "attribute", (int)ID);
		}

		for(const auto& [ID, p] : CItemDescription::Data())
		{
			pLanguage->Updater().Push(p.GetName(), "item_name", ID);
			pLanguage->Updater().Push(p.GetDescription(), "item_description", ID);
		}

		for(const auto& [ID, p] : CSkillDescription::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "skill_name", ID);
			pLanguage->Updater().Push(p->GetDescription(), "skill_description", ID);
		}

		for(const auto& [ID, p] : CQuestDescription::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "quest_name", ID);
		}

		for(const auto& p : CWarehouse::Data())
		{
			pLanguage->Updater().Push(p->GetName(), "warehouse_name", p->GetID());
		}

		for(const auto& p : CHouse::Data())
		{
			pLanguage->Updater().Push(p->GetClassName(), "house_class_name", p->GetID());
		}

		// finish
		pLanguage->Updater().Finish();
		UpdatedLanguages++;
	}

	return UpdatedLanguages;
}


bool CMmoController::ReloadLocalizations() const
{
	std::unique_lock Lock(gs_LocalizationSyncMutex, std::try_to_lock);
	if(!Lock.owns_lock())
	{
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "reload_localization", "Wait the last localization operation is in progress..");
		return false;
	}

	const int UpdatedLanguages = SyncLocalizationFiles();
	if(!GS()->Server()->Localization()->Reload())
	{
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "reload_localization", "Failed to reload localization index.");
		return false;
	}

	GS()->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "reload_localization", "Localization files have been synchronized and reloaded (%d language files).", UpdatedLanguages);
	return true;
}
