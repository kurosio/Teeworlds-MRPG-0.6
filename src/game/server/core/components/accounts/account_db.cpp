#include "account_db.h"

#include <base/system.h>

#include <algorithm>

namespace AccountDb
{
	namespace
	{
		constexpr const char* ACCOUNTS_TABLE = "tw_accounts";
		constexpr const char* ACCOUNTS_DATA_TABLE = "tw_accounts_data";
		constexpr const char* SESSIONS_TABLE = "tw_account_sessions";
		constexpr size_t SESSION_BATCH_SIZE = 100;

		// Accounts and sessions live in the shared database of the server.
		constexpr Db::EKind KIND = Db::EKind::Remote;

		std::string Placeholders(size_t Count)
		{
			std::string Out;
			for(size_t i = 0; i < Count; ++i)
				Out += i ? ", ?" : "?";
			return Out;
		}
	}

	// ---------------------------------------------------------------------------
	// registration and login
	// ---------------------------------------------------------------------------
	SNickname ParseNickname(const Db::CResultSet& Result)
	{
		SNickname Nickname;
		// No rows is the answer "the nickname is free" - and also what a failed
		// query leaves behind, so the caller checks Result.Ok() first.
		if(!Result.HasRows())
			return Nickname;

		Nickname.m_Found = true;
		Nickname.m_AccountID = Result.GetInt("ID");
		Nickname.m_Username = Result.GetString("Username");
		Nickname.m_TimeoutCode = Result.GetString("TimeoutCode");
		return Nickname;
	}

	SCredentials ParseCredentials(const Db::CResultSet& Result)
	{
		SCredentials Credentials;
		if(!Result.HasRows())
			return Credentials;

		Credentials.m_Found = true;
		Credentials.m_AccountID = Result.GetInt("ID");
		Credentials.m_Nickname = Result.GetString("AccountNickname");
		Credentials.m_CredentialsFound = Result.GetBoolean("CredentialsFound");
		Credentials.m_Username = Result.GetString("Username");
		Credentials.m_TimeoutCode = Result.GetString("TimeoutCode");
		Credentials.m_LoginDate = Result.GetString("LoginDate");
		Credentials.m_Language = Result.GetString("Language");
		Credentials.m_PinCode = Result.GetString("PinCode");
		Credentials.m_Password = Result.GetString("Password");
		Credentials.m_PasswordSalt = Result.GetString("PasswordSalt");
		Credentials.m_HasBan = Result.GetBoolean("HasBan");
		Credentials.m_BannedUntil = Result.GetString("BannedUntil");
		Credentials.m_BanReason = Result.GetString("Reason");
		return Credentials;
	}

	Db::CRequest LookupNickname(std::string_view Nickname)
	{
		// Check the remote authority so a new nickname is immediately visible.
		return Db::Query(KIND,
			"SELECT d.ID, a.Username, a.TimeoutCode FROM tw_accounts_data d "
			"LEFT JOIN tw_accounts a ON a.ID = d.ID WHERE d.Nick = ? LIMIT 1",
			Db::Bind(Nickname));
	}

	Db::CRequest Authenticate(std::string_view Nickname, std::string_view Login)
	{
		// LEFT JOIN keeps the nickname/credentials error distinction of the caller.
		// CredentialsFound answers "does this account hold that login", so the login
		// is bound - not the nickname, which is the name the player plays under and
		// is usually not the username.
		return Db::Query(KIND,
			"SELECT d.ID, d.Nick AS AccountNickname, "
			"(a.ID IS NOT NULL AND a.Username = ?) AS CredentialsFound, a.Username, a.TimeoutCode, "
			"a.LoginDate, a.Language, a.PinCode, a.Password, a.PasswordSalt, "
			"(b.AccountId IS NOT NULL) AS HasBan, b.BannedUntil, b.Reason "
			"FROM tw_accounts_data d "
			"LEFT JOIN tw_accounts a ON a.ID = d.ID "
			"LEFT JOIN tw_accounts_bans b ON b.AccountId = a.ID AND CURRENT_TIMESTAMP() < b.BannedUntil "
			"WHERE d.Nick = ? LIMIT 1",
			Db::Bind(Login, Nickname));
	}

	Db::CRequest LoadAccount(int AccountID)
	{
		// Read from the remote authority; the account may have been registered
		// moments ago on another server.
		return Db::Query(KIND, "SELECT * FROM tw_accounts_data WHERE ID = ?",
			Db::Bind(AccountID));
	}

	Db::CRequest Register(const SRegistration& Data)
	{
		return Db::Transaction(KIND, [Data](Db::CTransaction& Tx)
		{
			auto Created = Tx.Execute("INSERT INTO tw_accounts "
				"(Username, Password, PasswordSalt, RegisterDate, RegisteredIP, Language) "
				"VALUES (?, ?, ?, UTC_TIMESTAMP(), ?, ?)",
				Db::Bind(Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, Data.m_RegisteredIP, Data.m_Language));
			if(!Created.Changed())
				return false;

			const int64_t AccountID = Created.InsertId();
			if(AccountID <= 0)
			{
				// Without an id the data row cannot be linked: roll back.
				dbg_msg("Account", "Registration of '%s' returned no account id.", Data.m_Nickname.c_str());
				return false;
			}

			// The data row carries the nickname; the rating default of the table
			// applies and sv_min_rating is enforced by the caller afterwards.
			return Tx.Execute("INSERT INTO tw_accounts_data (ID, Nick) VALUES (?, ?)",
				Db::Bind(AccountID, Data.m_Nickname)).Changed();
		});
	}

	Db::CRequest DeleteAccount(int AccountID)
	{
		return Db::Transaction(KIND, [AccountID](Db::CTransaction& Tx)
		{
			if(!Tx.Execute("DELETE FROM tw_accounts_data WHERE ID = ?", Db::Bind(AccountID)).Ok())
				return false;
			return Tx.Execute("DELETE FROM tw_accounts WHERE ID = ?", Db::Bind(AccountID)).Ok();
		});
	}

	Db::CRequest DeleteAccountByCredentials(std::string_view Login, std::string_view PasswordHash, std::string_view PasswordSalt)
	{
		// The transaction executes asynchronously after this function returns;
		// own the credentials rather than capturing caller-owned string_views.
		const std::string OwnedLogin(Login);
		const std::string OwnedPasswordHash(PasswordHash);
		const std::string OwnedPasswordSalt(PasswordSalt);
		return Db::Transaction(KIND, [OwnedLogin, OwnedPasswordHash, OwnedPasswordSalt](Db::CTransaction& Tx)
		{
			auto Found = Tx.Execute("SELECT ID FROM tw_accounts WHERE Username = ? AND Password = ? AND PasswordSalt = ? ORDER BY ID DESC LIMIT 1",
				Db::Bind(OwnedLogin, OwnedPasswordHash, OwnedPasswordSalt));
			if(!Found.Ok() || !Found.HasRows())
				return false;

			const int AccountID = Found.GetInt("ID");
			if(!Tx.Execute("DELETE FROM tw_accounts_data WHERE ID = ?", Db::Bind(AccountID)).Ok())
				return false;
			return Tx.Execute("DELETE FROM tw_accounts WHERE ID = ?", Db::Bind(AccountID)).Ok();
		});
	}

	Db::CRequest UpdateCredentials(int AccountID, std::string_view Login, std::string_view PasswordHash, std::string_view PasswordSalt)
	{
		return Db::Query(KIND, "UPDATE tw_accounts SET Username = ?, Password = ?, PasswordSalt = ? WHERE ID = ?",
			Db::Bind(Login, PasswordHash, PasswordSalt, AccountID));
	}

	Db::CRequest UpdateLoginStamp(int AccountID, std::string_view Address, std::string_view CountryIso)
	{
		return Db::Query(KIND,
			"UPDATE tw_accounts SET LoginDate = CURRENT_TIMESTAMP, LoginIP = ?, CountryISO = ? WHERE ID = ?",
			Db::Bind(Address, CountryIso, AccountID));
	}

	// ---------------------------------------------------------------------------
	// account sessions
	// ---------------------------------------------------------------------------
	std::optional<SSessionRecord> ParseSession(const Db::CResultSet& Result, int StaleSeconds)
	{
		// A failed query says nothing about ownership: the caller must not treat
		// it as "no session".
		if(!Result.Ok())
			return std::nullopt;

		SSessionRecord Record;
		if(!Result.HasRows())
			return Record; // exists == false

		Record.m_Exists = true;
		Record.m_ServerID = Result.GetInt("ServerID");
		Record.m_ClientID = Result.GetInt("ClientID");
		Record.m_Token = Result.GetString("SessionToken");
		Record.m_Heartbeat = Result.GetInt("Heartbeat");

		const int Now = Result.GetInt("CurrentUnixTime");
		if(Record.m_Heartbeat == 0)
			Record.m_HeartbeatAge = StaleSeconds; // the owning login never finished
		else if(Record.m_Heartbeat > 1 && Now >= Record.m_Heartbeat)
			Record.m_HeartbeatAge = Now - Record.m_Heartbeat;
		return Record;
	}

	std::vector<SSessionKey> ParseSessionTokens(const Db::CResultSet& Result)
	{
		std::vector<SSessionKey> vRows;
		if(!Result.Ok())
			return vRows;

		Db::CResultSet Rows(Result);
		while(Rows.Next())
			vRows.push_back({ Rows.GetInt("AccountID"), Rows.GetString("SessionToken") });
		return vRows;
	}

	Db::CRequest ReadSession(int AccountID)
	{
		// The row arbitrates between servers: a stale snapshot fails the claim.
		return Db::Query(KIND,
			"SELECT ServerID, ClientID, SessionToken, Heartbeat, UNIX_TIMESTAMP() AS CurrentUnixTime "
			"FROM tw_account_sessions WHERE AccountID = ?",
			Db::Bind(AccountID));
	}

	Db::CRequest ClaimSession(const SSessionClaim& Claim, int StaleSeconds)
	{
		if(!Claim.m_Previous.m_Exists)
		{
			// The row was absent when read. ON DUPLICATE KEY UPDATE with a no-op
			// assignment reports 0 rows if someone created it meanwhile.
			return Db::Query(KIND,
				"INSERT INTO tw_account_sessions (AccountID, ServerID, ClientID, SessionToken, Heartbeat, UpdatedAt) "
				"VALUES (?, ?, ?, ?, 0, NOW()) ON DUPLICATE KEY UPDATE AccountID = AccountID",
				Db::Bind(Claim.m_AccountID, Claim.m_ServerID, Claim.m_ClientID, Claim.m_Token));
		}

		// Compare and swap: matches only while the row still carries the token we
		// read and its owner may be replaced.
		return Db::Query(KIND,
			"UPDATE tw_account_sessions SET ServerID = ?, ClientID = ?, SessionToken = ?, Heartbeat = 0, UpdatedAt = NOW() "
			"WHERE AccountID = ? AND SessionToken = ? AND (Heartbeat <> 0 OR UpdatedAt < NOW() - INTERVAL ? SECOND)",
			Db::Bind(Claim.m_ServerID, Claim.m_ClientID, Claim.m_Token, Claim.m_AccountID, Claim.m_Previous.m_Token, StaleSeconds));
	}

	Db::CRequest ActivateSession(const SSessionClaim& Claim)
	{
		return Db::Query(KIND,
			"UPDATE tw_account_sessions SET Heartbeat = UNIX_TIMESTAMP(), UpdatedAt = NOW() "
			"WHERE AccountID = ? AND SessionToken = ? AND Heartbeat = 0",
			Db::Bind(Claim.m_AccountID, Claim.m_Token));
	}

	Db::CRequest ReleaseSession(const SSessionClaim& Claim)
	{
		if(!Claim.m_Previous.m_Exists)
		{
			return Db::Query(KIND,
				"DELETE FROM tw_account_sessions WHERE AccountID = ? AND SessionToken = ? AND Heartbeat = 0",
				Db::Bind(Claim.m_AccountID, Claim.m_Token));
		}

		const SSessionRecord& Previous = Claim.m_Previous;
		return Db::Query(KIND,
			"UPDATE tw_account_sessions SET ServerID = ?, ClientID = ?, SessionToken = ?, Heartbeat = ?, UpdatedAt = NOW() "
			"WHERE AccountID = ? AND SessionToken = ? AND Heartbeat = 0",
			Db::Bind(Previous.m_ServerID, Previous.m_ClientID, Previous.m_Token, Previous.m_Heartbeat, Claim.m_AccountID, Claim.m_Token));
	}

	Db::CRequest RemoveSession(const SSessionKey& Key)
	{
		return Db::Query(KIND, "DELETE FROM tw_account_sessions WHERE AccountID = ? AND SessionToken = ?",
			Db::Bind(Key.m_AccountID, Key.m_Token));
	}

	Db::CRequest RefreshSessionHeartbeats(const std::vector<SSessionKey>& vKeys)
	{
		if(vKeys.empty())
			return {};

		// A single UPDATE is already atomic; avoid three transaction round trips
		// for the common case. Keep the transaction for multiple batches.
		if (vKeys.size() <= SESSION_BATCH_SIZE)
		{
			Db::CBindings Bindings;
			std::string Conditions;
			for (const auto& Key : vKeys)
			{
				if (!Conditions.empty())
					Conditions += " OR ";
				Conditions += "(AccountID = ? AND SessionToken = ?)";
				Bindings.Add(Key.m_AccountID).Add(Key.m_Token);
			}
			return Db::Query(KIND, "UPDATE tw_account_sessions SET Heartbeat = UNIX_TIMESTAMP(), UpdatedAt = NOW() "
				"WHERE Heartbeat <> 0 AND (" + Conditions + ")", Bindings);
		}

		// Batched heartbeat updates use the remote endpoint.
		return Db::Transaction(KIND, [vKeys](Db::CTransaction& Tx)
		{
			for(size_t Begin = 0; Begin < vKeys.size(); Begin += SESSION_BATCH_SIZE)
			{
				const size_t End = std::min(vKeys.size(), Begin + SESSION_BATCH_SIZE);
				Db::CBindings Bindings;
				std::string Conditions;
				for(size_t i = Begin; i < End; ++i)
				{
					if(!Conditions.empty())
						Conditions += " OR ";
					Conditions += "(AccountID = ? AND SessionToken = ?)";
					Bindings.Add(vKeys[i].m_AccountID).Add(vKeys[i].m_Token);
				}
				auto Updated = Tx.Execute(
					"UPDATE tw_account_sessions SET Heartbeat = UNIX_TIMESTAMP(), UpdatedAt = NOW() "
					"WHERE Heartbeat <> 0 AND (" + Conditions + ")", Bindings);
				if(!Updated.Ok())
					return false;
			}
			return true;
		});
	}

	Db::CRequest FetchSessionTokens(const std::vector<int>& vAccountIDs)
	{
		if(vAccountIDs.empty())
			return {};

		Db::CBindings Bindings;
		for(const int AccountID : vAccountIDs)
			Bindings.Add(AccountID);

		// A stale token here would kick a session that has just claimed its row.
		return Db::Query(KIND,
			"SELECT AccountID, SessionToken FROM tw_account_sessions WHERE AccountID IN (" + Placeholders(vAccountIDs.size()) + ")",
			Bindings);
	}

	// ---------------------------------------------------------------------------
	// the profile of a logged in account
	// ---------------------------------------------------------------------------
	Db::CRequest ReadPinCodeAndPassword(int AccountID)
	{
		return Db::Query(KIND, "SELECT PinCode, Password, PasswordSalt FROM tw_accounts WHERE ID = ?",
			Db::Bind(AccountID));
	}

	Db::CRequest SavePinCode(int AccountID, std::string_view PinCode)
	{
		return Db::Query(KIND, "UPDATE tw_accounts SET PinCode = ? WHERE ID = ?", Db::Bind(PinCode, AccountID));
	}

	Db::CRequest SavePassword(int AccountID, std::string_view PasswordHash, std::string_view PasswordSalt)
	{
		return Db::Query(KIND, "UPDATE tw_accounts SET Password = ?, PasswordSalt = ? WHERE ID = ?",
			Db::Bind(PasswordHash, PasswordSalt, AccountID));
	}

	Db::CRequest FindNicknameOwner(std::string_view Nickname)
	{
		return Db::Query(KIND, "SELECT ID FROM tw_accounts_data WHERE Nick = ?", Db::Bind(Nickname));
	}

	Db::CRequest SaveNickname(int AccountID, std::string_view Nickname)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET Nick = ? WHERE ID = ?", Db::Bind(Nickname, AccountID));
	}

	Db::CRequest SaveTimeoutCode(std::string_view TimeoutCode, std::string_view Nickname)
	{
		return Db::Query(KIND,
			"UPDATE tw_accounts a JOIN tw_accounts_data d ON d.ID = a.ID "
			"SET a.TimeoutCode = ? WHERE d.Nick = ? AND a.TimeoutCode != ?",
			Db::Bind(TimeoutCode, Nickname, TimeoutCode));
	}

	// ---------------------------------------------------------------------------
	// vouchers
	// ---------------------------------------------------------------------------
	Db::CRequest FindVoucher(int AccountID, std::string_view Code)
	{
		// The statement answers with the voucher row and the flag whether this
		// account used it before; the game decides what that means for a code
		// that may be redeemed many or one time.
		return Db::Query(KIND,
			"SELECT v.*, IF((SELECT r.ID FROM tw_voucher_redeemed r WHERE CASE v.Multiple WHEN 1 THEN r.VoucherID = v.ID AND r.UserID = ? "
			"ELSE r.VoucherID = v.ID END) IS NULL, FALSE, TRUE) AS used FROM tw_voucher v WHERE v.Code = ?",
			Db::Bind(AccountID, Code));
	}

	Db::CRequest RedeemVoucher(int VoucherID, int AccountID, int64_t CreatedAt)
	{
		return Db::Query(KIND, "INSERT INTO tw_voucher_redeemed (VoucherID, UserID, TimeCreated) VALUES (?, ?, ?)",
			Db::Bind(VoucherID, AccountID, CreatedAt));
	}

	// ---------------------------------------------------------------------------
	// bans
	// ---------------------------------------------------------------------------
	Db::CRequest ReadActiveBan(int AccountID)
	{
		return Db::Query(KIND, "SELECT BannedUntil FROM tw_accounts_bans WHERE AccountId = ? AND current_timestamp() < `BannedUntil`",
			Db::Bind(AccountID));
	}

	Db::CRequest CreateBan(int AccountID, int64_t DurationSeconds, std::string_view Reason)
	{
		return Db::Query(KIND,
			"INSERT INTO tw_accounts_bans (AccountId, BannedUntil, Reason) "
			"VALUES (?, TIMESTAMPADD(SECOND, ?, CURRENT_TIMESTAMP()), ?)",
			Db::Bind(AccountID, DurationSeconds, Reason));
	}

	Db::CRequest ReadActiveBanByID(int BanID)
	{
		return Db::Query(KIND, "SELECT AccountId FROM tw_accounts_bans WHERE Id = ? AND current_timestamp() < `BannedUntil`",
			Db::Bind(BanID));
	}

	Db::CRequest LiftBan(int BanID)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_bans SET BannedUntil = current_timestamp WHERE Id = ?", Db::Bind(BanID));
	}

	Db::CRequest ListActiveBans()
	{
		return Db::Query(KIND, "SELECT Id, BannedUntil, Reason, AccountId FROM tw_accounts_bans WHERE current_timestamp() < `BannedUntil`");
	}

	// ---------------------------------------------------------------------------
	// the tables of one account
	// ---------------------------------------------------------------------------
	Db::CRequest LoadProfessions(int AccountID)
	{
		return Db::Query(KIND, "SELECT ProfessionID, Data FROM tw_accounts_professions WHERE UserID = ?", Db::Bind(AccountID));
	}

	Db::CRequest CreateProfessions(int AccountID, const std::vector<SProfessionData>& vProfessions)
	{
		if(vProfessions.empty())
			return Db::Query(KIND, "INSERT INTO tw_accounts_professions (UserID, ProfessionID, Data) SELECT ?, ?, ? WHERE 1 = 0",
				Db::Bind(AccountID, 0, ""));

		std::string Values;
		Db::CBindings Bindings;
		for(const auto& Profession : vProfessions)
		{
			if(!Values.empty())
				Values += ", ";
			Values += "(?, ?, ?)";
			Bindings.Add(AccountID);
			Bindings.Add(Profession.m_ProfessionID);
			Bindings.Add(Profession.m_Data);
		}
		return Db::Query(KIND,
			"INSERT INTO tw_accounts_professions (UserID, ProfessionID, Data) VALUES " + Values,
			Bindings);
	}

	Db::CRequest SaveProfession(int AccountID, int ProfessionID, std::string_view Data)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_professions SET Data = ? WHERE ProfessionID = ? AND UserID = ?",
			Db::Bind(Data, ProfessionID, AccountID));
	}

	Db::CRequest SaveEquippedSlots(int AccountID, std::string_view Json)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET EquippedSlots = ? WHERE ID = ?", Db::Bind(Json, AccountID));
	}

	Db::CRequest SaveEquipmentPresets(int AccountID, std::string_view Json)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET EquipmentPresets = ? WHERE ID = ?", Db::Bind(Json, AccountID));
	}

	Db::CRequest SaveRating(int AccountID, int Rating)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET Rating = ? WHERE ID = ?", Db::Bind(Rating, AccountID));
	}

	// The level of a profession is stored inside its document, so the reduction
	// has to happen in the database: every account holds one row per profession,
	// and reading them all would transfer the whole table on a populated server.
	//
	// Expression detail: the level is the text after the last "level": of the
	// document and CAST reads its leading digits. No JSON function is involved,
	// so the statement does not depend on the database version.
	//
	// MIN() picks an account when several share the same best level - the
	// deterministic choice, so the leader of a profession does not change while
	// the level does not.
	Db::CRequest ListProfessionLeaders(std::string_view Tag)
	{
		return Db::Query(KIND,
			"SELECT MIN(UserID) AS UserID, ProfessionID, MAX(Level) AS Level FROM ("
			"SELECT UserID, ProfessionID, "
			"CAST(SUBSTRING_INDEX(LOWER(Data), '\"level\":', -1) AS SIGNED) AS Level "
			"FROM tw_accounts_professions WHERE Data LIKE '%\"level\"%'"
			") AS ranked GROUP BY ProfessionID HAVING Level > 0",
			{}, Db::SQueryOptions().Coalesce().Tag(Tag));
	}

	// ---------------------------------------------------------------------------
	// the account columns the controller saves
	// ---------------------------------------------------------------------------
	Db::CRequest SaveBank(int AccountID, std::string_view Bank)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET Bank = ? WHERE ID = ?", Db::Bind(Bank, AccountID));
	}

	Db::CRequest SaveCrimeScore(int AccountID, int64_t CrimeScore)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET CrimeScore = ? WHERE ID = ?", Db::Bind(CrimeScore, AccountID));
	}

	Db::CRequest SaveProfessionID(int AccountID, int ProfessionID)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET ProfessionID = ? WHERE ID = ?", Db::Bind(ProfessionID, AccountID));
	}

	Db::CRequest SaveWorldID(int AccountID, int WorldID)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET WorldID = ? WHERE ID = ?", Db::Bind(WorldID, AccountID));
	}

	Db::CRequest SaveStamps(int AccountID, int64_t Daily, int64_t Week, int64_t Month)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_data SET DailyStamp = ?, WeekStamp = ?, MonthStamp = ? WHERE ID = ?",
			Db::Bind(Daily, Week, Month, AccountID));
	}

	Db::CRequest SaveLanguage(int AccountID, std::string_view Language)
	{
		return Db::Query(KIND, "UPDATE tw_accounts SET Language = ? WHERE ID = ?", Db::Bind(Language, AccountID));
	}

	Db::CRequest SaveUsername(int AccountID, std::string_view Username)
	{
		return Db::Query(KIND, "UPDATE tw_accounts SET Username = ? WHERE ID = ?", Db::Bind(Username, AccountID));
	}

	Db::CRequest FindAccountByNickname(std::string_view Nickname)
	{
		// The remote authority must show the row the login just wrote.
		return Db::Query(KIND,
			"SELECT d.ID, d.Nick, a.Username, a.TimeoutCode FROM tw_accounts_data d JOIN tw_accounts a ON a.ID = d.ID WHERE d.Nick = ?",
			Db::Bind(Nickname));
	}
}
