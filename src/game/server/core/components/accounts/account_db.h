#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_DB_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_DB_H

#include <engine/server/db/db_async.h>

namespace AccountDb
{
	// A nickname as it is stored in tw_accounts_data / tw_accounts.
	struct SNickname
	{
		bool m_Found = false;
		int m_AccountID = 0;
		std::string m_Username;
		std::string m_TimeoutCode;
	};

	// The row of a login attempt: credentials, profile and ban state.
	struct SCredentials
	{
		bool m_Found = false; // the nickname exists
		int m_AccountID = 0;
		std::string m_Nickname;
		std::string m_Username;
		std::string m_TimeoutCode;
		std::string m_LoginDate;
		std::string m_Language;
		std::string m_PinCode;
		std::string m_Password;
		std::string m_PasswordSalt;
		bool m_CredentialsFound = false; // the account row holds exactly that login
		bool m_HasBan = false;
		std::string m_BannedUntil;
		std::string m_BanReason;
	};

	// What registration writes.
	struct SRegistration
	{
		std::string m_Login;
		std::string m_PasswordHash;
		std::string m_PasswordSalt;
		std::string m_Nickname;
		std::string m_RegisteredIP;
		std::string m_Language;
	};

	// --- queries ------------------------------------------------------------
	// Checks the authoritative remote database so a new account is visible immediately.
	Db::CRequest LookupNickname(std::string_view Nickname);

	// The login query: credentials, profile and an active ban, in one statement.
	// The row belongs to the nickname (the name the player uses in game); the
	// login is what the credentials have to match.
	Db::CRequest Authenticate(std::string_view Nickname, std::string_view Login);

	// The account row of tw_accounts_data, for the account to be loaded.
	Db::CRequest LoadAccount(int AccountID);

	// Creates tw_accounts and tw_accounts_data in one transaction. The generated
	// account id is the InsertId() of the result (the first insert of the
	// transaction, see CTransaction).
	Db::CRequest Register(const SRegistration& Data);

	// Removes an account created by a failed registration.
	Db::CRequest DeleteAccount(int AccountID);
	// Fallback only for an ambiguous registration commit when its inserted id is unknown.
	Db::CRequest DeleteAccountByCredentials(std::string_view Login, std::string_view PasswordHash, std::string_view PasswordSalt);

	// Credentials of an account that a guest registration claims.
	Db::CRequest UpdateCredentials(int AccountID, std::string_view Login, std::string_view PasswordHash, std::string_view PasswordSalt);

	// Login stamp of a successful authentication.
	Db::CRequest UpdateLoginStamp(int AccountID, std::string_view Address, std::string_view CountryIso);

	// --- pure row readers (no database access) ------------------------------
	SNickname ParseNickname(const Db::CResultSet& Result);
	SCredentials ParseCredentials(const Db::CResultSet& Result);

	// --- account sessions (tw_account_sessions) -----------------------------
	// The row that decides who may play an account. Every write is a compare and
	// swap on the token: a decision taken on an old read is refused by the
	// database instead of overwriting a newer owner.
	struct SSessionKey
	{
		int m_AccountID = 0;
		std::string m_Token;
	};

	struct SSessionRecord
	{
		bool m_Exists = false;
		int m_ServerID = 0;
		int m_ClientID = 0;
		std::string m_Token;
		int m_Heartbeat = 0;    // 0: the owning login never finished
		int m_HeartbeatAge = -1; // seconds since the last heartbeat, -1 if unknown
	};

	struct SSessionClaim
	{
		int m_AccountID = 0;
		int m_ClientID = 0;
		int m_ServerID = 0;
		std::string m_Token;
		SSessionRecord m_Previous;
		int64_t m_Deadline = 0;
	};

	// Reads the current owner. A result without rows is a valid answer (free
	// account); a failed query is not.
	Db::CRequest ReadSession(int AccountID);
	// Takes the account if the row still holds the token we read.
	Db::CRequest ClaimSession(const SSessionClaim& Claim, int StaleSeconds);
	// Turns a reservation into an active session (heartbeat set).
	Db::CRequest ActivateSession(const SSessionClaim& Claim);
	// Gives the reservation back: restores the previous row or removes ours.
	Db::CRequest ReleaseSession(const SSessionClaim& Claim);
	// Removes an active session on disconnect; matches only that token.
	Db::CRequest RemoveSession(const SSessionKey& Key);
	// Refreshes the heartbeats of the given sessions in one statement.
	Db::CRequest RefreshSessionHeartbeats(const std::vector<SSessionKey>& vKeys);
	// Tokens currently stored for the given accounts in the remote database.
	Db::CRequest FetchSessionTokens(const std::vector<int>& vAccountIDs);

	// Row readers of the session statements.
	std::optional<SSessionRecord> ParseSession(const Db::CResultSet& Result, int StaleSeconds);
	std::vector<SSessionKey> ParseSessionTokens(const Db::CResultSet& Result);

	// --- the profile of a logged in account ---------------------------------
	// The statements the account menu runs: a credential change reads the current
	// values, checks them in the game and writes the new one back.
	Db::CRequest ReadPinCodeAndPassword(int AccountID);
	Db::CRequest SavePinCode(int AccountID, std::string_view PinCode);
	Db::CRequest SavePassword(int AccountID, std::string_view PasswordHash, std::string_view PasswordSalt);

	// Rechecks nickname ownership in the authoritative remote database.
	Db::CRequest FindNicknameOwner(std::string_view Nickname);
	Db::CRequest SaveNickname(int AccountID, std::string_view Nickname);

	// The timeout code the client shows instead of a kick reason.
	Db::CRequest SaveTimeoutCode(std::string_view TimeoutCode, std::string_view Nickname);

	// --- vouchers -----------------------------------------------------------
	// One voucher by its code, with the flag whether this account used it already.
	Db::CRequest FindVoucher(int AccountID, std::string_view Code);
	Db::CRequest RedeemVoucher(int VoucherID, int AccountID, int64_t CreatedAt);

	// --- bans ---------------------------------------------------------------
	Db::CRequest ReadActiveBan(int AccountID);
	Db::CRequest CreateBan(int AccountID, int64_t DurationSeconds, std::string_view Reason);
	Db::CRequest ReadActiveBanByID(int BanID);
	Db::CRequest LiftBan(int BanID);
	Db::CRequest ListActiveBans();

	// --- the tables of one account (content, not credentials) ---------------
	struct SProfessionData
	{
		int m_ProfessionID;
		std::string m_Data;
	};

	Db::CRequest LoadProfessions(int AccountID);
	// The best account of every profession: exactly the rows the leveling
	// tracker and the specialist leaderboard need. The level lives inside the
	// profession document, so the maximum is computed by the database and only
	// one row per profession is transferred. Tag names the request for menus.
	Db::CRequest ListProfessionLeaders(std::string_view Tag);
	Db::CRequest CreateProfessions(int AccountID, const std::vector<SProfessionData>& vProfessions);
	Db::CRequest SaveProfession(int AccountID, int ProfessionID, std::string_view Data);

	Db::CRequest SaveEquippedSlots(int AccountID, std::string_view Json);
	Db::CRequest SaveEquipmentPresets(int AccountID, std::string_view Json);
	Db::CRequest SaveRating(int AccountID, int Rating);

	// --- the account columns the controller saves ---------------------------
	// One statement per saved group, so a save never writes a column it was not
	// asked about.
	Db::CRequest SaveBank(int AccountID, std::string_view Bank);
	Db::CRequest SaveCrimeScore(int AccountID, int64_t CrimeScore);
	Db::CRequest SaveProfessionID(int AccountID, int ProfessionID);
	Db::CRequest SaveWorldID(int AccountID, int WorldID);
	Db::CRequest SaveStamps(int AccountID, int64_t Daily, int64_t Week, int64_t Month);
	Db::CRequest SaveLanguage(int AccountID, std::string_view Language);
	Db::CRequest SaveUsername(int AccountID, std::string_view Username);

	// The account of a nickname, for the client info the server answers with.
	Db::CRequest FindAccountByNickname(std::string_view Nickname);
}

#endif // GAME_SERVER_COMPONENT_ACCOUNT_DB_H
