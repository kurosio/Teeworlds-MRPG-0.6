/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_data.h"
#include <game/server/gamecontext.h>
#include <game/server/core/components/mails/mail_wrapper.h>

CGS* CGuild::GS() const { return (CGS*)Instance::GameServerPlayer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID); }

CGuild::~CGuild()
{
	delete m_pMembers;
	delete m_pLogger;
	delete m_pRanks;
	delete m_pBankManager;
}

bool CGuild::Upgrade(GuildUpgrade Type)
{
	auto* pUpgradeField = &m_UpgradesData.getField<int>((int)Type);

	// check maximum for available slots
	if(Type == GuildUpgrade::AvailableSlots && pUpgradeField->m_Value >= GUILD_MAX_SLOTS)
		return false;

	const int Price = GetUpgradePrice(Type);
	if(m_pBankManager->Spend(Price))
	{
		pUpgradeField->m_Value += 1;
		Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "{} = '{}' WHERE ID = '{}'", pUpgradeField->getFieldName(), pUpgradeField->m_Value, m_ID);

		// Add and send a history entry for the upgrade
		m_pLogger->Add(LOGFLAG_UPGRADES_CHANGES, "'%s' upgraded to %d level", pUpgradeField->getDescription(), pUpgradeField->m_Value);
		GS()->ChatGuild(m_ID, "'{}' upgraded to {} level", pUpgradeField->getDescription(), pUpgradeField->m_Value);
		return true;
	}

	return false;
}

void CGuild::AddExperience(uint64_t Experience)
{
	// initialize variables
	bool UpdateTable = false;
	auto ExperienceNeed = computeExperience(m_Level);

	// add experience
	m_Experience += Experience;

	// check if guild needs to level up
	while(m_Experience >= ExperienceNeed)
	{
		// level up
		m_Experience -= ExperienceNeed;
		m_Level++;

		// send messages
		GS()->Chat(-1, "Guild '{}' raised the level up to {}", GetName(), m_Level);
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Guild raised level to '%d'.", m_Level);

		// recompute experience
		ExperienceNeed = computeExperience(m_Level);
		UpdateTable = true;
	}

	// update table
	if(rand() % 10 == 2 || UpdateTable)
	{
		Database->Execute<DB::UPDATE>("tw_guilds", "Level = '{}', Exp = '{}' WHERE ID = '{}'", m_Level, m_Experience, m_ID);
	}
}

GuildResult CGuild::SetLeader(int AccountID)
{
	// check if the given AccountID is the guild leader
	if(AccountID == m_LeaderUID)
		return GuildResult::SET_LEADER_PLAYER_ALREADY_LEADER;

	// check if the given AccountID is a guild player
	if(!m_pMembers->Get(AccountID))
		return GuildResult::SET_LEADER_NON_GUILD_PLAYER;

	// implement the leader change
	m_LeaderUID = AccountID;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LeaderUID = '{}' WHERE ID = '{}'", m_LeaderUID, m_ID);

	// send messages
	const char* pNickNewLeader = Instance::Server()->GetAccountNickname(m_LeaderUID);
	m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "New guild leader '%s'", pNickNewLeader);
	GS()->ChatGuild(m_ID, "New guild leader '{}'", pNickNewLeader);
	return GuildResult::SUCCESSFUL;
}

GuildResult CGuild::BuyHouse(int HouseID)
{
	// check if the guild already has a house
	if(m_pHouse != nullptr)
		return GuildResult::BUY_HOUSE_ALREADY_HAVE;

	// find the house data
	auto IterHouse = std::find_if(CGuildHouse::Data().begin(), CGuildHouse::Data().end(), [&HouseID](const CGuildHouse* p)
	{ return p->GetID() == HouseID; });

	// check house validity
	if(IterHouse == CGuildHouse::Data().end())
		return GuildResult::BUY_HOUSE_UNAVAILABLE;

	// check if the house is already purchased
	if((*IterHouse)->IsPurchased())
		return GuildResult::BUY_HOUSE_ALREADY_PURCHASED;

	// try to buy the house
	if(GetBankManager()->Spend((*IterHouse)->GetInitialFee()))
	{
		// implement the house
		m_pHouse = *IterHouse;
		m_pHouse->UpdateGuild(this);
		m_pHouse->m_RentDays = GUILD_RENT_DAYS_DEFAULT;
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = '{}', RentDays = '{}' WHERE ID = '{}'", m_ID, m_pHouse->m_RentDays, HouseID);

		// send messages
		m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Your guild has purchased a house!");
		GS()->ChatGuild(m_ID, "Your guild has purchased a house!");
		return GuildResult::SUCCESSFUL;

	}

	// failed to buy the house
	return GuildResult::BUY_HOUSE_NOT_ENOUGH_GOLD;
}

void CGuild::SellHouse()
{
	// check if the guild has a house
	if(m_pHouse == nullptr)
		return;

	// send mail
	const int ReturnedGold = m_pHouse->GetInitialFee();
	MailWrapper Mail("System", m_LeaderUID, "Guild house is sold.");
	Mail.AddDescLine("We returned some gold from your guild.");
	Mail.AttachItem(CItem(itGold, ReturnedGold));
	Mail.Send();

	// send messages
	m_pLogger->Add(LOGFLAG_GUILD_MAIN_CHANGES, "Lost a house on '%s'.", Server()->GetWorldName(m_pHouse->GetWorldID()));
	GS()->ChatGuild(m_ID, "House sold, {}gold returned to leader", ReturnedGold);

	// implement the sell of the house
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "GuildID = NULL WHERE ID = '{}'", m_pHouse->GetID());
	m_pHouse->GetDoorManager()->CloseAll();
	m_pHouse->UpdateGuild(nullptr);
	m_pHouse = nullptr;
}

void CGuild::HandleTimePeriod(ETimePeriod Period)
{
	// rent paid
	if(Period == DAILY_STAMP && HasHouse())
	{
		// can pay
		if(m_pHouse->ReduceRentDays(1))
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
	time_t TimeUntilEnd = time(nullptr) + (g_Config.m_SvGuildWarDurationMinutes * 60);
	pWarHandler->Init({ this, pTargetGuild, 0 }, { pTargetGuild, this, 0 }, TimeUntilEnd);
	return true;
}

int CGuild::GetUpgradePrice(GuildUpgrade Type)
{
	int EndPrice = 0;

	if(Type == GuildUpgrade::AvailableSlots)
	{
		const int CurrentPoint = m_UpgradesData.getRef<int>((int)GuildUpgrade::AvailableSlots);
		EndPrice = CurrentPoint * g_Config.m_SvGuildSlotUpgradePrice;
	}
	else
	{
		const int CurrentPoint = m_UpgradesData.getRef<int>((int)Type);
		EndPrice = CurrentPoint * g_Config.m_SvGuildAnotherUpgradePrice;
	}

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
CGS* CGuild::CBank::GS() const
{
	return m_pGuild->GS();
}

void CGuild::CBank::Add(const BigInt& Value)
{
	m_Value += Value;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '{}' WHERE ID = '{}'", m_Value, m_pGuild->GetID());
}

bool CGuild::CBank::Spend(const BigInt& Value)
{
	if(m_Value <= 0 || m_Value < Value)
		return false;

	m_Value -= Value;
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '{}' WHERE ID = '{}'", m_Value, m_pGuild->GetID());
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

	// update the database
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LogFlag = '{}' WHERE ID = '{}'", m_Logflag, m_pGuild->GetID());
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

		// check if the logs list is full
		if(m_aLogs.size() >= GUILD_LOGS_MAX_COUNT)
			m_aLogs.pop_front();

		// get the current timestamp
		char aBufTimeStamp[64];
		str_timestamp_format(aBufTimeStamp, sizeof(aBufTimeStamp), FORMAT_WITHOUT_SEC_SPACE);

		// add the formatted string and timestamp to the logs list
		const auto cBuf = CSqlString<64>(aBuf);
		m_aLogs.push_back({ cBuf.cstr(), aBufTimeStamp });
		Database->Execute<DB::INSERT>(TW_GUILDS_HISTORY_TABLE, "(GuildID, Text, Time) VALUES ('{}', '{}', '{}')", m_pGuild->GetID(), cBuf.cstr(), aBufTimeStamp);
	}
}

void CGuild::CLogEntry::InitLogs()
{
	// initialize the logs list
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '{}' ORDER BY ID DESC LIMIT {}", m_pGuild->GetID(), (int)GUILD_LOGS_MAX_COUNT);
	while(pRes->next())
		m_aLogs.push_back({ pRes->getString("Text").c_str(), pRes->getString("Time").c_str() });
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
	// initialize variables
	auto cstrNewRank = CSqlString<64>(NewRank.c_str());
	const int LengthRank = str_length(cstrNewRank.cstr());

	// check validity
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
		return GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME;

	// check already exists
	if(m_pGuild->GetRanks()->Get(cstrNewRank.cstr()))
		return GuildResult::RANK_RENAME_ALREADY_NAME_EXISTS;

	// implement renaming
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "renamed rank '%s' to '%s'", m_Rank.c_str(), cstrNewRank.cstr());
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Name = '{}' WHERE ID = '{}'", cstrNewRank.cstr(), m_ID);
	m_Rank = cstrNewRank.cstr();
	return GuildResult::RANK_SUCCESSFUL;
}

void CGuild::CRank::SetRights(GuildRankRights Rights)
{
	// implement setting new rights
	m_Rights = (GuildRankRights)clamp((int)Rights, (int)GUILD_RANK_RIGHT_DEFAULT, (int)GUILD_RANK_RIGHT_FULL);
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Rights = '{}' WHERE ID = '{}'", (int)m_Rights, m_ID);

	// send messages
	GS()->ChatGuild(m_pGuild->GetID(), "Rank '{}' new rights '{}'!", m_Rank.c_str(), GetRightsName());
}

const char* CGuild::CRank::GetRightsName(GuildRankRights Rights) const
{
	switch(Rights)
	{
		case GUILD_RANK_RIGHT_INVITE_KICK: return "Invite & kick";
		case GUILD_RANK_RIGHT_UPGRADES_HOUSE: return "Upgrade & house door's";
		case GUILD_RANK_RIGHT_FULL: return "Full";
		default: return "Default";
	}
}

/* -------------------------------------
 * Ranks manager impl
 * ------------------------------------- */
CGS* CGuild::CRanksManager::GS() const { return m_pGuild->GS(); }
CGuild::CRanksManager::CRanksManager(CGuild* pGuild, GuildRankIdentifier DefaultID)
	: m_pGuild(pGuild)
{
	// initialize ranks
	CGuild::CRanksManager::Init(DefaultID);
}

CGuild::CRanksManager::~CRanksManager()
{
	// delete all guild ranks
	for(auto p : m_aRanks)
		delete p;

	// clear the default rank pointer and the ranks vector
	m_pDefaultRank = nullptr;
	m_aRanks.clear();
}

void CGuild::CRanksManager::Init(GuildRankIdentifier DefaultID)
{
	// execute a database query to get the rank data for the guild
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '{}'", m_pGuild->GetID());
	while(pRes->next())
	{
		// initialize variables
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		GuildRankRights Rights = (GuildRankRights)pRes->getInt("Rights");

		// check is default rank and set the default rank pointer
		if(DefaultID == RID)
			m_pDefaultRank = m_aRanks.emplace_back(new CRank(RID, std::forward<std::string>(Rank), GUILD_RANK_RIGHT_DEFAULT, m_pGuild));
		else
			m_aRanks.emplace_back(new CRank(RID, std::forward<std::string>(Rank), Rights, m_pGuild));
	}
}

void CGuild::CRanksManager::UpdateDefaultRank()
{
	// check if there is already a default rank
	if(m_pDefaultRank)
		return;

	// if there are no ranks, create a default rank
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

	// update the guild members
	for(const auto& pIterMember : m_pGuild->GetMembers()->GetContainer())
	{
		const auto pMember = pIterMember.second;
		if(!pMember->GetRank())
		{
			bool Status = pMember->SetRank(m_pDefaultRank);
			dbg_assert(Status, "guild cannot set a default rank for member");
		}
	}

	// save the guild members
	m_pGuild->GetMembers()->Save();
}

GuildResult CGuild::CRanksManager::Add(const std::string& Rank)
{
	// initialize variables
	auto cstrRank = CSqlString<64>(Rank.c_str());
	const int LengthRank = str_length(cstrRank.cstr());

	// check length
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
		return GuildResult::RANK_WRONG_NUMBER_OF_CHAR_IN_NAME;

	// check if the rank already exists
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CRank* pRank) { return std::string(pRank->GetName()) == cstrRank.cstr(); }))
		return GuildResult::RANK_ADD_ALREADY_EXISTS;

	// check if the rank total count has reached the limit
	if((int)m_aRanks.size() >= GUILD_RANKS_MAX_COUNT)
		return GuildResult::RANK_ADD_LIMIT_HAS_REACHED;

	// get next rank ID
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1;

	// implement the new rank
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, Rights, GuildID, Name) VALUES ('{}', '{}', '{}', '{}')", InitID, (int)GUILD_RANK_RIGHT_DEFAULT, GuildID, cstrRank.cstr());
	m_aRanks.emplace_back(new CRank(InitID, cstrRank.cstr(), GUILD_RANK_RIGHT_DEFAULT, m_pGuild));

	// send messages
	GS()->ChatGuild(GuildID, "New rank is created [{}]!", cstrRank.cstr());
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "added rank '%s'", cstrRank.cstr());
	return GuildResult::RANK_SUCCESSFUL;
}

GuildResult CGuild::CRanksManager::Remove(const std::string& Rank)
{
	// initialize variables
	auto cstrRank = CSqlString<64>(Rank.c_str());
	auto pRank = Get(cstrRank.cstr());

	// check validity
	if(!pRank)
		return GuildResult::RANK_REMOVE_DOES_NOT_EXIST;

	// check is default
	if(pRank == m_pDefaultRank)
		return GuildResult::RANK_REMOVE_IS_DEFAULT;

	// update member ranks
	for(auto& pMember : m_pGuild->GetMembers()->GetContainer())
	{
		if(pRank->GetID() == pMember.second->GetRank()->GetID())
		{
			bool Status = pMember.second->SetRank(m_pDefaultRank);
			dbg_assert(Status, "guild cannot set a default rank for member");
		}
	}

	// implement removal
	Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '{}'", pRank->GetID());
	m_aRanks.erase(std::find(m_aRanks.begin(), m_aRanks.end(), pRank));
	delete pRank;

	// send messages
	GS()->ChatGuild(m_pGuild->GetID(), "Rank '{}' successfully delete", cstrRank.cstr());
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
CGuild::CMember::CMember(CGuild* pGuild, int AccountID, CRank* pRank, const BigInt Deposit) : m_pGuild(pGuild), m_pRank(pRank)
{
	m_AccountID = AccountID;
	m_Deposit = Deposit;

	// if member does not have a rank then set it to default
	if(!m_pRank)
		m_pRank = pGuild->GetRanks()->GetDefaultRank();
}

CGuild::CMember::~CMember()
{
	// reinitialize player guild data
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID))
	{
		pPlayer->Account()->ReinitializeGuild(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
}

bool CGuild::CMember::IsOnline() const
{
	return GS()->GetPlayerByUserID(m_AccountID) != nullptr;
}

bool CGuild::CMember::SetRank(GuildRankIdentifier RankID)
{
	// check rank validity
	auto* pRank = m_pGuild->GetRanks()->Get(RankID);
	if(!pRank)
		return false;

	// update member rank
	return SetRank(pRank);
}

bool CGuild::CMember::SetRank(CRank* pRank)
{
	// check rank validity
	if(!pRank)
		return false;

	// implement rank change
	m_pRank = pRank;
	m_pGuild->GetMembers()->Save();

	// send messages
	const char* pNickname = Instance::Server()->GetAccountNickname(m_AccountID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "%s rank changed to %s", pNickname, m_pRank->GetName());
	GS()->ChatGuild(m_pGuild->GetID(), "'{}' rank changed to '{}'!", pNickname, m_pRank->GetName());
	return true;
}

bool CGuild::CMember::DepositInBank(int Value)
{
	// check player validity
	const auto* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	// try spend from player
	if(pPlayer->Account()->SpendCurrency(Value))
	{
		// implement deposit
		m_Deposit += Value;
		m_pGuild->GetBankManager()->Add(Value);
		m_pGuild->GetMembers()->Save();

		// send messages
		const char* pNickname = Instance::Server()->GetAccountNickname(m_AccountID);
		m_pGuild->GetLogger()->Add(LOGFLAG_BANK_CHANGES, "'%s' deposit '%d' in the guild safe.", pNickname, Value);
		GS()->ChatGuild(m_pGuild->GetID(), "'{}' deposit {} gold in the safe, now {}!", pNickname, Value, m_pGuild->GetBankManager()->Get());
		return true;
	}

	return false;
}

bool CGuild::CMember::WithdrawFromBank(int Value)
{
	// check player validity
	auto* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	// try spend from guild bank
	if(m_pGuild->GetBankManager()->Spend(Value))
	{
		// implement the withdraw
		m_Deposit -= Value;
		pPlayer->Account()->AddGold(Value);
		m_pGuild->GetMembers()->Save();

		// send messages
		const char* pNickname = Instance::Server()->GetAccountNickname(m_AccountID);
		m_pGuild->GetLogger()->Add(LOGFLAG_BANK_CHANGES, "'%s' withdrawn '%d' from the guild safe.", pNickname, Value);
		GS()->ChatGuild(m_pGuild->GetID(), "'{}' withdrawn {} gold from the safe, now {}!", pNickname, Value, m_pGuild->GetBankManager()->Get());
		return true;
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
CGuild::CMembersManager::CMembersManager(CGuild* pGuild, const std::string& JsonMembers) : m_pGuild(pGuild)
{
	// Create a new instance of CRequestsManager with pGuild as the parameter
	m_pRequests = new CRequestsManager(pGuild);

	// Initialize CGuildMembersManager with JsonMembers using std::move to transfer ownership
	CGuild::CMembersManager::Init(JsonMembers);
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
	if(m_apMembers.find(AccountID) != m_apMembers.end() || IsAccountMemberGuild(AccountID))
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
	const int CurrentSlots = m_pGuild->GetUpgrades().getRef<int>((int)GuildUpgrade::AvailableSlots);
	return (int)m_apMembers.size() < CurrentSlots;
}

std::pair<int, int> CGuild::CMembersManager::GetCurrentSlots() const
{
	const int CurrentSlots = m_pGuild->GetUpgrades().getRef<int>((int)GuildUpgrade::AvailableSlots);
	return { (int)m_apMembers.size(), CurrentSlots };
}

void CGuild::CMembersManager::ResetDeposits()
{
	// Iterate through each member in the member list
	for(auto& [UID, pMember] : m_apMembers)
		pMember->SetDeposit(0);

	// Save the updated member list
	Save();
}

int CGuild::CMembersManager::GetOnlineCount() const
{
	return (int)std::count_if(m_apMembers.begin(), m_apMembers.end(), [](const auto& pMember) { return pMember.second->IsOnline(); });
}

void CGuild::CMembersManager::Init(const std::string& JsonMembers)
{
	// Assert by empty
	dbg_assert(m_apMembers.empty(), "");

	// Parse the JSON string
	mystd::json::parse(JsonMembers, [this](nlohmann::json& pJson)
	{
		for(auto& pMember : pJson["members"])
		{
			// Check if the member ID is valid and not already in the member list
			int UID = pMember.value("id", -1);
			if(!m_apMembers.contains(UID))
			{
				// Get the rank ID and deposit for the member
				int RID = pMember.value("rank_id", -1);
				BigInt Deposit(pMember.value("deposit", BigInt(0)));

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
	Database->Execute<DB::UPDATE, 300>(TW_GUILDS_TABLE, "DefaultRankID = '{}', Members = '{}' WHERE ID = '{}'",
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
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '{}'", m_pGuild->GetID());
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
	if(std::any_of(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&FromUID](const RequestData* p){ return p->GetFromUID() == FromUID; }))
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
	Database->Execute<DB::INSERT>(TW_GUILDS_INVITES_TABLE, "(GuildID, UserID) VALUES ('{}', '{}')", m_pGuild->GetID(), FromUID);
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
	if(Iter != m_aRequestsJoin.end())
	{
		// Remove the request from the database and delete it from memory
		Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '{}' AND UserID = '{}'", m_pGuild->GetID(), (*Iter)->GetFromUID());
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

		return Result;
	}

	return GuildResult::MEMBER_UNDEFINED_ERROR;
}

void CGuild::CRequestsManager::Deny(int UserID, const CMember* pFromMember)
{
	// Find the request in m_aRequestsJoin that matches the UserID
	auto Iter = std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&UserID](const RequestData* pRequest)
	{
		return pRequest->GetFromUID() == UserID;
	});

	// If the request was found
	if(Iter != m_aRequestsJoin.end())
	{
		// Remove the request from the database
		Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '{}' AND UserID = '{}'", m_pGuild->GetID(), (*Iter)->GetFromUID());

		// If pFromMember exists
		if(pFromMember)
		{
			// Get the nicknames of the users
			const char* pFromNickname = Instance::Server()->GetAccountNickname(UserID);
			const char* pByNickname = Instance::Server()->GetAccountNickname(pFromMember->GetAccountID());

			// Send a message to the user and the guild
			GS()->ChatAccount(UserID, "'{}' denied your invitation to join a guild '{}'.", pByNickname, m_pGuild->GetName());
			m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' denied invitation from '%s'.", pByNickname, pFromNickname);
			GS()->ChatGuild(m_pGuild->GetID(), "'{}' denied invitation from '{}'.", pByNickname, pFromNickname);
		}

		// Delete the request and remove it from m_aRequestsJoin
		delete (*Iter);
		m_aRequestsJoin.erase(Iter);
	}
}