/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mmo_controller.h"

#include <game/server/gamecontext.h>

#include "components/Accounts/AccountManager.h"
#include "components/Accounts/AccountMiningManager.h"
#include "components/Accounts/AccountFarmingManager.h"
#include "components/achievements/achievement_manager.h"
#include "components/auction/auction_manager.h"
#include "components/aethernet/aethernet_manager.h"
#include "components/Bots/BotManager.h"
#include "components/crafting/craft_manager.h"
#include "components/Dungeons/DungeonManager.h"
#include "components/Eidolons/EidolonManager.h"
#include "components/groups/group_manager.h"
#include "components/guilds/guild_manager.h"
#include "components/houses/house_manager.h"
#include "components/Inventory/InventoryManager.h"
#include "components/mails/mailbox_manager.h"
#include "components/quests/quest_manager.h"
#include "components/skills/skill_manager.h"
#include "components/tutorial/tutorial_manager.h"
#include "components/warehouse/warehouse_manager.h"
#include "components/worlds/world_manager.h"
#include <teeother/components/localization.h>

inline static void InsertUpgradesVotes(CPlayer* pPlayer, AttributeGroup Type, VoteWrapper* pWrapper)
{
	pWrapper->MarkList().Add("Total statistics:");
	{
		pWrapper->BeginDepth();
		for(auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->IsGroup(Type) && pAttribute->HasDatabaseField())
			{
				// if upgrades are cheap, they have a division of statistics
				const int AttributeSize = pPlayer->GetTotalAttributeValue(ID);

				// percent data TODO: extract percent attributes
				char aBuf[64] {};
				float Percent = pPlayer->GetAttributeChance(ID);
				if(Percent)
				{
					str_format(aBuf, sizeof(aBuf), "(%0.4f%%)", Percent);
				}
				pWrapper->Add("{} - {}{}", pAttribute->GetName(), AttributeSize, aBuf);
			}
		}
		pWrapper->EndDepth();
	}
	pWrapper->AddLine();
	pWrapper->MarkList().Add("Upgrading:");
	{
		pWrapper->BeginDepth();
		for(auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->IsGroup(Type) && pAttribute->HasDatabaseField())
				pWrapper->AddOption("UPGRADE", (int)ID, pAttribute->GetUpgradePrice(), "{} - {} (cost {} point)",
					pAttribute->GetName(), pPlayer->Account()->m_aStats[ID], pAttribute->GetUpgradePrice());
		}
		pWrapper->EndDepth();
	}
}

CMmoController::CMmoController(CGS* pGameServer) : m_pGameServer(pGameServer)
{
	// order
	m_System.add(m_pAchievementManager = new CAchievementManager);
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
	m_System.add(m_pAccountMiningManager = new CAccountMiningManager);
	m_System.add(m_pAccountFarmingManager = new CAccountFarmingManager);
	m_System.add(m_pMailboxManager = new CMailboxManager);

	// initialize components
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

	// update language data
	SyncLocalizations();
}

void CMmoController::OnTick()
{
	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnTick();

	// Check if the current tick is a multiple of the time period check time
	if(GS()->Server()->Tick() % ((GS()->Server()->TickSpeed() * 60) * g_Config.m_SvTimePeriodCheckTime) == 0)
		OnHandleTimePeriod();
}

bool CMmoController::OnClientMessage(int MsgID, void* pRawMsg, int ClientID)
{
	if(GS()->Server()->ClientIngame(ClientID) && GS()->GetPlayer(ClientID))
	{
		for(auto& pComponent : m_System.m_vComponents)
		{
			if(pComponent->OnClientMessage(MsgID, pRawMsg, ClientID))
				return true;
		}
	}

	return false;
}

void CMmoController::OnPlayerLogin(CPlayer* pPlayer)
{
	for(auto& pComponent : m_System.m_vComponents)
		pComponent->OnPlayerLogin(pPlayer);
}

bool CMmoController::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnSendMenuMotd(pPlayer, Menulist))
			return true;
	}
	return false;
}

bool CMmoController::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	// initialize variables
	int ClientID = pPlayer->GetCID();

	// main menu
	if(Menulist == MENU_MAIN)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// statistics menu
		const int ExpForLevel = computeExperience(pPlayer->Account()->GetLevel());
		const char* pStrLastLoginDate = pPlayer->Account()->GetLastLoginDate();
		VoteWrapper VMain(ClientID, VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE | VWF_SEPARATE, "Account info");
		VMain.Add("Last log in: {}", pStrLastLoginDate);
		VMain.Add("Level {}, Exp {}/{}", pPlayer->Account()->GetLevel(), pPlayer->Account()->GetExperience(), ExpForLevel);
		VMain.Add("Gold: {$}, Bank: {$}", pPlayer->Account()->GetGold(), pPlayer->Account()->GetBank());
		VMain.Add("Skill Point {}SP", pPlayer->GetItem(itSkillPoint)->GetValue());
		VoteWrapper::AddEmptyline(ClientID);

		// personal menu
		VoteWrapper VPersonal(ClientID, VWF_ALIGN_TITLE, "\u262A PERSONAL");
		VPersonal.AddMenu(MENU_INVENTORY, "\u205C Inventory");
		VPersonal.AddMenu(MENU_EQUIPMENT, "\u26B0 Equipment");
		VPersonal.AddMenu(MENU_UPGRADES, "\u2657 Upgrades({}p)", pPlayer->Account()->m_Upgrade);
		VPersonal.AddMenu(MENU_ACHIEVEMENTS, "\u2654 Achievements");
		VPersonal.AddMenu(MENU_EIDOLON_COLLECTION, "\u2727 Eidolon Collection");
		VPersonal.AddMenu(MENU_DUNGEONS, "\u262C Dungeons");
		VPersonal.AddMenu(MENU_GROUP, "\u2042 Group");
		VPersonal.AddMenu(MENU_SETTINGS, "\u2699 Settings");
		VPersonal.AddMenu(MENU_MAILBOX, "\u2709 Mailbox");
		VPersonal.AddMenu(MENU_JOURNAL_MAIN, "\u270D Journal");
		if(pPlayer->Account()->HasHouse())
			VPersonal.AddMenu(MENU_HOUSE, "\u2302 House");
		VPersonal.AddMenu(MENU_GUILD_FINDER, "\u20AA Guild finder");
		if(pPlayer->Account()->HasGuild())
			VPersonal.AddMenu(MENU_GUILD, "\u32E1 Guild");
		VoteWrapper::AddEmptyline(ClientID);

		// info menu
		VoteWrapper VInfo(ClientID, VWF_ALIGN_TITLE, "\u262A INFORMATION");
		VInfo.AddMenu(MENU_GUIDE_GRINDING, "\u10D3 Wiki / Grinding Guide ");
		VInfo.AddMenu(MENU_TOP_LIST, "\u21F0 Ranking guilds and players");
		return true;
	}

	// player upgrades
	if(Menulist == MENU_UPGRADES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		const char* paGroupNames[] = {
			Instance::Localize(ClientID, "\u2699 Disciple of Tank"),
			Instance::Localize(ClientID, "\u2696 Disciple of Healer"),
			Instance::Localize(ClientID, "\u2694 Disciple of War"),
			Instance::Localize(ClientID, "\u2690 Weapons / Ammo")
		};

		// information
		VoteWrapper VUpgrInfo(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Upgrades Information");
		VUpgrInfo.Add("You can upgrade your character's statistics.");
		VUpgrInfo.Add("Each update costs differently point.");
		VUpgrInfo.Add("You can get points by leveling up.");
		VUpgrInfo.Add("Upgrade Point's: {}P", pPlayer->Account()->m_Upgrade);
		VoteWrapper::AddEmptyline(ClientID);

		// upgrade group's
		const ClassGroup& Class = pPlayer->GetClass()->GetGroup();
		VoteWrapper VUpgrGroupSelect(ClientID, VWF_OPEN, "Select a type of upgrades");
		if(Class == ClassGroup::Dps)
			VUpgrGroupSelect.AddMenu(MENU_UPGRADES, (int)AttributeGroup::Dps, paGroupNames[(int)AttributeGroup::Dps]);
		else if(Class == ClassGroup::Tank)
			VUpgrGroupSelect.AddMenu(MENU_UPGRADES, (int)AttributeGroup::Tank, paGroupNames[(int)AttributeGroup::Tank]);
		else if(Class == ClassGroup::Healer)
			VUpgrGroupSelect.AddMenu(MENU_UPGRADES, (int)AttributeGroup::Healer, paGroupNames[(int)AttributeGroup::Healer]);
		VUpgrGroupSelect.AddMenu(MENU_UPGRADES, (int)AttributeGroup::Weapon, paGroupNames[(int)AttributeGroup::Weapon]);
		VoteWrapper::AddEmptyline(ClientID);

		// Upgrades by group
		if(pPlayer->m_VotesData.GetExtraID() >= 0)
		{
			auto Group = (AttributeGroup)clamp(pPlayer->m_VotesData.GetExtraID(), (int)AttributeGroup::Tank, (int)AttributeGroup::Weapon);
			const char* pGroupName = paGroupNames[(int)Group];

			VoteWrapper VUpgrGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "{} : Strength {}", pGroupName, pPlayer->GetTotalAttributesInGroup(Group));
			InsertUpgradesVotes(pPlayer, Group, &VUpgrGroup);
			VUpgrGroup.AddLine();
		}

		// Add back page
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// top list
	if(Menulist == MENU_TOP_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// information
		VoteWrapper VTopInfo(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE, "Top List Information");
		VTopInfo.Add("You can view the top 10 players and guilds.");
		VoteWrapper::AddEmptyline(ClientID);

		// select type list
		VoteWrapper VTopSelect(ClientID, VWF_OPEN, "Select a type of ranking");
		VTopSelect.AddMenu(MENU_TOP_LIST, (int)ToplistType::GUILDS_LEVELING, "Top 10 guilds leveling");
		VTopSelect.AddMenu(MENU_TOP_LIST, (int)ToplistType::GUILDS_WEALTHY, "Top 10 guilds wealthy");
		VTopSelect.AddMenu(MENU_TOP_LIST, (int)ToplistType::PLAYERS_LEVELING, "Top 10 players leveling");
		VTopSelect.AddMenu(MENU_TOP_LIST, (int)ToplistType::PLAYERS_WEALTHY, "Top 10 players wealthy");
		VoteWrapper::AddEmptyline(ClientID);

		// show top list
		VoteWrapper VTopList(ClientID, VWF_STYLE_SIMPLE|VWF_SEPARATE);
		if(const int& TemporaryInteger = pPlayer->m_VotesData.GetExtraID(); TemporaryInteger >= 0)
			ShowTopList(ClientID, (ToplistType)TemporaryInteger, 10, &VTopList);

		// backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// grinding guide
	if(Menulist == MENU_GUIDE_GRINDING)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// discord information
		VoteWrapper VDiscordInfo(ClientID, VWF_STYLE_STRICT_BOLD);
		VDiscordInfo.AddLine().Add("Discord: \"{}\"", g_Config.m_SvDiscordInviteLink).AddLine();

		// information
		VoteWrapper VGrindingInfo(ClientID, VWF_SEPARATE_CLOSED, "Grinding Information");
		VGrindingInfo.Add("You can look mob, farm, and mining point's.");
		VGrindingInfo.AddLine();

		// show all world's
		VoteWrapper VGrindingSelect(ClientID, VWF_SEPARATE_OPEN, "Select a zone to view information");
		for(int ID = MAIN_WORLD_ID; ID < GS()->Server()->GetWorldsSize(); ID++)
			VGrindingSelect.AddMenu(MENU_GUIDE_GRINDING_SELECT, ID, "{}", GS()->Server()->GetWorldName(ID));

		// add back page
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// grinding guide selected
	if(Menulist == MENU_GUIDE_GRINDING_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_GUIDE_GRINDING);

		const int WorldID = pPlayer->m_VotesData.GetExtraID();

		// ores information detail
		VoteWrapper VMiningPoints(ClientID, VWF_STYLE_STRICT_BOLD);
		VMiningPoints.AddLine().Add("Mining point's from ({})", Instance::Server()->GetWorldName(WorldID)).AddLine();
		if(!AccountMiningManager()->InsertItemsDetailVotes(pPlayer, WorldID))
			VoteWrapper(ClientID).Add("No mining point's in this world");
		VoteWrapper::AddEmptyline(ClientID);

		// farm information detail
		VoteWrapper VFarmingPoints(ClientID, VWF_STYLE_STRICT_BOLD);
		VFarmingPoints.AddLine().Add("Farming point's from ({})", Instance::Server()->GetWorldName(WorldID)).AddLine();
		if(!AccountFarmingManager()->InsertItemsDetailVotes(pPlayer, WorldID))
			VoteWrapper(ClientID).Add("No farm point's in this world");
		VoteWrapper::AddEmptyline(ClientID);

		// mobs information detail
		VoteWrapper VMobsPoints(ClientID, VWF_STYLE_STRICT_BOLD);
		VMobsPoints.AddLine().Add("Mob point's from ({})", Instance::Server()->GetWorldName(WorldID)).AddLine();
		if(!BotManager()->InsertItemsDetailVotes(pPlayer, WorldID))
			VoteWrapper(ClientID).Add("No mob point's in this world");
		VoteWrapper::AddEmptyline(ClientID);

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

bool CMmoController::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int ExtraValue1, const int ExtraValue2, int ReasonNumber, const char* pReason)
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

bool CMmoController::OnPlayerMotdCommand(CPlayer* pPlayer, const char* pCmd, int ExtraValue)
{
	if(!pPlayer)
		return true;

	for(auto& pComponent : m_System.m_vComponents)
	{
		if(pComponent->OnPlayerMotdCommand(pPlayer, pCmd, ExtraValue))
			return true;
	}
	return false;
}

void CMmoController::OnResetClientData(int ClientID)
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
	Utils::Files::Result Result = Utils::Files::loadFile("time_periods.cfg", &RawData);
	if(Result == Utils::Files::Result::ERROR_FILE)
	{
		// Save the data to the file "time_periods.cfg"
		std::string Data = std::to_string(CurrentTimeStamp) + "\n" + std::to_string(CurrentTimeStamp) + "\n" + std::to_string(CurrentTimeStamp);
		Utils::Files::saveFile("time_periods.cfg", Data.data(), (unsigned)Data.size());
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
		Utils::Files::saveFile("time_periods.cfg", Data.data(), (unsigned)Data.size());

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

void CMmoController::OnHandlePlayerTimePeriod(CPlayer* pPlayer)
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

	CAccountData* pAcc = pPlayer->Account();

	if(Table == SAVE_STATS)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Level = '%d', Exp = '%d', Bank = '%s' WHERE ID = '%d'", 
			pAcc->GetLevel(), pAcc->GetExperience(), pAcc->GetBank().to_string().c_str(), pAcc->GetID());
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
	else if(Table == SAVE_FARMING_DATA)
	{
		std::string Fields = pAcc->m_FarmingData.getUpdateField();
		Database->Execute<DB::UPDATE>("tw_accounts_farming", "%s WHERE UserID = '%d'", Fields.c_str(), pAcc->GetID());
	}
	else if(Table == SAVE_MINING_DATA)
	{
		std::string Fields = pAcc->m_MiningData.getUpdateField();
		Database->Execute<DB::UPDATE>("tw_accounts_mining", "%s WHERE UserID = '%d'", Fields.c_str(), pAcc->GetID());
	}
	else if(Table == SAVE_SOCIAL_STATUS)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "CrimeScore = '%d', PrisonSeconds = '%d' WHERE ID = '%d'",
			pAcc->GetCrimeScore(), pAcc->m_PrisonSeconds, pAcc->GetID());
	}
	else if(Table == SAVE_GUILD_DATA)
	{
		//
	}
	else if(Table == SAVE_POSITION)
	{
		const int LatestCorrectWorldID = AccountManager()->GetLastVisitedWorldID(pPlayer);
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
	else if(Table == SAVE_ACHIEVEMENTS)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Achievements = '%s' WHERE ID = '%d'", pAcc->GetAchievementsData().dump().c_str(), pAcc->GetID());
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

void CMmoController::ShowLoadingProgress(const char* pLoading, size_t Size) const
{
	char aLoadingBuf[128];
	str_format(aLoadingBuf, sizeof(aLoadingBuf), "[Loaded %d %s] :: WorldID %d.", (int)Size, pLoading, GS()->GetWorldID());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "LOAD DB", aLoadingBuf);
}

void CMmoController::ShowTopList(int ClientID, ToplistType Type, int Rows, VoteWrapper* pWrapper) const
{
	if(Type == ToplistType::GUILDS_LEVELING)
	{
		if(pWrapper)
			pWrapper->SetTitle("Top 10 guilds leveling");

		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Level DESC, Experience DESC LIMIT %d", Rows);
		while(pRes->next())
		{
			char NameGuild[64];
			const int Rank = pRes->getRow();
			const int Level = pRes->getInt("Level");
			const int Experience = pRes->getInt("Experience");
			str_copy(NameGuild, pRes->getString("Name").c_str(), sizeof(NameGuild));

			if(pWrapper)
				pWrapper->Add("{}. {} :: Level {} : Exp {}", Rank, NameGuild, Level, Experience);
			else
				GS()->Chat(ClientID, "{}. {} :: Level {} : Exp {}", Rank, NameGuild, Level, Experience);
		}
	}
	else if(Type == ToplistType::GUILDS_WEALTHY)
	{
		if(pWrapper)
			pWrapper->SetTitle("Top 10 guilds wealthy");

		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds", "ORDER BY Bank DESC LIMIT %d", Rows);
		while(pRes->next())
		{
			char NameGuild[64];
			const int Rank = pRes->getRow();
			const int Gold = pRes->getInt("Bank");
			str_copy(NameGuild, pRes->getString("Name").c_str(), sizeof(NameGuild));

			if(pWrapper)
				pWrapper->Add("{}. {} :: Gold {$}", Rank, NameGuild, Gold);
			else
				GS()->Chat(ClientID, "{}. {} :: Gold {$}", Rank, NameGuild, Gold);
		}
	}
	else if(Type == ToplistType::PLAYERS_LEVELING)
	{
		if(pWrapper)
			pWrapper->SetTitle("Top 10 players leveling");

		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_data", "ORDER BY Level DESC, Exp DESC LIMIT %d", Rows);
		while(pRes->next())
		{
			char Nick[64];
			const int Rank = pRes->getRow();
			const int Level = pRes->getInt("Level");
			const int Experience = pRes->getInt("Exp");
			str_copy(Nick, pRes->getString("Nick").c_str(), sizeof(Nick));

			if(pWrapper)
				pWrapper->Add("{}. {} :: Level {} : Exp {}", Rank, Nick, Level, Experience);
			else
				GS()->Chat(ClientID, "{}. {} :: Level {} : Exp {}", Rank, Nick, Level, Experience);
		}
	}
	else if(Type == ToplistType::PLAYERS_WEALTHY)
	{
		if(pWrapper)
			pWrapper->SetTitle("Top 10 players wealthy");

		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE ItemID = '%d' ORDER BY Value DESC LIMIT %d", (ItemIdentifier)itGold, Rows);
		while(pRes->next())
		{
			char Nick[64];
			BigInt Gold = pRes->getInt("Value");
			const int Rank = pRes->getRow();
			const int UserID = pRes->getInt("UserID");
			str_copy(Nick, Instance::Server()->GetAccountNickname(UserID), sizeof(Nick));

			if(pWrapper)
				pWrapper->Add("{}. {} :: Gold {$}", Rank, Nick, Gold);
			else
				GS()->Chat(ClientID, "{}. {} :: Gold {$}", Rank, Nick, Gold);
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
		CGS* pGS = (CGS*)Instance::Server()->GameServerPlayer(ClientID);

		if(!pRes->next())
		{
			pGS->Chat(ClientID, "You need to register using /register <login> <pass>!");
			pGS->Chat(-1, "Apparently, we have a new player, {}!", PlayerName.c_str());
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
