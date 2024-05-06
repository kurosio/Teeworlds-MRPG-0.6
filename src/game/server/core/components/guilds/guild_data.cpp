/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_data.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/mails/mail_wrapper.h>

CGS* CGuildData::GS() const { return (CGS*)Instance::GameServerPlayer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID); }

CGuildData::~CGuildData()
{
	delete m_pMembers;
	delete m_pLogger;
	delete m_pRanks;
	delete m_pBank;
}

bool CGuildData::Upgrade(int Type)
{
	// Check if the Type is within the valid range
	if(Type < UPGRADE_AVAILABLE_SLOTS || Type >= NUM_GUILD_UPGRADES)
		return false;

	// Check if the type is UPGRADE_AVAILABLE_SLOTS and the value of the first upgrade data is greater than or equal to MAX_GUILD_SLOTS
	auto* pUpgradeData = &m_UpgradesData(Type, 0);
	if(Type == UPGRADE_AVAILABLE_SLOTS && m_UpgradesData(Type, 0).m_Value >= MAX_GUILD_SLOTS)
		return false;

	// Check if the guild has enough money to spend on the upgrade
	int Price = GetUpgradePrice(Type);
	if(m_pBank->Spend(Price))
	{
		// Increase the value of the upgrade by 1
		pUpgradeData->m_Value += 1;
		Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "%s = '%d' WHERE ID = '%d'", pUpgradeData->getFieldName(), pUpgradeData->m_Value, m_ID);

		// Add and send a history entry for the upgrade
		m_pLogger->Add(LOGFLAG_UPGRADES_CHANGES, "'%s' upgraded to %d level", pUpgradeData->getDescription(), pUpgradeData->m_Value);
		GS()->ChatGuild(m_ID, "'{}' upgraded to {} level", pUpgradeData->getDescription(), pUpgradeData->m_Value);
		return true;
	}

	return false;
}

void CGuildData::AddExperience(int Experience)
{
	// Increase the guild's experience by the given amount
	m_Experience += Experience;

	// Variable to track if the guild's table needs to be updated
	bool UpdateTable = false;

	// Check if the guild's experience is enough to level up
	int ExperienceNeed = (int)computeExperience(m_Level);
	while(m_Experience >= ExperienceNeed)
	{
		// Increase the guild's level & subtract the experience needed to level up from the guild's experience
		m_Experience -= ExperienceNeed;
		m_Level++;

		// Calculate the new experience needed to level up
		ExperienceNeed = (int)computeExperience(m_Level);

		// Add a history and send a chat message to the server indicating the guild's level up
		GS()->Chat(-1, "Guild {} raised the level up to {}", GetName(), m_Level);
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Guild raised level to '%d'.", m_Level);
		UpdateTable = true;
	}

	// Check if a random chance or the need to update the table is met
	if(rand() % 10 == 2 || UpdateTable)
	{
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '%d', Experience = '%d' WHERE ID = '%d'", m_Level, m_Experience, m_ID);
	}
}

GUILD_RESULT CGuildData::SetNewLeader(int AccountID)
{
	// Check if the given AccountID is already the leader of the guild
	if(AccountID == m_LeaderUID)
	{
		return GUILD_RESULT::SET_LEADER_PLAYER_ALREADY_LEADER;
	}

	// Check if the given AccountID is a member of the guild
	if(!m_pMembers->Get(AccountID))
	{
		return GUILD_RESULT::SET_LEADER_NON_GUILD_PLAYER;
	}

	// Update data
	m_LeaderUID = AccountID;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LeaderUID = '%d' WHERE ID = '%d'", m_LeaderUID, m_ID);

	// Get the nickname of the new guild leader
	const char* pNickNewLeader = Instance::Server()->GetAccountNickname(m_LeaderUID);

	// Add a new entry and send chat message to the guild history indicating the change of leader
	m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "New guild leader '%s'", pNickNewLeader);
	GS()->ChatGuild(m_ID, "New guild leader '{}'", pNickNewLeader);

	// Return a success code
	return GUILD_RESULT::SUCCESSFUL;
}

GUILD_RESULT CGuildData::BuyHouse(int HouseID)
{
	// Check if the guild already has a house
	if(m_pHouse != nullptr)
	{
		return GUILD_RESULT::BUY_HOUSE_ALREADY_HAVE;
	}

	// Find the house data with the specified HouseID
	auto IterHouse = std::find_if(CGuildHouseData::Data().begin(), CGuildHouseData::Data().end(), [&HouseID](const CGuildHouseData* p)
	{
		return p->GetID() == HouseID;
	});

	// Check if the house data was found
	if(IterHouse == CGuildHouseData::Data().end())
	{
		return GUILD_RESULT::BUY_HOUSE_UNAVAILABLE;
	}

	// Retrieve the price of the house from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HOUSES, "WHERE ID = '%d'", HouseID);
	if(pRes->next())
	{
		const int Price = pRes->getInt("Price");

		// Check if the guild has enough gold to purchase the house
		if(!GetBank()->Spend(Price))
		{
			return GUILD_RESULT::BUY_HOUSE_NOT_ENOUGH_GOLD;
		}

		// Assign the house to the guild
		m_pHouse = *IterHouse;
		m_pHouse->UpdateGuild(this);

		// Add a history entry and send a chat message for getting a guild house
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Your guild has purchased a house!");
		GS()->ChatGuild(m_ID, "Your guild has purchased a house!");

		// Update the GuildID of the house in the database
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = '%d' WHERE ID = '%d'", m_ID, HouseID);
		return GUILD_RESULT::SUCCESSFUL; // Return success code
	}

	return GUILD_RESULT::BUY_HOUSE_ALREADY_PURCHASED;
}

bool CGuildData::SellHouse()
{
	// Check if the guild house is not null
	if(m_pHouse != nullptr)
	{
		// Send an inbox message to the guild leader
		const int ReturnedGold = m_pHouse->GetPrice();

		// Send mail
		MailWrapper Mail("System", m_LeaderUID, "Guild house is sold.");
		Mail.AddDescLine("We returned some gold from your guild.");
		Mail.AttachItem(CItem(itGold, ReturnedGold));
		Mail.Send();

		// Add a history entry and send a chat message for losing a guild house
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Lost a house on '%s'.", Server()->GetWorldName(m_pHouse->GetWorldID()));
		GS()->ChatGuild(m_ID, "House sold, {}gold returned to leader", ReturnedGold);

		// Update the database to remove the guild ID from the guild house
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = NULL WHERE ID = '%d'", m_pHouse->GetID());

		// Reset house pointers
		m_pHouse->GetDoorManager()->CloseAll();
		m_pHouse->UpdateGuild(nullptr);
		m_pHouse = nullptr;
		return true;
	}

	return false;
}

void CGuildData::TimePeriodEvent(TIME_PERIOD Period)
{
	if(Period == TIME_PERIOD::DAILY_STAMP)
	{
		// Check if the guild has a house
		if(m_pHouse != nullptr)
		{
			// Check if the guild has enough money to pay the rent
			if(!m_pBank->Spend(m_pHouse->GetRentPrice()) && SellHouse())
			{
				// Send a chat message and log to the guild notifying them that their guild house rent has been paid
				GS()->ChatGuild(m_ID, "Your guild house rent has expired, has been sold.");
				m_pLogger->Add(LOGFLAG_HOUSE_MAIN_CHANGES, "House rent has expired, has been sold.");
				GS()->UpdateVotesIfForAll(MENU_GUILD);
				return;
			}

			// Send a chat message and log to the guild notifying them that their guild house rent has been paid
			GS()->ChatGuild(m_ID, "Your guild house rent has been paid.");
			m_pLogger->Add(LOGFLAG_HOUSE_MAIN_CHANGES, "House rent has been paid.");
		}
	}
	else if(Period == TIME_PERIOD::WEEK_STAMP)
	{
		// Reset the deposits of the guild members
		m_pMembers->ResetDeposits();

		// Send a chat message to the guild members
		GS()->ChatGuild(m_ID, "Membership deposits have been reset.");
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Membership deposits have been reset.");
	}
}

bool CGuildData::StartWar(CGuildData* pTargetGuild)
{
	if(!pTargetGuild || pTargetGuild->GetWar() || GetWar())
		return false;
	
	CGuildWarHandler* pWarHandler = CGuildWarHandler::CreateElement();
	time_t TimeUntilEnd = time(nullptr) + (g_Config.m_SvGuildWarDuration * 60);
	pWarHandler->Init({ this, pTargetGuild, 0 }, { pTargetGuild, this, 0 }, TimeUntilEnd);
	return true;
}

int CGuildData::GetUpgradePrice(int Type)
{
	// Check if the Type is within the valid range
	if(Type < UPGRADE_AVAILABLE_SLOTS || Type >= NUM_GUILD_UPGRADES)
		return 0;

	// Return the calculated price
	return m_UpgradesData(Type, 0).m_Value * (Type == UPGRADE_AVAILABLE_SLOTS ? g_Config.m_SvPriceUpgradeGuildSlot : g_Config.m_SvPriceUpgradeGuildAnother);
}

bool CGuildData::IsAccountMemberGuild(int AccountID)
{
	return std::any_of(CGuildData::Data().begin(), CGuildData::Data().end(), [&AccountID](const CGuildData* p)
	{
		return p->GetMembers()->Get(AccountID) != nullptr;
	});
}

/*
 * -------------------------------------
 * Bank impl
 * -------------------------------------
 */
CGS* CGuildData::CBank::GS() const { return m_pGuild->GS(); }

void CGuildData::CBank::Set(int Value)
{
	m_Bank = Value;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
}

bool CGuildData::CBank::Spend(int Value)
{
	// Retrieve the current bank value from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILDS_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		int Bank = pRes->getInt("Bank");
		if(Bank >= Value)
		{
			// Update the bank value and update the database
			m_Bank = Bank - Value;
			Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
			return true;
		}
	}

	return false;
}


/*
 * -------------------------------------
 * Logger impl
 * -------------------------------------
 */
CGuildData::CLogEntry::CLogEntry(CGuildData* pGuild, int64_t Logflag) : m_pGuild(pGuild)
{
	m_Logflag = Logflag < 0 ? (int64_t)LOGFLAG_GUILD_FULL : Logflag;

	CGuildData::CLogEntry::InitLogs();
}

void CGuildData::CLogEntry::SetActivityFlag(int64_t Flag)
{
	if(Flag & m_Logflag)
		m_Logflag &= ~Flag;
	else
		m_Logflag |= Flag;

	// Update the log flag in the database for the guild
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LogFlag = '%d' WHERE ID = '%d'", m_Logflag, m_pGuild->GetID());
}

bool CGuildData::CLogEntry::IsActivityFlagSet(int64_t Flag) const
{
	// Return true if the log flag is less than or equal to 0, or if the given flag is set in the log flag, or if the guild full flag is set in the log flag
	return (Flag & m_Logflag);
}

void CGuildData::CLogEntry::Add(int64_t LogFlag, const char* pBuffer, ...)
{
	if(LogFlag & m_Logflag)
	{
		// Initialize the variable argument list
		char aBuf[512];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
#if defined(CONF_FAMILY_WINDOWS)
		_vsnprintf(aBuf, sizeof(aBuf), pBuffer, VarArgs);
#else
		vsnprintf(aBuf, sizeof(aBuf), pBuffer, VarArgs);
#endif
		va_end(VarArgs);

		// If the number of logs exceeds the maximum limit, remove the oldest log
		if(m_aLogs.size() >= MAX_GUILD_LOGS_NUM)
		{
			m_aLogs.pop_front();
		}

		// Get the current timestamp and store it in aBufTimeStamp
		char aBufTimeStamp[64];
		str_timestamp(aBufTimeStamp, sizeof(aBufTimeStamp));

		// Add the formatted string and timestamp to the logs list
		const sqlstr::CSqlString<64> cBuf = sqlstr::CSqlString<64>(aBuf);
		m_aLogs.push_back({ cBuf.cstr(), aBufTimeStamp });

		// Execute an SQL INSERT statement to store the log in the database
		Database->Execute<DB::INSERT>(TW_GUILDS_HISTORY_TABLE, "(GuildID, Text, Time) VALUES ('%d', '%s', '%s')", m_pGuild->GetID(), cBuf.cstr(), aBufTimeStamp);
	}
}

void CGuildData::CLogEntry::InitLogs()
{
	// Execute a select query to fetch guild logs from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '%d' ORDER BY ID DESC LIMIT %d", m_pGuild->GetID(), (int)MAX_GUILD_LOGS_NUM);
	while(pRes->next())
	{
		// Create a log object and populate it with time and text values from the database
		m_aLogs.push_back({ pRes->getString("Text").c_str(), pRes->getString("Time").c_str() });
	}
}

/* -------------------------------------
 * Ranks impl
 * ------------------------------------- */
CGS* CGuildData::CRank::GS() const { return m_pGuild->GS(); }

CGuildData::CRank::CRank(GuildRankIdentifier RID, std::string&& Rank, GuildRankAccess Access, CGuildData* pGuild) : m_ID(RID), m_Rank(std::move(Rank))
{
	m_Access = Access;
	m_pGuild = pGuild;
}

GUILD_RESULT CGuildData::CRank::Rename(std::string NewRank)
{
	// Create a CSqlString object from the NewRank string
	auto cstrNewRank = CSqlString<64>(NewRank.c_str());

	// Check if the length of the string is less than 2 or greater than 16
	const int LengthRank = str_length(cstrNewRank.cstr());
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
	{
		// If the length is not within the valid range, return the result RANK_WRONG_NUMBER_OF_CHAR_IN_NAME
		return GUILD_RESULT::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME;
	}

	// Check if the new rank name is already used by another rank in the guild
	if(std::count_if(m_pGuild->GetRanks()->GetContainer().begin(), m_pGuild->GetRanks()->GetContainer().end(),
		[&cstrNewRank](const CRank* pRank) {return str_comp(pRank->m_Rank.c_str(), cstrNewRank.cstr()) == 0; }))
	{
		return GUILD_RESULT::RANK_RENAME_ALREADY_NAME_EXISTS;
	}

	// Update the name of the guild rank
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "renamed rank '%s' to '%s'", m_Rank.c_str(), cstrNewRank.cstr());
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Name = '%s' WHERE ID = '%d'", cstrNewRank.cstr(), m_ID);
	m_Rank = cstrNewRank.cstr();
	return GUILD_RESULT::RANK_SUCCESSFUL;
}

void CGuildData::CRank::ChangeAccess()
{
	if(m_Access >= RIGHTS_FULL)
		SetAccess(RIGHTS_DEFAULT);
	else
		SetAccess((GuildRankAccess)(m_Access + 1));
}

void CGuildData::CRank::SetAccess(GuildRankAccess Access)
{
	// Set the access level, clamping it between RIGHTS_DEFAULT and RIGHTS_FULL
	m_Access = (GuildRankAccess)clamp((int)Access, (int)RIGHTS_DEFAULT, (int)RIGHTS_FULL);

	// Save the updated access level in the database
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Access = '%d' WHERE ID = '%d'", m_Access, m_ID);

	// Send a chat message to the guild with the updated access level
	GS()->ChatGuild(GuildID, "Rank '{}' new rights '{}'!", m_Rank.c_str(), GetAccessName());
}

const char* CGuildData::CRank::GetAccessName() const
{
	if(m_Access == RIGHTS_INVITE_KICK)
		return "Invite & kick";
	if(m_Access == RIGHTS_UPGRADES_HOUSE)
		return "Upgrade & house door's";
	if(m_Access == RIGHTS_FULL)
		return "Full";
	return "Default";
}

/* -------------------------------------
 * Ranks manager impl
 * ------------------------------------- */
CGS* CGuildData::CRanksManager::GS() const { return m_pGuild->GS(); }

CGuildData::CRanksManager::CRanksManager(CGuildData* pGuild, GuildRankIdentifier DefaultID)
	: m_pGuild(pGuild)
{
	// Initialize the guild ranks controller
	CGuildData::CRanksManager::Init(DefaultID);
}

CGuildData::CRanksManager::~CRanksManager()
{
	// Delete all the rank data objects
	for(auto p : m_aRanks)
		delete p;

	// Clear the default rank pointer and the ranks vector
	m_pDefaultRank = nullptr;
	m_aRanks.clear();
}

void CGuildData::CRanksManager::Init(GuildRankIdentifier DefaultID)
{
	// Execute a database query to get the rank data for the guild
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		// Get the rank ID, name, and access level from the database query result
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		GuildRankAccess Access = (GuildRankAccess)pRes->getInt("Access");

		// Create a new CRank object and add it to the ranks vector
		if(DefaultID == RID)
		{
			m_pDefaultRank = m_aRanks.emplace_back(new CRank(RID, std::forward<std::string>(Rank), RIGHTS_DEFAULT, m_pGuild));
		}
		else
		{
			m_aRanks.emplace_back(new CRank(RID, std::forward<std::string>(Rank), Access, m_pGuild));
		}
	}
}

void CGuildData::CRanksManager::UpdateDefaultRank()
{
	// If the default rank already exists, return
	if(m_pDefaultRank)
		return;

	// If there are no ranks, create a default rank called "Member"
	if(!m_aRanks.empty())
	{
		m_pDefaultRank = m_aRanks.back();
		m_pDefaultRank->SetAccess(RIGHTS_DEFAULT);
	}
	else
	{
		GUILD_RESULT Result = Add("Newbie");
		dbg_assert(Result == GUILD_RESULT::RANK_SUCCESSFUL, "guild cannot initialize a default rank");
		m_pDefaultRank = Get("Newbie");
	}

	// Set the default rank for all guild members who do not have a rank
	for(auto& pIterMember : m_pGuild->GetMembers()->GetContainer())
	{
		auto pMember = pIterMember.second;
		if(!pMember->GetRank())
		{
			bool Status = pMember->SetRank(m_pDefaultRank);
			dbg_assert(Status, "guild cannot set a default rank for member");
		}
	}

	// Save the guild members
	m_pGuild->GetMembers()->Save();
}

GUILD_RESULT CGuildData::CRanksManager::Add(const std::string& Rank)
{
	// Create a CSqlString object for the rank name
	auto cstrRank = CSqlString<64>(Rank.c_str());

	// Check if the length of the string is less than 2 or greater than 16
	const int LengthRank = str_length(cstrRank.cstr());
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
	{
		// If the length is not within the valid range, return the result RANK_WRONG_NUMBER_OF_CHAR_IN_NAME
		return GUILD_RESULT::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME;
	}

	// Check if the rank already exists
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CRank* pRank) { return std::string(pRank->GetName()) == cstrRank.cstr(); }))
	{
		return GUILD_RESULT::RANK_ADD_ALREADY_EXISTS;
	}

	// Check if the rank count has reached the limit
	if((int)m_aRanks.size() >= MAX_GUILD_RANK_NUM)
	{
		return GUILD_RESULT::RANK_ADD_LIMIT_HAS_REACHED;
	}

	// Get the ID for the new rank
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1;

	// Insert the new rank into the database
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, Access, GuildID, Name) VALUES ('%d', '%d', '%d', '%s')", InitID, (int)RIGHTS_DEFAULT, GuildID, cstrRank.cstr());
	m_aRanks.emplace_back(new CRank(InitID, cstrRank.cstr(), RIGHTS_DEFAULT, m_pGuild));

	// Send information to the game server and update the guild history
	GS()->ChatGuild(GuildID, "New rank is created [{}]!", cstrRank.cstr());
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "added rank '%s'", cstrRank.cstr());
	return GUILD_RESULT::RANK_SUCCESSFUL;
}

GUILD_RESULT CGuildData::CRanksManager::Remove(const std::string& Rank)
{
	// Create a CSqlString object for the rank name
	auto cstrRank = CSqlString<64>(Rank.c_str());

	// Find the rank in the ranks vector
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CRank* pRank)
	{
		return str_comp(pRank->GetName(), cstrRank.cstr()) == 0;
	});

	// If the rank is the default rank, return
	if(*Iter == m_pDefaultRank)
	{
		return GUILD_RESULT::RANK_REMOVE_IS_DEFAULT;
	}

	// If the rank does not exist, return
	if(Iter == m_aRanks.end())
	{
		return GUILD_RESULT::RANK_REMOVE_DOES_NOT_EXIST;
	}

	// Set the default rank for all guild members who have the rank being removed
	for(auto& pMember : m_pGuild->GetMembers()->GetContainer())
	{
		if((*Iter)->GetID() == pMember.second->GetRank()->GetID())
		{
			bool Status = pMember.second->SetRank(m_pDefaultRank->GetID());
			dbg_assert(Status, "guild cannot set a default rank for member");
		}
	}

	// Remove the rank from the database and delete the rank data object
	Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '%d'", (*Iter)->GetID());
	delete (*Iter);
	m_aRanks.erase(Iter);

	// Send information to the game server and update the guild history
	GS()->ChatGuild(m_pGuild->GetID(), "Rank [{}] succesful delete", cstrRank.cstr());
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "removed rank '%s'", cstrRank.cstr());
	return GUILD_RESULT::RANK_SUCCESSFUL;
}

CGuildData::CRank* CGuildData::CRanksManager::Get(const std::string& Rank) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CRank* pRank){ return pRank->GetName() == Rank; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}

CGuildData::CRank* CGuildData::CRanksManager::Get(GuildRankIdentifier ID) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [ID](const CRank* pRank){ return pRank->GetID() == ID; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}

/* -------------------------------------
 * Members impl
 * ------------------------------------- */
CGS* CGuildData::CMember::GS() const { return m_pGuild->GS(); }

CGuildData::CMember::CMember(CGuildData* pGuild, int AccountID, CRank* pRank, int Deposit) : m_pGuild(pGuild)
{
	m_AccountID = AccountID;
	m_Deposit = Deposit;

	// If the given rank is null, set the member's rank to the default rank of the guild
	m_pRank = pRank == nullptr ? pGuild->GetRanks()->GetDefaultRank() : pRank;
}

CGuildData::CMember::~CMember()
{
	// Reinitialize the guild for the player's account
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID))
	{
		pPlayer->Account()->ReinitializeGuild(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
}

bool CGuildData::CMember::SetRank(GuildRankIdentifier RankID)
{
	// Get the rank object from the guild's ranks using the given rank ID
	CRank* pRank = m_pGuild->GetRanks()->Get(RankID);
	if(!pRank)
		return false;

	// Set the member's rank to the given rank
	return SetRank(pRank);
}

bool CGuildData::CMember::SetRank(CRank* pRank)
{
	// Check if the rank is valid
	if(!pRank)
		return false;

	// Set the member's rank
	m_pRank = pRank;

	// Send a chat message to the player about changed rank
	const char* pNickname = Instance::Server()->GetAccountNickname(m_AccountID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "%s rank changed to %s", pNickname, m_pRank->GetName());
	GS()->ChatGuild(m_pGuild->GetID(), "'{}' rank changed to '{}'!", pNickname, m_pRank->GetName());

	// Save the guild data
	m_pGuild->GetMembers()->Save();
	return true;
}

bool CGuildData::CMember::DepositInBank(int Golds)
{
	// Get the player object of the member
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	// Get the bank value from the guild table in the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILDS_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		// If the player has enough gold to deposit
		if(pPlayer->Account()->SpendCurrency(Golds))
		{
			// Increase the member's deposit and the guild bank value
			m_Deposit += Golds;
			m_pGuild->GetBank()->Set(pRes->getInt("Bank") + Golds);
			Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_pGuild->GetBank()->Get(), m_pGuild->GetID());

			// Send a chat message to the player indicating the successful deposit and the new bank value
			const char* pNickname = Instance::Server()->GetAccountNickname(m_AccountID);
			m_pGuild->GetLogger()->Add(LOGFLAG_BANK_CHANGES, "'%s' deposit '%d' in the guild safe.", pNickname, Golds);
			GS()->ChatGuild(m_pGuild->GetID(), "'{}' deposit {} gold in the safe, now {}!", pNickname, Golds, m_pGuild->GetBank()->Get());

			// Save guild data
			m_pGuild->GetMembers()->Save();
			return true;
		}
	}

	return false;
}

bool CGuildData::CMember::WithdrawFromBank(int Golds)
{
	// Get the player object of the member
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	// Get the bank value from the guild table in the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILDS_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		int Bank = pRes->getInt("Bank");

		// Make sure the requested withdrawal amount is not greater than the available bank value
		Golds = minimum(Golds, Bank);
		if(Golds > 0)
		{
			// Decrease the member's deposit, add the withdrawn gold to the player's account, and decrease the guild bank value
			m_Deposit -= Golds;
			pPlayer->Account()->AddGold(Golds);
			m_pGuild->GetBank()->Set(Bank - Golds);

			// Send a chat message to the player indicating the successful withdrawal and the new bank value
			const char* pNickname = Instance::Server()->GetAccountNickname(m_AccountID);
			m_pGuild->GetLogger()->Add(LOGFLAG_BANK_CHANGES, "'%s' withdrawn '%d' from the guild safe.", pNickname, Golds);
			GS()->ChatGuild(m_pGuild->GetID(), "'{}' withdrawn {} gold from the safe, now {}!", pNickname, Golds, m_pGuild->GetBank()->Get());

			// Save guild data
			m_pGuild->GetMembers()->Save();
			return true;
		}
	}

	return false;
}

bool CGuildData::CMember::CheckAccess(GuildRankAccess RequiredAccess) const
{
	return (m_pGuild->GetLeaderUID() == m_AccountID || m_pRank->GetAccess() == RequiredAccess
		|| (m_pRank->GetAccess() == RIGHTS_FULL && RequiredAccess != RIGHTS_LEADER));
}

/* -------------------------------------
 * Members manager impl
 * ------------------------------------- */
CGS* CGuildData::CMembersManager::GS() const { return m_pGuild->GS(); }

CGuildData::CMembersManager::CMembersManager(CGuildData* pGuild, std::string&& MembersData) : m_pGuild(pGuild)
{
	// Create a new instance of CRequestsManager with pGuild as the parameter
	m_pRequests = new CRequestsManager(pGuild);

	// Initialize CGuildMembersManager with MembersData using std::move to transfer ownership
	CGuildData::CMembersManager::Init(std::move(MembersData));
}

CGuildData::CMembersManager::~CMembersManager()
{
	// Delete all member data objects
	for(auto pIterMember : m_apMembers)
	{
		delete pIterMember.second;
		pIterMember.second = nullptr;
	}

	delete m_pRequests;
	m_apMembers.clear();
}

GUILD_RESULT CGuildData::CMembersManager::Join(int AccountID)
{
	// Check if the member is already in the guild
	if(m_apMembers.find(AccountID) != m_apMembers.end() || CGuildData::IsAccountMemberGuild(AccountID))
	{
		return GUILD_RESULT::MEMBER_JOIN_ALREADY_IN_GUILD;
	}

	// Check if there are free slots available in the guild
	if(!HasFreeSlots())
	{
		return GUILD_RESULT::MEMBER_NO_AVAILABLE_SLOTS;
	}

	// Create a new guild member data and add it to the guild members map
	m_apMembers[AccountID] = new CMember(m_pGuild, AccountID, m_pGuild->GetRanks()->GetDefaultRank());

	// Reinitialize the guild for the player if they are online
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
	{
		pPlayer->Account()->ReinitializeGuild();
	}

	// Add a join message to the guild history and send chat message
	const char* pNickname = Instance::Server()->GetAccountNickname(AccountID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' has joined the guild.", pNickname);
	GS()->ChatGuild(m_pGuild->GetID(), "'{}' has joined the guild!", pNickname);

	// Save the guild members data
	Save();
	return GUILD_RESULT::MEMBER_SUCCESSFUL;
}

GUILD_RESULT CGuildData::CMembersManager::Kick(int AccountID)
{
	// Check if the player is the guild leader
	if(m_pGuild->GetLeaderUID() == AccountID)
	{
		return GUILD_RESULT::MEMBER_KICK_IS_OWNER;
	}

	// Check if the player is a member of the guild
	if(auto Iter = m_apMembers.find(AccountID); Iter != m_apMembers.end())
	{
		// Delete the member data object and remove it from the member list
		delete (*Iter).second;
		m_apMembers.erase(Iter);

		// Reinitialize the guild for the player
		if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
		{
			pPlayer->Account()->ReinitializeGuild();
			pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		}

		// Add a left message to the guild history and send chat message
		const char* pNickname = Instance::Server()->GetAccountNickname(AccountID);
		m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' has left the guild.", pNickname);
		GS()->ChatGuild(m_pGuild->GetID(), "'{}' has left the guild!", pNickname);

		// Save the guild data
		Save();
		return GUILD_RESULT::MEMBER_SUCCESSFUL;
	}

	return GUILD_RESULT::MEMBER_KICK_DOES_NOT_EXIST;
}

bool CGuildData::CMembersManager::HasFreeSlots() const
{
	return (int)m_apMembers.size() < m_pGuild->GetUpgrades(CGuildData::UPGRADE_AVAILABLE_SLOTS)->getValue();
}

std::pair<int, int> CGuildData::CMembersManager::GetCurrentSlots() const
{
	return std::pair((int)m_apMembers.size(), m_pGuild->GetUpgrades(CGuildData::UPGRADE_AVAILABLE_SLOTS)->getValue());
}

void CGuildData::CMembersManager::ResetDeposits()
{
	// Iterate through each member in the member list
	for(auto& [UID, pMember] : m_apMembers)
	{
		// Set the deposit amount for the member to 0
		pMember->SetDeposit(0);
	}

	// Save the updated member list
	Save();
}

int CGuildData::CMembersManager::GetOnlinePlayersCount() const
{
	int Count = 0;
	for(auto& [UID, pMember] : m_apMembers)
	{
		if(GS()->GetPlayerByUserID(UID))
		{
			Count++;
		}
	}
	return Count;
}

void CGuildData::CMembersManager::Init(std::string&& MembersData)
{
	// Assert by empty
	dbg_assert(m_apMembers.empty(), "");

	// Parse the JSON string
	Tools::Json::parseFromString(MembersData, [this](nlohmann::json& pJson)
	{
		for(auto& pMember : pJson["members"])
		{
			// Get the member ID
			int UID = pMember.value("id", -1);

			// Check if the member ID is valid and not already in the member list
			if(UID > 0 && m_apMembers.find(UID) == m_apMembers.end())
			{
				// Get the rank ID and deposit for the member
				int RID = pMember.value("rank_id", -1);
				int Deposit = pMember.value("deposit", 0);

				// Create a new member data object and add it to the member list
				m_apMembers[UID] = (new CMember(m_pGuild, UID, m_pGuild->GetRanks()->Get(RID), Deposit));
			}
		}
	});
}

void CGuildData::CMembersManager::Save() const
{
	// Create a JSON object for the member data
	nlohmann::json MembersData;
	for(auto& [UID, pMember] : m_apMembers)
	{
		nlohmann::json memberData;
		memberData["id"] = UID;
		memberData["rank_id"] = pMember->GetRank()->GetID();
		memberData["deposit"] = pMember->GetDeposit();
		MembersData["members"].push_back(memberData);
	}

	// Update the guild data in the database
	Database->Execute<DB::UPDATE, 300>(TW_GUILDS_TABLE, "DefaultRankID = '%d', Members = '%s' WHERE ID = '%d'",
		m_pGuild->GetRanks()->GetDefaultRank()->GetID(), MembersData.dump().c_str(), m_pGuild->GetID());
}

CGuildData::CMember* CGuildData::CMembersManager::Get(int AccountID)
{
	return m_apMembers.find(AccountID) != m_apMembers.end() ? m_apMembers[AccountID] : nullptr;
}


/* -------------------------------------
 * Requests member manager impl
 * ------------------------------------- */
CGS* CGuildData::CRequestsManager::GS() const { return m_pGuild->GS(); }

CGuildData::CRequestsManager::CRequestsManager(CGuildData* pGuild) : m_pGuild(pGuild)
{
	CRequestsManager::Init();
}

CGuildData::CRequestsManager::~CRequestsManager()
{
	for(auto p : m_aRequestsJoin)
		delete p;

	m_aRequestsJoin.clear();
}

void CGuildData::CRequestsManager::Init()
{
	// Execute a database query to get the rank data for the guild
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		// Get the rank ID, name, and access level from the database query result
		int AccountID = pRes->getInt("UserID");
		m_aRequestsJoin.push_back(new RequestData(AccountID));
	}
}

GUILD_RESULT CGuildData::CRequestsManager::Request(int FromUID)
{
	// Check if the invite already exists in the guild's invites container
	if(std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(),
		[&FromUID](const RequestData* p){ return p->GetFromUID() == FromUID; }) != m_aRequestsJoin.end())
		return GUILD_RESULT::MEMBER_REQUEST_ALREADY_SEND;

	// Check if the guild's member list has free slots
	if(!m_pGuild->GetMembers()->HasFreeSlots())
	{
		return GUILD_RESULT::MEMBER_NO_AVAILABLE_SLOTS;
	}

	// Add the invite to the guild's history and send a chat message
	const char* pFromNickname = Instance::Server()->GetAccountNickname(FromUID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "invitation to join from '%s'.", pFromNickname);
	GS()->ChatGuild(m_pGuild->GetID(), "invitation to join from '{}'.", pFromNickname);

	// Create a new invite data object and add it to the guild's invites container
	m_aRequestsJoin.push_back(new RequestData(FromUID));

	// Insert the invite into the database
	Database->Execute<DB::INSERT>(TW_GUILDS_INVITES_TABLE, "(GuildID, UserID) VALUES ('%d', '%d')", m_pGuild->GetID(), FromUID);
	return GUILD_RESULT::MEMBER_SUCCESSFUL;
}

GUILD_RESULT CGuildData::CRequestsManager::Accept(int UserID, const CMember* pFromMember)
{
	// Find the request with the given UserID in the m_aRequestsJoin vector
	auto Iter = std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&UserID](const RequestData* pRequest)
	{
		return pRequest->GetFromUID() == UserID;
	});

	// If the request is not found, return an undefined error
	if(Iter == m_aRequestsJoin.end())
	{
		return GUILD_RESULT::MEMBER_UNDEFINED_ERROR;
	}

	// Remove the request from the database and delete it from memory
	Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d' AND UserID = '%d'", m_pGuild->GetID(), (*Iter)->GetFromUID());
	delete (*Iter);
	m_aRequestsJoin.erase(Iter);

	// Try to add the user to the guild as a member if the user is successfully added as a member and pFromMember is not null
	GUILD_RESULT Result = m_pGuild->GetMembers()->Join(UserID);
	if(Result == GUILD_RESULT::MEMBER_SUCCESSFUL && pFromMember)
	{
		// Get the nicknames of the users
		const char* pFromNickname = Instance::Server()->GetAccountNickname(UserID);
		const char* pByNickname = Instance::Server()->GetAccountNickname(pFromMember->GetAccountID());

		// Add a history entry and send a guild chat message
		m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' accepted invitation from '%s'.", pByNickname, pFromNickname);
		GS()->ChatGuild(m_pGuild->GetID(), "'{}' accepted invitation from '{}'.", pByNickname, pFromNickname);
	}

	// Return the result of adding the user as a member
	return Result;
}

void CGuildData::CRequestsManager::Deny(int UserID, const CMember* pFromMember)
{
	// Find the request in m_aRequestsJoin that matches the UserID
	auto Iter = std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&UserID](const RequestData* pRank)
	{
		return pRank->GetFromUID() == UserID;
	});

	// If the request was found
	if(Iter != m_aRequestsJoin.end())
	{
		// Remove the request from the database
		Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d' AND UserID = '%d'", m_pGuild->GetID(), (*Iter)->GetFromUID());

		// If pFromMember exists
		if(pFromMember)
		{
			// Get the nicknames of the users
			const char* pFromNickname = Instance::Server()->GetAccountNickname(UserID);
			const char* pByNickname = Instance::Server()->GetAccountNickname(pFromMember->GetAccountID());

			// Add a history entry and send a guild chat message
			m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' denied invitation from '%s'.", pByNickname, pFromNickname);
			GS()->ChatGuild(m_pGuild->GetID(), "'{}' denied invitation from '{}'.", pByNickname, pFromNickname);
		}

		// Delete the request and remove it from m_aRequestsJoin
		delete (*Iter);
		m_aRequestsJoin.erase(Iter);
	}
}