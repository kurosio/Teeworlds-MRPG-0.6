/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_data.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/mails/mail_wrapper.h>

CGS* CGuild::GS() const { return (CGS*)Instance::GameServerPlayer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID); }

CGuild::~CGuild()
{
	delete m_pMembers;
	delete m_pLogger;
	delete m_pRanks;
	delete m_pBank;
}

bool CGuild::Upgrade(GuildUpgrade Type)
{
	// Check if the type is AVAILABLE_SLOTS and the value of the first upgrade data is greater than or equal to GUILD_MAX_SLOTS
	auto* pUpgradeData = &m_UpgradesData((int)Type, 0);
	if(Type == GuildUpgrade::AVAILABLE_SLOTS && pUpgradeData->m_Value >= GUILD_MAX_SLOTS)
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

void CGuild::AddExperience(int Experience)
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

		// Add a history and send a chat message to the server indicating the guild's level up
		GS()->Chat(-1, "Guild {} raised the level up to {}", GetName(), m_Level);
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Guild raised level to '%d'.", m_Level);
		UpdateTable = true;

		// Recalculate the new experience needed to level up
		ExperienceNeed = (int)computeExperience(m_Level);
	}

	// Check if a random chance or the need to update the table is met
	if(rand() % 10 == 2 || UpdateTable)
	{
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '%d', Experience = '%d' WHERE ID = '%d'", m_Level, m_Experience, m_ID);
	}
}

GuildResult CGuild::SetLeader(int AccountID)
{
	// Check if the given AccountID is already the leader of the guild
	if(AccountID == m_LeaderUID)
	{
		return GuildResult::SET_LEADER_PLAYER_ALREADY_LEADER;
	}

	// Check if the given AccountID is a member of the guild
	if(!m_pMembers->Get(AccountID))
	{
		return GuildResult::SET_LEADER_NON_GUILD_PLAYER;
	}

	// Update data
	m_LeaderUID = AccountID;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LeaderUID = '%d' WHERE ID = '%d'", m_LeaderUID, m_ID);

	// Add a new entry and send chat message to the guild history indicating the change of leader
	const char* pNickNewLeader = Instance::Server()->GetAccountNickname(m_LeaderUID);
	m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "New guild leader '%s'", pNickNewLeader);
	GS()->ChatGuild(m_ID, "New guild leader '{}'", pNickNewLeader);
	return GuildResult::SUCCESSFUL;
}

GuildResult CGuild::BuyHouse(int HouseID)
{
	// Check if the guild already has a house
	if(m_pHouse != nullptr)
	{
		return GuildResult::BUY_HOUSE_ALREADY_HAVE;
	}

	// Find the house data with the specified HouseID
	auto IterHouse = std::find_if(CGuildHouse::Data().begin(), CGuildHouse::Data().end(), [&HouseID](const CGuildHouse* p)
	{
		return p->GetID() == HouseID;
	});

	// Check if the house data was found
	if(IterHouse == CGuildHouse::Data().end())
	{
		return GuildResult::BUY_HOUSE_UNAVAILABLE;
	}

	// Retrieve the price of the house from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HOUSES, "WHERE ID = '%d'", HouseID);
	if(pRes->next())
	{
		const int Price = pRes->getInt("Price");

		// Check if the guild has enough gold to purchase the house
		if(!GetBank()->Spend(Price))
		{
			return GuildResult::BUY_HOUSE_NOT_ENOUGH_GOLD;
		}

		// Assign the house to the guild
		m_pHouse = *IterHouse;
		m_pHouse->UpdateGuild(this);
		m_pHouse->m_RentDays = GUILD_RENT_DAYS_DEFAULT;
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = '%d', RentDays = '%d' WHERE ID = '%d'", m_ID, m_pHouse->m_RentDays, HouseID);

		// Add a history entry and send a chat message for getting a guild house
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Your guild has purchased a house!");
		GS()->ChatGuild(m_ID, "Your guild has purchased a house!");
		return GuildResult::SUCCESSFUL; 
	}

	return GuildResult::BUY_HOUSE_ALREADY_PURCHASED;
}

void CGuild::SellHouse()
{
	// Check if the guild house is not null
	if(!m_pHouse)
		return;

	// Send mail
	const int ReturnedGold = m_pHouse->GetInitialFee();
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
}

void CGuild::HandleTimePeriod(TIME_PERIOD Period)
{
	// rent paid
	if(Period == DAILY_STAMP && HasHouse())
	{
		// can pay
		if(m_pHouse->ReduceRentDays())
		{
			GS()->ChatGuild(m_ID, "Your guild house rent has been paid.");
			m_pLogger->Add(LOGFLAG_HOUSE_MAIN_CHANGES, "House rent has been paid.");
			return;
		}

		// can't pay, so sell the house
		SellHouse();
		GS()->ChatGuild(m_ID, "Your guild house rent has expired, has been sold.");
		m_pLogger->Add(LOGFLAG_HOUSE_MAIN_CHANGES, "House rent has expired, has been sold.");
		GS()->UpdateVotesIfForAll(MENU_GUILD);
	}

	// reset members deposits
	if(Period == MONTH_STAMP)
	{
		m_pMembers->ResetDeposits();
		GS()->ChatGuild(m_ID, "Membership deposits have been reset.");
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Membership deposits have been reset.");
	}
}

bool CGuild::StartWar(CGuild* pTargetGuild)
{
	if(!pTargetGuild || pTargetGuild->GetWar() || GetWar())
		return false;
	
	CGuildWarHandler* pWarHandler = CGuildWarHandler::CreateElement();
	time_t TimeUntilEnd = time(nullptr) + (g_Config.m_SvGuildWarDuration * 60);
	pWarHandler->Init({ this, pTargetGuild, 0 }, { pTargetGuild, this, 0 }, TimeUntilEnd);
	return true;
}

int CGuild::GetUpgradePrice(GuildUpgrade Type)
{
	int EndPrice = 0;

	if(Type == GuildUpgrade::AVAILABLE_SLOTS)
		EndPrice = m_UpgradesData((int)Type, 0).m_Value * g_Config.m_SvPriceUpgradeGuildSlot;
	else if(Type == GuildUpgrade::HOUSE_CHAIR_EXPERIENCE)
		EndPrice = m_UpgradesData((int)Type, 0).m_Value * g_Config.m_SvPriceUpgradeGuildAnother;

	return EndPrice;
}

bool CGuild::IsAccountMemberGuild(int AccountID)
{
	return std::any_of(CGuild::Data().begin(), CGuild::Data().end(), [&AccountID](const CGuild* p)
	{
		return p->GetMembers()->Get(AccountID) != nullptr;
	});
}

/* -------------------------------------
 * Bank impl
 * ------------------------------------- */
CGS* CGuild::CBank::GS() const { return m_pGuild->GS(); }
void CGuild::CBank::Set(int Value)
{
	m_Bank = Value;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
}

bool CGuild::CBank::Spend(int Value)
{
	if(m_Bank <= 0 || m_Bank < Value)
		return false;

	m_Bank -= Value;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pGuild->GetID());
	return true;
}

/* -------------------------------------
 * Logger impl
 * ------------------------------------- */
CGuild::CLogEntry::CLogEntry(CGuild* pGuild, int64_t Logflag) : m_pGuild(pGuild)
{
	m_Logflag = Logflag < 0 ? (int64_t)LOGFLAG_GUILD_FULL : Logflag;

	CGuild::CLogEntry::InitLogs();
}

void CGuild::CLogEntry::SetActivityFlag(int64_t Flag)
{
	if(Flag & m_Logflag)
		m_Logflag &= ~Flag;
	else
		m_Logflag |= Flag;

	// Update the log flag in the database for the guild
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LogFlag = '%d' WHERE ID = '%d'", m_Logflag, m_pGuild->GetID());
}

bool CGuild::CLogEntry::IsActivityFlagSet(int64_t Flag) const { return (Flag & m_Logflag); }

void CGuild::CLogEntry::Add(int64_t LogFlag, const char* pBuffer, ...)
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
		if(m_aLogs.size() >= GUILD_LOGS_MAX_COUNT)
			m_aLogs.pop_front();

		// Get the current timestamp and store it in aBufTimeStamp
		char aBufTimeStamp[64];
		str_timestamp_format(aBufTimeStamp, sizeof(aBufTimeStamp), FORMAT_WITHOUT_SEC_SPACE);

		// Add the formatted string and timestamp to the logs list
		const sqlstr::CSqlString<64> cBuf = sqlstr::CSqlString<64>(aBuf);
		m_aLogs.push_back({ cBuf.cstr(), aBufTimeStamp });

		// Execute an SQL INSERT statement to store the log in the database
		Database->Execute<DB::INSERT>(TW_GUILDS_HISTORY_TABLE, "(GuildID, Text, Time) VALUES ('%d', '%s', '%s')", m_pGuild->GetID(), cBuf.cstr(), aBufTimeStamp);
	}
}

void CGuild::CLogEntry::InitLogs()
{
	// Execute a select query to fetch guild logs from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '%d' ORDER BY ID DESC LIMIT %d", m_pGuild->GetID(), (int)GUILD_LOGS_MAX_COUNT);
	while(pRes->next())
	{
		// Create a log object and populate it with time and text values from the database
		m_aLogs.push_back({ pRes->getString("Text").c_str(), pRes->getString("Time").c_str() });
	}
}

/* -------------------------------------
 * Ranks impl
 * ------------------------------------- */
CGS* CGuild::CRank::GS() const { return m_pGuild->GS(); }
CGuild::CRank::CRank(GuildRankIdentifier RID, std::string&& Rank, GuildRankRights Rights, CGuild* pGuild) : m_ID(RID), m_Rank(std::move(Rank))
{
	m_Rights = Rights;
	m_pGuild = pGuild;
}

GuildResult CGuild::CRank::Rename(std::string NewRank)
{
	// Create a CSqlString object from the NewRank string
	auto cstrNewRank = CSqlString<64>(NewRank.c_str());

	// Check if the length of the string is less than 2 or greater than 16
	const int LengthRank = str_length(cstrNewRank.cstr());
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
		return GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME;

	// Check if the new rank name is already used by another rank in the guild
	if(std::count_if(m_pGuild->GetRanks()->GetContainer().begin(), m_pGuild->GetRanks()->GetContainer().end(),
		[&cstrNewRank](const CRank* pRank) {return str_comp(pRank->m_Rank.c_str(), cstrNewRank.cstr()) == 0; }))
		return GuildResult::RANK_RENAME_ALREADY_NAME_EXISTS;

	// Update the name of the guild rank
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "renamed rank '%s' to '%s'", m_Rank.c_str(), cstrNewRank.cstr());
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Name = '%s' WHERE ID = '%d'", cstrNewRank.cstr(), m_ID);
	m_Rank = cstrNewRank.cstr();
	return GuildResult::RANK_SUCCESSFUL;
}

void CGuild::CRank::SetRights(GuildRankRights Rights)
{
	// Set the access level, clamping it between GUILD_RANK_RIGHT_DEFAULT and GUILD_RANK_RIGHT_FULL
	m_Rights = (GuildRankRights)clamp((int)Rights, (int)GUILD_RANK_RIGHT_DEFAULT, (int)GUILD_RANK_RIGHT_FULL);

	// Save the updated access level in the database
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Rights = '%d' WHERE ID = '%d'", m_Rights, m_ID);

	// Send a chat message to the guild with the updated access level
	GS()->ChatGuild(GuildID, "Rank '{}' new rights '{}'!", m_Rank.c_str(), GetRightsName());
}

const char* CGuild::CRank::GetRightsName(GuildRankRights Rights) const
{
	if(Rights == GUILD_RANK_RIGHT_INVITE_KICK)
		return "Invite & kick";
	if(Rights == GUILD_RANK_RIGHT_UPGRADES_HOUSE)
		return "Upgrade & house door's";
	if(Rights == GUILD_RANK_RIGHT_FULL)
		return "Full";
	return "Default";
}

/* -------------------------------------
 * Ranks manager impl
 * ------------------------------------- */
CGS* CGuild::CRanksManager::GS() const { return m_pGuild->GS(); }
CGuild::CRanksManager::CRanksManager(CGuild* pGuild, GuildRankIdentifier DefaultID)
	: m_pGuild(pGuild)
{
	// Initialize the guild ranks controller
	CGuild::CRanksManager::Init(DefaultID);
}

CGuild::CRanksManager::~CRanksManager()
{
	// Delete all the rank data objects
	for(auto p : m_aRanks)
		delete p;

	// Clear the default rank pointer and the ranks vector
	m_pDefaultRank = nullptr;
	m_aRanks.clear();
}

void CGuild::CRanksManager::Init(GuildRankIdentifier DefaultID)
{
	// Execute a database query to get the rank data for the guild
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		// Get the rank ID, name, and access level from the database query result
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		GuildRankRights Rights = (GuildRankRights)pRes->getInt("Rights");

		// Create a new CRank object and add it to the ranks vector
		if(DefaultID == RID)
			m_pDefaultRank = m_aRanks.emplace_back(new CRank(RID, std::forward<std::string>(Rank), GUILD_RANK_RIGHT_DEFAULT, m_pGuild));
		else
			m_aRanks.emplace_back(new CRank(RID, std::forward<std::string>(Rank), Rights, m_pGuild));
	}
}

void CGuild::CRanksManager::UpdateDefaultRank()
{
	// If the default rank already exists, return
	if(m_pDefaultRank)
		return;

	// If there are no ranks, create a default rank called "Member"
	if(!m_aRanks.empty())
	{
		m_pDefaultRank = m_aRanks.back();
		m_pDefaultRank->SetRights(GUILD_RANK_RIGHT_DEFAULT);
	}
	else
	{
		GuildResult Result = Add("Newbie");
		dbg_assert(Result == GuildResult::RANK_SUCCESSFUL, "guild cannot initialize a default rank");
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

GuildResult CGuild::CRanksManager::Add(const std::string& Rank)
{
	// Create a CSqlString object for the rank name
	auto cstrRank = CSqlString<64>(Rank.c_str());

	// Check if the length of the string is less than 2 or greater than 16
	const int LengthRank = str_length(cstrRank.cstr());
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
		return GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME;

	// Check if the rank already exists
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CRank* pRank) { return std::string(pRank->GetName()) == cstrRank.cstr(); }))
		return GuildResult::RANK_ADD_ALREADY_EXISTS;

	// Check if the rank count has reached the limit
	if((int)m_aRanks.size() >= GUILD_RANKS_MAX_COUNT)
		return GuildResult::RANK_ADD_LIMIT_HAS_REACHED;

	// Get the ID for the new rank
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1;

	// Insert the new rank into the database
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, Rights, GuildID, Name) VALUES ('%d', '%d', '%d', '%s')", InitID, (int)GUILD_RANK_RIGHT_DEFAULT, GuildID, cstrRank.cstr());
	m_aRanks.emplace_back(new CRank(InitID, cstrRank.cstr(), GUILD_RANK_RIGHT_DEFAULT, m_pGuild));

	// Send information to the game server and update the guild history
	GS()->ChatGuild(GuildID, "New rank is created [{}]!", cstrRank.cstr());
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "added rank '%s'", cstrRank.cstr());
	return GuildResult::RANK_SUCCESSFUL;
}

GuildResult CGuild::CRanksManager::Remove(const std::string& Rank)
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
		return GuildResult::RANK_REMOVE_IS_DEFAULT;

	// If the rank does not exist, return
	if(Iter == m_aRanks.end())
		return GuildResult::RANK_REMOVE_DOES_NOT_EXIST;

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
	return GuildResult::RANK_SUCCESSFUL;
}

CGuild::CRank* CGuild::CRanksManager::Get(const std::string& Rank) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CRank* pRank){ return pRank->GetName() == Rank; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}

CGuild::CRank* CGuild::CRanksManager::Get(GuildRankIdentifier ID) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [ID](const CRank* pRank){ return pRank->GetID() == ID; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}

/* -------------------------------------
 * Members impl
 * ------------------------------------- */
CGS* CGuild::CMember::GS() const { return m_pGuild->GS(); }
CGuild::CMember::CMember(CGuild* pGuild, int AccountID, CRank* pRank, BigInt Deposit) : m_pGuild(pGuild)
{
	m_AccountID = AccountID;
	m_Deposit = Deposit;

	// If the given rank is null, set the member's rank to the default rank of the guild
	m_pRank = pRank == nullptr ? pGuild->GetRanks()->GetDefaultRank() : pRank;
}

CGuild::CMember::~CMember()
{
	// Reinitialize the guild for the player's account
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID))
	{
		pPlayer->Account()->ReinitializeGuild(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
}

bool CGuild::CMember::SetRank(GuildRankIdentifier RankID)
{
	// Get the rank object from the guild's ranks using the given rank ID
	CRank* pRank = m_pGuild->GetRanks()->Get(RankID);
	if(!pRank)
		return false;

	// Set the member's rank to the given rank
	return SetRank(pRank);
}

bool CGuild::CMember::SetRank(CRank* pRank)
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

bool CGuild::CMember::DepositInBank(int Golds)
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

bool CGuild::CMember::WithdrawFromBank(int Golds)
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

bool CGuild::CMember::CheckAccess(GuildRankRights RequiredAccess) const
{
	return (m_pGuild->GetLeaderUID() == m_AccountID || m_pRank->GetRights() == RequiredAccess
		|| (m_pRank->GetRights() == GUILD_RANK_RIGHT_FULL && RequiredAccess != GUILD_RANK_RIGHT_LEADER));
}

/* -------------------------------------
 * Members manager impl
 * ------------------------------------- */
CGS* CGuild::CMembersManager::GS() const { return m_pGuild->GS(); }
CGuild::CMembersManager::CMembersManager(CGuild* pGuild, std::string&& JsonMembers) : m_pGuild(pGuild)
{
	// Create a new instance of CRequestsManager with pGuild as the parameter
	m_pRequests = new CRequestsManager(pGuild);

	// Initialize CGuildMembersManager with JsonMembers using std::move to transfer ownership
	CGuild::CMembersManager::Init(std::move(JsonMembers));
}

CGuild::CMembersManager::~CMembersManager()
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

GuildResult CGuild::CMembersManager::Join(int AccountID)
{
	// Check if the member is already in the guild
	if(m_apMembers.find(AccountID) != m_apMembers.end() || CGuild::IsAccountMemberGuild(AccountID))
		return GuildResult::MEMBER_JOIN_ALREADY_IN_GUILD;

	// Check if there are free slots available in the guild
	if(!HasFreeSlots())
		return GuildResult::MEMBER_NO_AVAILABLE_SLOTS;

	// Create a new guild member data and add it to the guild members map
	m_apMembers[AccountID] = new CMember(m_pGuild, AccountID, m_pGuild->GetRanks()->GetDefaultRank());

	// Reinitialize the guild for the player if they are online
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
		pPlayer->Account()->ReinitializeGuild();

	// Add a join message to the guild history and send chat message
	const char* pNickname = Instance::Server()->GetAccountNickname(AccountID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' has joined the guild.", pNickname);
	GS()->ChatGuild(m_pGuild->GetID(), "'{}' has joined the guild!", pNickname);

	// Save the guild members data
	Save();
	return GuildResult::MEMBER_SUCCESSFUL;
}

GuildResult CGuild::CMembersManager::Kick(int AccountID)
{
	// Check if the player is the guild leader
	if(m_pGuild->GetLeaderUID() == AccountID)
		return GuildResult::MEMBER_KICK_IS_OWNER;

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
		return GuildResult::MEMBER_SUCCESSFUL;
	}

	return GuildResult::MEMBER_KICK_DOES_NOT_EXIST;
}

bool CGuild::CMembersManager::HasFreeSlots() const
{
	return (int)m_apMembers.size() < m_pGuild->GetUpgrades(GuildUpgrade::AVAILABLE_SLOTS)->getValue();
}

std::pair<int, int> CGuild::CMembersManager::GetCurrentSlots() const
{
	return std::pair((int)m_apMembers.size(), m_pGuild->GetUpgrades(GuildUpgrade::AVAILABLE_SLOTS)->getValue());
}

void CGuild::CMembersManager::ResetDeposits()
{
	// Iterate through each member in the member list
	for(auto& [UID, pMember] : m_apMembers)
		pMember->SetDeposit(0);

	// Save the updated member list
	Save();
}

int CGuild::CMembersManager::GetOnlinePlayersCount() const
{
	int Count = 0;
	for(auto& [UID, pMember] : m_apMembers)
	{
		if(GS()->GetPlayerByUserID(UID))
			Count++;
	}
	return Count;
}

void CGuild::CMembersManager::Init(std::string&& JsonMembers)
{
	// Assert by empty
	dbg_assert(m_apMembers.empty(), "");

	// Parse the JSON string
	Tools::Json::parseFromString(JsonMembers, [this](nlohmann::json& pJson)
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
				BigInt Deposit(pMember.value("deposit", "0"));

				// Create a new member data object and add it to the member list
				m_apMembers[UID] = (new CMember(m_pGuild, UID, m_pGuild->GetRanks()->Get(RID), Deposit));
			}
		}
	});
}

void CGuild::CMembersManager::Save() const
{
	// Create a JSON object for the member data
	nlohmann::json MembersData;
	for(auto& [UID, pMember] : m_apMembers)
	{
		nlohmann::json memberData;
		memberData["id"] = UID;
		memberData["rank_id"] = pMember->GetRank()->GetID();
		memberData["deposit"] = pMember->GetDeposit().to_string().c_str();
		MembersData["members"].push_back(memberData);
	}

	// Update the guild data in the database
	Database->Execute<DB::UPDATE, 300>(TW_GUILDS_TABLE, "DefaultRankID = '%d', Members = '%s' WHERE ID = '%d'",
		m_pGuild->GetRanks()->GetDefaultRank()->GetID(), MembersData.dump().c_str(), m_pGuild->GetID());
}

CGuild::CMember* CGuild::CMembersManager::Get(int AccountID)
{
	return m_apMembers.find(AccountID) != m_apMembers.end() ? m_apMembers[AccountID] : nullptr;
}


/* -------------------------------------
 * Requests member manager impl
 * ------------------------------------- */
CGS* CGuild::CRequestsManager::GS() const { return m_pGuild->GS(); }
CGuild::CRequestsManager::CRequestsManager(CGuild* pGuild) : m_pGuild(pGuild)
{
	CRequestsManager::Init();
}

CGuild::CRequestsManager::~CRequestsManager()
{
	for(auto p : m_aRequestsJoin)
		delete p;

	m_aRequestsJoin.clear();
}

void CGuild::CRequestsManager::Init()
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

GuildResult CGuild::CRequestsManager::Request(int FromUID)
{
	// Check if the invite already exists in the guild's invites container
	if(std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(),
		[&FromUID](const RequestData* p){ return p->GetFromUID() == FromUID; }) != m_aRequestsJoin.end())
		return GuildResult::MEMBER_REQUEST_ALREADY_SEND;

	// Check if the guild's member list has free slots
	if(!m_pGuild->GetMembers()->HasFreeSlots())
		return GuildResult::MEMBER_NO_AVAILABLE_SLOTS;

	// Add the invite to the guild's history and send a chat message
	const char* pFromNickname = Instance::Server()->GetAccountNickname(FromUID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "invitation to join from '%s'.", pFromNickname);
	GS()->ChatGuild(m_pGuild->GetID(), "invitation to join from '{}'.", pFromNickname);

	// Create a new invite data object and add it to the guild's invites container
	m_aRequestsJoin.push_back(new RequestData(FromUID));

	// Insert the invite into the database
	Database->Execute<DB::INSERT>(TW_GUILDS_INVITES_TABLE, "(GuildID, UserID) VALUES ('%d', '%d')", m_pGuild->GetID(), FromUID);
	return GuildResult::MEMBER_SUCCESSFUL;
}

GuildResult CGuild::CRequestsManager::Accept(int UserID, const CMember* pFromMember)
{
	// Find the request with the given UserID in the m_aRequestsJoin vector
	auto Iter = std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&UserID](const RequestData* pRequest)
	{
		return pRequest->GetFromUID() == UserID;
	});

	// If the request is not found, return an undefined error
	if(Iter == m_aRequestsJoin.end())
		return GuildResult::MEMBER_UNDEFINED_ERROR;

	// Remove the request from the database and delete it from memory
	Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d' AND UserID = '%d'", m_pGuild->GetID(), (*Iter)->GetFromUID());
	delete (*Iter);
	m_aRequestsJoin.erase(Iter);

	// Try to add the user to the guild as a member if the user is successfully added as a member and pFromMember is not null
	GuildResult Result = m_pGuild->GetMembers()->Join(UserID);
	if(Result == GuildResult::MEMBER_SUCCESSFUL && pFromMember)
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

void CGuild::CRequestsManager::Deny(int UserID, const CMember* pFromMember)
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