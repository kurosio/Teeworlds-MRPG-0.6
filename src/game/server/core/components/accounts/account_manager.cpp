/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_manager.h"

#include <base/hash_ctxt.h>
#include <base/system.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include <game/server/worldmodes/default.h>

#include <casino/dice_duel_game.h>
#include <casino/roulette_game.h>

#include <game/server/core/components/inventory/item_data.h>
#include <game/server/core/components/inventory/inventory_manager.h>
#include <game/server/core/components/mails/mailbox_manager.h>
#include <game/server/core/components/worlds/world_data.h>
#include <game/server/core/tools/db_async_context.h>

#include <teeother/components/localization.h>

#include <algorithm>
#include <optional>

constexpr int LENGTH_LOGPASS_MAX = 12;
constexpr int LENGTH_LOGPASS_MIN = 4;
constexpr int LENGTH_PIN_MAX = 6;
constexpr int LENGTH_PIN_MIN = 4;
constexpr int BET_FIELD_AMOUNT = 0;
constexpr int AUTH_FIELD_PASSWORD = 1;

namespace
{
	constexpr const char* ACCOUNT_SESSION_TAKEOVER_REASON = "Account session was taken over";
	constexpr int ACCOUNT_SESSION_TAKEOVER_TIMEOUT_SECONDS = 10;
	constexpr int ACCOUNT_SESSION_CLAIM_ATTEMPTS = 3;

	// The remote database can have WAN latency, so a login may wait several seconds.
	constexpr int64_t AUTH_PROGRESS_FIRST_SECONDS = 5;
	constexpr int64_t AUTH_PROGRESS_REPEAT_SECONDS = 15;

	void StartAuthProgress(CAccountSharedData& Shared, const std::shared_ptr<void>& pAttempt, bool Registration)
	{
		Shared.m_pAuthorizationAttempt = pAttempt;
		Shared.m_AuthIsRegistration = Registration;
		Shared.m_AuthStartedAt = time_get();
		Shared.m_AuthProgressTick = time_get() + AUTH_PROGRESS_FIRST_SECONDS * time_freq();
	}

	void TickAuthProgress()
	{
		static int64_t s_NextCheck = 0; // notices are seconds apart: scan at 1 Hz, not per tick
		const int64_t Now = time_get();
		if(Now < s_NextCheck)
			return;
		s_NextCheck = Now + time_freq();

		std::vector<std::pair<int, bool>> vNotices;
		for(auto& [ClientID, Shared] : CAccountSharedData::ms_aPlayerSharedData)
		{
			if(Shared.m_pAuthorizationAttempt.expired() || Now < Shared.m_AuthProgressTick)
				continue;

			Shared.m_AuthProgressTick = Now + AUTH_PROGRESS_REPEAT_SECONDS * time_freq();
			vNotices.emplace_back(ClientID, Shared.m_AuthIsRegistration);
		}

		// Sent after the loop: Chat can add entries to the map we iterate.
		for(const auto& [ClientID, Registration] : vNotices)
			if(auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID)))
				pGS->Chat(ClientID, Registration ? "Registration is still in progress. Please wait..." : "Login is still in progress. Please wait...");
	}

	// A previous owner on another server has to end its session before we play.
	bool NeedsRemoteTakeover(const AccountSession::SClaim& Claim)
	{
		return Claim.m_Previous.m_ServerID > 0 && Claim.m_Previous.m_ServerID != g_Config.m_SvSyncServerID;
	}

	bool CheckLoginPasswordLength(CGS* pGS, int ClientID, const char* pLogin, const char* pPassword)
	{
		const int LengthLogin = str_length(pLogin);
		const int LengthPassword = str_length(pPassword);
		if(LengthLogin <= LENGTH_LOGPASS_MAX && LengthLogin >= LENGTH_LOGPASS_MIN
			&& LengthPassword <= LENGTH_LOGPASS_MAX && LengthPassword >= LENGTH_LOGPASS_MIN)
			return true;

		pGS->Chat(ClientID, "The username and password must each contain '{} - {} characters'.", LENGTH_LOGPASS_MIN, LENGTH_LOGPASS_MAX);
		return false;
	}
}

class DbAuthorization
{
	struct CAuthorizationPayload
	{
		CAccountManager* m_pAccountManager{};
		std::string m_Login{};
		std::string m_Password{};
		std::string m_Language{};
		std::string m_LoginDate{};
		std::string m_PinCode{};
		AccountSession::SClaim m_Claim{};
		int m_Attempts{};
		// The account row is read while the session of the account is taken, so
		// the login does not spend a round trip on it after the claim.
		Db::CRequest m_AccountRequest{};
	};
	using CAuthorizationContextPtr = std::shared_ptr<DbAsync::CContext<CAuthorizationPayload>>;

	static CPlayer* GetPlayer(const CAuthorizationContextPtr& pContext)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer || pPlayer->GetSharedData().m_pAuthorizationAttempt.lock().get() != pContext.get()
			|| pPlayer->Account()->GetID() > 0)
			return nullptr;
		return pPlayer;
	}

	// One login query, one step of checks: everything the decision needs arrives
	// in a single row, so the pipeline cannot read the account twice and take the
	// two answers for the same state.
	static void OnCheckNickname(const CAuthorizationContextPtr& pContext, Db::CResultSet&& Result)
	{
		auto* pPlayer = GetPlayer(pContext);
		if(!pPlayer)
			return;

		if(!Result.Ok())
		{
			dbg_msg("Account", "Login query failed: %s", Result.Error().ToString().c_str());
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: the account database is not reachable.");
			pContext->GS()->Chat(pContext->GetClientID(), "Please try again a bit later.");
			return;
		}

		const AccountDb::SCredentials Credentials = AccountDb::ParseCredentials(Result);
		if(!Credentials.m_Found)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: account with this nickname was not found.");
			return;
		}

		pContext->Data().m_Claim.m_AccountID = Credentials.m_AccountID;
		OnCheckCredentials(pContext, Credentials);
	}

	static void OnCheckCredentials(const CAuthorizationContextPtr& pContext, const AccountDb::SCredentials& Credentials)
	{
		auto* pPlayer = GetPlayer(pContext);
		if(!pPlayer)
			return;

		if(!Credentials.m_CredentialsFound ||
			Credentials.m_Password != CAccountManager::HashPassword(pContext->Data().m_Password, Credentials.m_PasswordSalt))
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: incorrect login or password.");
			return;
		}

		// All entry points, including /login, must enforce guest protection.
		if(CAccountManager::IsGuestCredentialForNickname(Credentials.m_Username, Credentials.m_Nickname.c_str()) &&
			!Credentials.m_TimeoutCode.empty() && Credentials.m_TimeoutCode != pPlayer->GetSharedData().m_TimeoutCode)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Invalid timeout code. Please try /timeout <code> again.");
			return;
		}

		auto& Data = pContext->Data();
		Data.m_Language = Credentials.m_Language;
		Data.m_LoginDate = Credentials.m_LoginDate;
		Data.m_PinCode = Credentials.m_PinCode;
		OnCheckBans(pContext, Credentials);
	}

	static void OnCheckBans(const CAuthorizationContextPtr& pContext, const AccountDb::SCredentials& Credentials)
	{
		auto* pPlayer = GetPlayer(pContext);
		if(!pPlayer)
			return;

		if(Credentials.m_HasBan)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Account is temporarily blocked.");
			pContext->GS()->Chat(pContext->GetClientID(), "Until: '{}'. Reason: '{}'.", Credentials.m_BannedUntil, Credentials.m_BanReason);
			return;
		}

		// The row the account is created from does not depend on the session, so
		// it is asked for now: its answer is ready by the time the claim is ours.
		pContext->Data().m_AccountRequest = AccountDb::LoadAccount(Credentials.m_AccountID);
		ReadOwnerAndClaim(pContext);
	}

	// Reads the owner and tries to take the account. Repeated if a claim is refused.
	static void ReadOwnerAndClaim(const CAuthorizationContextPtr& pContext)
	{
		AccountSession::ReadRecord(pContext->Data().m_Claim.m_AccountID,
			[pContext](std::optional<AccountSession::SRecord> Owner) { OnReadOwner(pContext, Owner); });
	}

	static void OnReadOwner(const CAuthorizationContextPtr& pContext, std::optional<AccountSession::SRecord> Owner)
	{
		auto* pPlayer = GetPlayer(pContext);
		if(!pPlayer)
			return;

		// Without a successful read the previous owner is unknown: do not claim blind.
		if(!Owner)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: session state could not be read.");
			pContext->GS()->Chat(pContext->GetClientID(), "Please try again a bit later.");
			return;
		}

		auto& Claim = pContext->Data().m_Claim;
		Claim.m_Previous = *Owner;
		AccountSession::TryClaim(Claim, [pContext](bool Claimed) { OnClaimed(pContext, Claimed); });
	}

	static void OnClaimed(const CAuthorizationContextPtr& pContext, bool Claimed)
	{
		const auto& Claim = pContext->Data().m_Claim;
		if(Claimed && GetPlayer(pContext) && time_get() < Claim.m_Deadline)
		{
			LoadAccountData(pContext);
			return;
		}

		// Refused, lost or too late. Give back whatever the write did (a no-op unless
		// our token is stored), then decide whether to try again.
		AccountSession::Release(Claim, [pContext](bool) { OnReleased(pContext); });
	}

	static void OnReleased(const CAuthorizationContextPtr& pContext)
	{
		auto* pPlayer = GetPlayer(pContext);
		if(!pPlayer)
			return;

		auto& Data = pContext->Data();
		if(time_get() >= Data.m_Claim.m_Deadline)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: session reservation expired. Please try again.");
			return;
		}

		if(++Data.m_Attempts < ACCOUNT_SESSION_CLAIM_ATTEMPTS)
		{
			ReadOwnerAndClaim(pContext);
			return;
		}
		pContext->GS()->Chat(pContext->GetClientID(), "Login failed: another login is in progress.");
	}

	static void LoadAccountData(const CAuthorizationContextPtr& pContext)
	{
		auto& Data = pContext->Data();
		auto Load = std::move(Data.m_AccountRequest);
		Data.m_AccountRequest = {};

		// The read was started with the session step; only a login that reached
		// this point without one reads the row here.
		if(!Load)
			Load = AccountDb::LoadAccount(Data.m_Claim.m_AccountID);

		Load.Then([pContext](Db::CResultSet&& Result) { OnLoadAccountData(pContext, std::move(Result)); });
	}

	static void OnLoadAccountData(const CAuthorizationContextPtr& pContext, Db::CResultSet&& Result)
	{
		auto* pPlayer = GetPlayer(pContext);
		auto& Data = pContext->Data();
		const auto& Claim = Data.m_Claim;
		if(!pPlayer || time_get() >= Claim.m_Deadline)
		{
			AccountSession::Release(Claim);
			return;
		}

		// A failed read and an empty account row are different problems; neither
		// may leave the reservation behind.
		if(!Result.Ok() || !Result.Next())
		{
			AccountSession::Release(Claim);
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: account data could not be loaded.");
			pContext->GS()->Chat(pContext->GetClientID(), "Please reconnect and try again.");
			return;
		}

		if(!pPlayer->GetSharedData().m_GuestLogin.empty() && Data.m_Login != pPlayer->GetSharedData().m_GuestLogin)
			pPlayer->GetSharedData().m_GuestLogin.clear();

		pPlayer->Account()->Init(Claim.m_AccountID, pContext->GetClientID(), Data.m_Login.c_str(), Data.m_Language,
			Data.m_LoginDate, Result);
		pPlayer->Account()->SetSessionToken(Claim.m_Token);
		if(time_get() >= Claim.m_Deadline)
		{
			AccountSession::Release(Claim);
			pContext->Server()->Kick(pContext->GetClientID(), "Account session reservation expired");
			return;
		}
		Data.m_pAccountManager->BeginSessionTakeover(pPlayer, Claim, Data.m_PinCode.empty() && !pPlayer->IsGuestLogin());
	}

public:
	static void StartAuthorization(CAccountManager* pMgr, int ClientID, std::string Login, std::string Pass, std::string Nick)
	{
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
		if(!pPlayer || !pPlayer->GetSharedData().m_pAuthorizationAttempt.expired() || pPlayer->Account()->GetID() > 0)
			return;

		CAuthorizationPayload Payload {};
		Payload.m_pAccountManager = pMgr;
		Payload.m_Login = Login;
		Payload.m_Password = Pass;
		Payload.m_Claim.m_ClientID = ClientID;
		Payload.m_Claim.m_Token = AccountSession::GenerateToken();
		Payload.m_Claim.m_Deadline = time_get() + AccountSession::CLAIM_SECONDS * time_freq();
		auto pContext = DbAsync::MakeContext<DbAuthorization::CAuthorizationPayload>(ClientID, std::move(Payload));

		StartAuthProgress(pPlayer->GetSharedData(), pContext, false);

		// The nickname, the password hash check and the ban arrive in one row.
		AccountDb::Authenticate(Nick, Login).Then([pContext](Db::CResultSet&& Result)
		{
			DbAuthorization::OnCheckNickname(pContext, std::move(Result));
		});
	}
};

class DbRegistration
{
	struct CRegistrationPayload
	{
		CAccountManager* m_pAccountManager{};
		int m_InitID{};
		std::string m_Login{};
		std::string m_Password{};
		std::string m_PasswordHash{};
		std::string m_PasswordSalt{};
		std::string m_Nickname{};
		std::string m_RegisteredIP{};
		std::string m_Language{};
	};
	using CRegistrationContextPtr = std::shared_ptr<DbAsync::CContext<CRegistrationPayload>>;

	static void CleanupAccountOnError(const CRegistrationContextPtr& pContext, const char* pStage)
	{
		const auto& Data = pContext->Data();
		dbg_msg("Account", "Registration of '%s' failed at '%s' (client %d, account id %d).",
			Data.m_Nickname.c_str(), pStage, pContext->GetClientID(), Data.m_InitID);

		if(Data.m_InitID > 0)
			AccountDb::DeleteAccount(Data.m_InitID);
		else
			AccountDb::DeleteAccountByCredentials(Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt);

		if(!pContext->GetPlayer())
			return;
		pContext->GS()->Chat(pContext->GetClientID(), "Registration failed due to server error.");
		pContext->GS()->Chat(pContext->GetClientID(), "Please try again a bit later.");
	}

	static void OnLookup(const CRegistrationContextPtr& pContext, Db::CResultSet&& Result)
	{
		auto* pGS = pContext->GS();
		const auto& Data = pContext->Data();
		const int ClientID = pContext->GetClientID();
		const auto pPlayer = pContext->GetPlayer();

		if(!pPlayer)
			return;

		// A failed lookup is not "the nickname is free": creating the account
		// would then fail on the unique index instead of the check above.
		if(!Result.Ok())
		{
			CleanupAccountOnError(pContext, "lookup nickname");
			return;
		}

		const AccountDb::SNickname Nickname = AccountDb::ParseNickname(Result);
		if(Nickname.m_Found)
		{
			const auto AccountID = Nickname.m_AccountID;
			const auto CurrentLogin = Nickname.m_Username;
			const auto TimeoutCode = Nickname.m_TimeoutCode;
			if(!CAccountManager::IsGuestCredentialForNickname(CurrentLogin, Data.m_Nickname.c_str()))
			{
				pGS->Chat(ClientID, "This nickname is already linked to another account.");
				pGS->Chat(ClientID, "Change nickname or contact support for recovery.");
				pGS->Chat(ClientID, "Discord: \"{~}\".", g_Config.m_SvDiscordInviteLink);
				return;
			}

			if(!TimeoutCode.empty() && (!pPlayer->IsAuthed() || pPlayer->Account()->GetID() != AccountID || !pPlayer->Account()->IsReadyToPlay()))
			{
				pGS->Chat(ClientID, "This nickname is protected by timeout code.");
				pGS->Chat(ClientID, "First complete authorization of the previous account.");
				return;
			}

			AccountDb::UpdateCredentials(AccountID, Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt)
				.Then([pContext](Db::CResultSet&& Result)
				{
					if(!pContext->GetPlayer())
						return;
					if(!Result.Changed())
					{
						dbg_msg("Account", "Registration of '%s' could not update the existing account (client %d).",
							pContext->Data().m_Nickname.c_str(), pContext->GetClientID());
						pContext->GS()->Chat(pContext->GetClientID(), "Registration update failed.");
						pContext->GS()->Chat(pContext->GetClientID(), "Please try again later.");
						return;
					}

					pContext->GS()->Chat(pContext->GetClientID(), "Registration completed successfully.");
					pContext->GS()->Chat(pContext->GetClientID(), "Remember your login and password.");
					pContext->GS()->Chat(pContext->GetClientID(), "Your nickname is your unique game identifier.");
				});
			return;
		}

		// The LEFT JOIN above also finds orphaned data rows, which reserve the nickname.
		CreateAccount(pContext);
	}

	static void CreateAccount(const CRegistrationContextPtr& pContext)
	{
		const auto& Data = pContext->Data();
		AccountDb::SRegistration Registration;
		Registration.m_Login = Data.m_Login;
		Registration.m_PasswordHash = Data.m_PasswordHash;
		Registration.m_PasswordSalt = Data.m_PasswordSalt;
		Registration.m_Nickname = Data.m_Nickname;
		Registration.m_RegisteredIP = Data.m_RegisteredIP;
		Registration.m_Language = Data.m_Language;

		// The account and its data row are created in one transaction: a failure
		// leaves nothing behind that would have to be cleaned up afterwards.
		AccountDb::Register(Registration).Then([pContext](Db::CResultSet&& Result)
		{
			auto& Data = pContext->Data();
			const int64_t AccountID = Result.Ok() ? Result.InsertId() : 0;
			// kept even when the player is gone: the cleanup removes exactly this row
			Data.m_InitID = static_cast<int>(std::max<int64_t>(0, AccountID));

			if(AccountID <= 0)
			{
				CleanupAccountOnError(pContext, "create account");
				return;
			}
			if(!pContext->GetPlayer())
			{
				// The login cannot continue, so the account must not stay either.
				CleanupAccountOnError(pContext, "player gone after create account");
				return;
			}
			AuthorizeAfterSuccess(pContext);
		});
	}

	static void AuthorizeAfterSuccess(const CRegistrationContextPtr& pContext)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;
		pPlayer->GetSharedData().m_pAuthorizationAttempt.reset();
		const auto& Data = pContext->Data();
		dbg_msg("Account", "Registration of '%s' stored the account (client %d, account id %d).", Data.m_Nickname.c_str(), pContext->GetClientID(), Data.m_InitID);
		pContext->Server()->UpdateAccountBase(Data.m_InitID, Data.m_Nickname, g_Config.m_SvMinRating);
		DbAuthorization::StartAuthorization(Data.m_pAccountManager, pContext->GetClientID(), Data.m_Login, Data.m_Password, Data.m_Nickname);
	}

public:
	static void StartRegistration(CAccountManager* pMgr, int ClientID, std::string Login, std::string Pass, std::string Nick, std::string RegisterIP)
	{
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
		if(!pPlayer || !pPlayer->GetSharedData().m_pAuthorizationAttempt.expired())
			return;
		char aSalt[32] = { 0 };
		secure_random_password(aSalt, sizeof(aSalt), 24);
		const auto AccountPasswordHash = CAccountManager::HashPassword(Pass, aSalt);
		const std::string Language = pPlayer->GetLanguage();
		auto pContext = DbAsync::MakeContext<DbRegistration::CRegistrationPayload>(ClientID,
			DbRegistration::CRegistrationPayload { pMgr, 0, Login, Pass, AccountPasswordHash, aSalt, Nick, RegisterIP, Language });
		StartAuthProgress(pPlayer->GetSharedData(), pContext, true);

		AccountDb::LookupNickname(Nick).Then([pContext](Db::CResultSet&& Result)
		{
			DbRegistration::OnLookup(pContext, std::move(Result));
		});
	}
};


void CAccountManager::OnPlayerLogin(CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->Account())
		return;

	// select first random profession by system
	auto* pAccount = pPlayer->Account();
	if(pAccount->GetActiveProfessionID() == ProfessionIdentifier::None)
	{
		ChanceProcessor<ProfessionIdentifier> process;
		process.addElement(ProfessionIdentifier::Tank, 100.f);
		process.addElement(ProfessionIdentifier::Healer, 100.f);
		process.addElement(ProfessionIdentifier::Dps, 100.f);
		process.setEqualChance(100.f);
		process.normalizeChances();
		pAccount->ChangeProfession(process.getRandomElement());
		GS()->Chat(pPlayer->GetCID(), "Your profession has been chosen by the system. You can change it at any time in the voting menu.");
	}
}

void CAccountManager::OnClientReset(int ClientID)
{
	if(const auto It = CAccountData::ms_aData.find(ClientID); It != CAccountData::ms_aData.end())
		It->second.CancelLoad();

	// A login that never completed gives its reservation back.
	for(const auto& Pending : m_vPendingLogins)
	{
		if(Pending.m_ClientID == ClientID)
			AccountSession::Release(Pending.m_Claim);
	}
	std::erase_if(m_vPendingLogins, [ClientID](const SPendingLogin& Pending) { return Pending.m_ClientID == ClientID; });

	// The same for a login that already started to load its account.
	for(const auto& Loading : m_vLoadingLogins)
	{
		if(Loading.m_ClientID == ClientID)
			AccountSession::Release(Loading.m_Claim);
	}
	std::erase_if(m_vLoadingLogins, [ClientID](const SLoadingLogin& Loading) { return Loading.m_ClientID == ClientID; });

	int AccountID = 0;
	std::string SessionToken;
	bool Ready = false;
	if(const auto It = CAccountData::ms_aData.find(ClientID); It != CAccountData::ms_aData.end())
	{
		AccountID = It->second.GetID();
		SessionToken = It->second.GetSessionToken();
		Ready = It->second.IsReadyToPlay();
	}

	// Only the row that still carries our token is removed; a newer owner's row stays.
	if(AccountID > 0 && !SessionToken.empty() && Ready)
		AccountSession::Remove({ AccountID, SessionToken });

	CAccountSharedData::ms_aPlayerSharedData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
}

void CAccountManager::BeginSessionTakeover(CPlayer* pPlayer, const AccountSession::SClaim& Claim, bool RecommendPin)
{
	if(!pPlayer || Claim.m_Token.empty() || pPlayer->Account()->GetID() != Claim.m_AccountID)
	{
		// The reservation was written for a login that cannot continue: left
		// behind, it would refuse the next attempt of this account until the
		// heartbeat counts as stale.
		AccountSession::Release(Claim);
		return;
	}

	// Sessions of this account on this server end now. The login completes only
	// after their slots are released (OnClientReset).
	KickOtherLocalSessions(Claim.m_AccountID, Claim.m_Token);

	SPendingLogin Pending {};
	Pending.m_ClientID = pPlayer->GetCID();
	Pending.m_Claim = Claim;
	Pending.m_RecommendPin = RecommendPin;
	Pending.m_StartTime = time_get();
	if(NeedsRemoteTakeover(Claim))
	{
		// The other server ends its session and answers with an ack carrying this event ID.
		Pending.m_EventID = Sync->Push(syncevents::ACCOUNT_SESSION_TAKEOVER, GS()->GetWorldID(), Claim.m_AccountID);
		Pending.m_RelayFailed = Pending.m_EventID <= 0;
	}
	m_vPendingLogins.push_back(Pending);

	GS()->Chat(Pending.m_ClientID, "Verifying account session...");
}

std::vector<CAccountManager::SLocalSession> CAccountManager::CollectLocalSessions(int AccountID) const
{
	std::vector<SLocalSession> vSessions;
	for(const auto& [ClientID, Data] : CAccountData::ms_aData)
	{
		if(Data.GetID() <= 0 || Data.GetSessionToken().empty())
			continue;
		if(AccountID > 0 && Data.GetID() != AccountID)
			continue;

		vSessions.push_back({ ClientID, { Data.GetID(), Data.GetSessionToken() } });
	}
	return vSessions;
}

bool CAccountManager::HasOtherLocalSession(int AccountID, const std::string& Token) const
{
	for(const auto& [ClientID, Data] : CAccountData::ms_aData)
	{
		if(Data.GetID() == AccountID && !Data.GetSessionToken().empty() && Data.GetSessionToken() != Token)
			return true;
	}
	return false;
}

void CAccountManager::KickLocalSession(const SLocalSession& Session)
{
	// Re-check: an earlier kick may already have changed this slot.
	const auto It = CAccountData::ms_aData.find(Session.m_ClientID);
	if(It == CAccountData::ms_aData.end() || It->second.GetID() != Session.m_Key.m_AccountID || It->second.GetSessionToken() != Session.m_Key.m_Token)
		return;

	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(Session.m_ClientID));
	if(!pGS || !pGS->GetPlayer(Session.m_ClientID, false))
		return;

	// Stop the old session from acting during its final disconnect tick.
	It->second.SetReadyToPlay(false);
	pGS->Server()->Kick(Session.m_ClientID, ACCOUNT_SESSION_TAKEOVER_REASON);
}

void CAccountManager::KickOtherLocalSessions(int AccountID, const std::string& Token)
{
	for(const auto& Session : CollectLocalSessions(AccountID))
	{
		if(Session.m_Key.m_Token != Token)
			KickLocalSession(Session);
	}
}

void CAccountManager::VerifySessions(std::vector<SLocalSession> vSessions, std::function<void()> OnVerified)
{
	std::vector<int> vAccountIDs;
	for(const auto& Session : vSessions)
		vAccountIDs.push_back(Session.m_Key.m_AccountID);

	AccountSession::FetchTokens(vAccountIDs, [this, vSessions = std::move(vSessions), OnVerified](bool Success, const std::vector<AccountSession::SKey>& vRows)
	{
		if(!Success)
		{
			dbg_msg("Account", "could not verify local sessions against the database");
			return;
		}

		// The row is the truth: a session whose token is no longer stored there was taken over.
		for(const auto& Session : vSessions)
		{
			const auto It = std::find_if(vRows.begin(), vRows.end(),
				[&Session](const AccountSession::SKey& Row) { return Row.m_AccountID == Session.m_Key.m_AccountID; });
			if(It != vRows.end() && !It->m_Token.empty() && It->m_Token != Session.m_Key.m_Token)
				KickLocalSession(Session);
		}

		if(OnVerified)
			OnVerified();
	});
}

void CAccountManager::FailPendingLogin(const SPendingLogin& Pending)
{
	AccountSession::Release(Pending.m_Claim);

	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(Pending.m_ClientID));
	if(!pGS)
		return;

	if(Pending.m_RelayFailed)
	{
		pGS->Chat(Pending.m_ClientID, "Login failed: session takeover relay is unavailable.");
		pGS->Server()->Kick(Pending.m_ClientID, "Session takeover relay unavailable");
		return;
	}

	pGS->Chat(Pending.m_ClientID, "Login failed: session takeover was not confirmed.");
	pGS->Server()->Kick(Pending.m_ClientID, "Session takeover was not confirmed");
}

bool CAccountManager::CompleteLogin(int ClientID, const AccountSession::SClaim& Claim, bool RecommendPin)
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
	auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
	if(!pGS || !pPlayer || !pPlayer->IsAuthed())
		return false;

	const auto It = CAccountData::ms_aData.find(ClientID);
	if(It == CAccountData::ms_aData.end() || It->second.GetID() != Claim.m_AccountID || It->second.GetSessionToken() != Claim.m_Token)
		return false;

	if(time_get() >= Claim.m_Deadline)
		return false;

	if(pPlayer->m_pMotdMenu)
		pPlayer->CloseMotdMenu();
	pGS->m_pController->DoTeamChange(pPlayer);

	// The account load answers on the game thread: only then is the account
	// playable and only then can the session be activated. The reservation stays
	// until the row is active, so no other login can take over in the meantime.
	auto pContext = DbAsync::MakeContext(ClientID);
	auto* pManager = this;

	// The load is watched until it answers: the login fails with a message when
	// the deadline passes instead of waiting for an answer that is not coming.
	m_vLoadingLogins.push_back({ ClientID, Claim, time_get() });

	const bool Started = pGS->Core()->AccountManager()->LoadAccount(pPlayer, true, Claim.m_Deadline,
		[pContext, pManager, Claim, RecommendPin](bool LoadSuccess)
		{
			const int64_t LoadMs = pManager->AckLoadingLogin(pContext->GetClientID());
			auto* pPlayer = pContext->GetPlayer();
			if(!LoadSuccess)
			{
				AccountSession::Release(Claim);
				if(pPlayer)
				{
					pContext->GS()->Chat(pContext->GetClientID(), "Login failed: account data could not be loaded.");
					pContext->Server()->Kick(pContext->GetClientID(), "Account data load failed");
				}
				return;
			}
			if(!pPlayer || pPlayer->Account()->GetSessionToken() != Claim.m_Token || !pPlayer->Account()->IsReadyToPlay() || time_get() >= Claim.m_Deadline)
			{
				pManager->AbortSessionClaim(pContext->GetClientID(), Claim);
				return;
			}

			// One line per login: it is where a slow login is read from, next to
			// the slow statement report of the database.
			int64_t AuthMs = 0;
			if(const auto ItShared = CAccountSharedData::ms_aPlayerSharedData.find(pContext->GetClientID()); ItShared != CAccountSharedData::ms_aPlayerSharedData.end() && ItShared->second.m_AuthStartedAt > 0)
				AuthMs = (time_get() - ItShared->second.m_AuthStartedAt) * 1000 / time_freq();
			dbg_msg("Account", "login of client %d is complete: account load %lld ms, the whole login %lld ms",
				pContext->GetClientID(), (long long)LoadMs, (long long)AuthMs);

			// The account is complete and the session is ours: this - not the
			// moment the password matched - is the login, so this is when the
			// player is told about it.
			pContext->GS()->Chat(pContext->GetClientID(), "Login successful. Welcome back!");
			if(NeedsRemoteTakeover(Claim))
				pContext->GS()->Chat(pContext->GetClientID(), "Warning: The previous account session on another server was terminated.");
			if(RecommendPin)
			{
				pContext->GS()->Chat(pContext->GetClientID(), "Security recommendation: set a PIN code.");
				pContext->GS()->Chat(pContext->GetClientID(), "Command: /set_pin <current_password> <new_pin>.");
			}

			AccountSession::Activate(Claim, [pContext, Token = Claim.m_Token](bool Success)
			{
				auto* pCurrentPlayer = pContext->GetPlayer();
				if(!Success && pCurrentPlayer && pCurrentPlayer->Account()->GetSessionToken() == Token)
					pContext->Server()->Kick(pContext->GetClientID(), "Account session activation failed");
			});
			g_EventListenerManager.Notify<IEventListener::PlayerLogin>(pPlayer, pPlayer->Account());
		});

	// No load was started, so no answer will ever arrive: end the login here
	// instead of leaving the player with an account that never becomes playable.
	if(!Started)
	{
		AckLoadingLogin(ClientID);
		AccountSession::Release(Claim);
		pGS->Chat(ClientID, "Login failed: the account could not be loaded.");
		pGS->Chat(ClientID, "Please reconnect and try again.");
		pGS->Server()->Kick(ClientID, "Account load did not start");
	}
	return true;
}

void CAccountManager::AbortSessionClaim(int ClientID, const AccountSession::SClaim& Claim)
{
	// The load did not finish in time or the session is no longer ours: give the
	// reservation back and end the connection that was waiting for it.
	AckLoadingLogin(ClientID);
	AccountSession::Release(Claim);

	const auto It = CAccountData::ms_aData.find(ClientID);
	if(It == CAccountData::ms_aData.end() || It->second.GetSessionToken() != Claim.m_Token)
		return;

	// The login is over without a playable account; say so instead of dropping
	// the player back into the world as if nothing had happened.
	if(auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID)))
		pGS->Chat(ClientID, "Login failed: the account did not finish loading.");
	Server()->Kick(ClientID, "Account session reservation expired");
}

CAccountManager::EPendingState CAccountManager::EvaluatePendingLogin(const SPendingLogin& Pending, int64_t Now) const
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(Pending.m_ClientID));
	auto* pPlayer = pGS ? pGS->GetPlayer(Pending.m_ClientID, false) : nullptr;
	if(!pPlayer || !pPlayer->IsAuthed())
		return EPendingState::Abandoned;

	if(Now >= Pending.m_Claim.m_Deadline)
		return EPendingState::Failed;

	if(NeedsRemoteTakeover(Pending.m_Claim) && !Pending.m_RemoteAck)
	{
		if(!Pending.m_RelayFailed && Now < Pending.m_StartTime + ACCOUNT_SESSION_TAKEOVER_TIMEOUT_SECONDS * time_freq())
			return EPendingState::Wait;

		// No answer from the previous server: only a stale heartbeat proves its session is gone.
		const int ElapsedSeconds = static_cast<int>((Now - Pending.m_StartTime) / time_freq());
		const int Age = Pending.m_Claim.m_Previous.m_HeartbeatAge;
		const bool Stale = Age >= 0 && Age + ElapsedSeconds >= AccountSession::HEARTBEAT_STALE_SECONDS;
		if(!Stale)
			return EPendingState::Failed;
	}

	// Wait until the old session on this server has disconnected (OnClientReset).
	if(HasOtherLocalSession(Pending.m_Claim.m_AccountID, Pending.m_Claim.m_Token))
		return EPendingState::Wait;

	return EPendingState::Ready;
}

int64_t CAccountManager::AckLoadingLogin(int ClientID)
{
	const int64_t Now = time_get();
	for(auto It = m_vLoadingLogins.begin(); It != m_vLoadingLogins.end(); ++It)
	{
		if(It->m_ClientID != ClientID)
			continue;

		const int64_t Ms = (Now - It->m_StartedAt) * 1000 / time_freq();
		m_vLoadingLogins.erase(It);
		return Ms;
	}
	return 0;
}

// A login ends in three stages: the pending one waits for the previous owner of
// the account, this one waits for the account itself. Both are watched, because
// a stage that never answers would keep the reservation and leave the player in
// "Login is still in progress" until the round ends.
void CAccountManager::SweepLoadingLogins()
{
	const int64_t Now = time_get();
	for(auto It = m_vLoadingLogins.begin(); It != m_vLoadingLogins.end();)
	{
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(It->m_ClientID));
		auto* pPlayer = pGS ? pGS->GetPlayer(It->m_ClientID, false) : nullptr;
		if(!pPlayer)
		{
			// nobody is left to be told: only the reservation has to go back
			AccountSession::Release(It->m_Claim);
			It = m_vLoadingLogins.erase(It);
			continue;
		}

		if(Now < It->m_Claim.m_Deadline)
		{
			++It;
			continue;
		}

		const int ClientID = It->m_ClientID;
		const AccountSession::SClaim Claim = It->m_Claim;
		const int64_t ElapsedS = (Now - It->m_StartedAt) / time_freq();
		It = m_vLoadingLogins.erase(It);

		dbg_msg("Account", "the account of client %d did not finish loading in %lld seconds: the login is given up",
			ClientID, (long long)ElapsedS);
		AbortSessionClaim(ClientID, Claim);
	}
}

void CAccountManager::ProcessPendingLogins()
{
	const int64_t Now = time_get();
	std::vector<SPendingLogin> vReady;
	std::vector<SPendingLogin> vFailed;
	for(auto It = m_vPendingLogins.begin(); It != m_vPendingLogins.end();)
	{
		switch(EvaluatePendingLogin(*It, Now))
		{
		case EPendingState::Wait:
			++It;
			continue;
		case EPendingState::Ready:
			vReady.push_back(*It);
			break;
		case EPendingState::Failed:
			vFailed.push_back(*It);
			break;
		case EPendingState::Abandoned:
			// The player is gone without OnClientReset: give the reservation back.
			AccountSession::Release(It->m_Claim);
			break;
		}
		It = m_vPendingLogins.erase(It);
	}

	// Act only after the list is final: these calls may disconnect players.
	for(const auto& Pending : vFailed)
		FailPendingLogin(Pending);

	for(const auto& Pending : vReady)
	{
		if(CompleteLogin(Pending.m_ClientID, Pending.m_Claim, Pending.m_RecommendPin))
			continue;

		AccountSession::Release(Pending.m_Claim);
		const auto It = CAccountData::ms_aData.find(Pending.m_ClientID);
		if(It != CAccountData::ms_aData.end() && It->second.GetSessionToken() == Pending.m_Claim.m_Token)
			Server()->Kick(Pending.m_ClientID, "Account session reservation expired");
	}
}

void CAccountManager::OnSyncEvent(std::string EventName, const CServerSync::SRequest& Req)
{
	const std::string AckEvent = std::string(syncevents::ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX) + std::to_string(g_Config.m_SvSyncServerID);
	if(EventName == AckEvent)
	{
		if(Req.Value <= 0)
			return;

		for(auto& Pending : m_vPendingLogins)
		{
			if(Pending.m_EventID == Req.Value && Pending.m_Claim.m_Previous.m_ServerID == Req.ServerID)
			{
				Pending.m_RemoteAck = true;
				break;
			}
		}
		return;
	}

	if(EventName != syncevents::ACCOUNT_SESSION_TAKEOVER || Req.Value <= 0)
		return;

	// Another server claimed this account: end the local sessions that lost the row,
	// then acknowledge. The ack is sent only if the check succeeded.
	const std::string AckTarget = std::string(syncevents::ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX) + std::to_string(Req.ServerID);
	VerifySessions(CollectLocalSessions(Req.Value), [AckTarget, WorldID = Req.WorldID, EventID = Req.ID]()
	{
		Sync->Push(AckTarget.c_str(), WorldID, EventID);
	});
}

void CAccountManager::OnTick()
{
	ProcessPendingLogins();
	SweepLoadingLogins();

	if(GS()->GetWorldID() != INITIALIZER_WORLD_ID)
		return;

	TickAuthProgress(); // server wide data, driven by this world only

	const int RefreshSeconds = g_Config.m_SvAccountSessionRefresh;
	if(RefreshSeconds <= 0)
		return;

	const int64_t Now = time_get();
	if(Now < m_NextSessionCheck)
		return;
	m_NextSessionCheck = Now + RefreshSeconds * time_freq();

	const auto vSessions = CollectLocalSessions();
	std::vector<AccountSession::SKey> vKeys;
	vKeys.reserve(vSessions.size());
	for(const auto& Session : vSessions)
		vKeys.push_back(Session.m_Key);

	AccountSession::RefreshHeartbeats(vKeys);
	VerifySessions(vSessions);
}

void CAccountManager::AddMenuProfessionUpgrades(CPlayer* pPlayer, CProfession* pProf) const
{
	if(!pProf || !pPlayer)
		return;

	const char* pProfName = GetProfessionName(pProf->GetProfessionID());
	const auto ClientID = pPlayer->GetCID();
	const auto ProfID = pProf->GetProfessionID();

	VoteWrapper VUpgrades(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "{} upgrades", pProfName);
	{
		for(const auto& ID : pProf->GetAttributes() | std::views::keys)
		{
			const auto* pAttribute = GS()->GetAttributeInfo(ID);
			const int AttributeSize = pPlayer->GetTotalAttributeValue(ID);
			const auto PercentOpt = pPlayer->GetTotalAttributeChance(ID);

			char aBuf[64] {};
			if(PercentOpt)
			{
				str_format(aBuf, sizeof(aBuf), "(%0.2f%%)", *PercentOpt);
			}
			VUpgrades.Add("Total {} - {}{~}", pAttribute->GetName(), AttributeSize, aBuf);
		}
	}
	VUpgrades.AddLine();
	{
		for(auto& [ID, Value] : pProf->GetAttributes())
		{
			const auto* pAttribute = GS()->GetAttributeInfo(ID);
			VUpgrades.AddOption("UPGRADE", MakeAnyList(ProfID, ID), "Upgrade {} - {} ({} point)", pAttribute->GetName(), Value, pAttribute->GetUpgradePrice());
		}
	}
}

bool CAccountManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// account information
	if(Menulist == MENU_ACCOUNT_DETAIL_INFO)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		auto* pAccount = pPlayer->Account();
		const char* pLastLoginDate = pAccount->GetLastLoginDate();

		// Account information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Account Information");
		VInfo.Add("Last login: {}", pLastLoginDate);
		VInfo.Add("Account ID: {}", pAccount->GetID());
		VInfo.Add("Login: {}", pAccount->GetLogin());
		VInfo.Add("Class: {}", GetProfessionName(pPlayer->Account()->GetActiveProfessionID()));
		VInfo.Add("Crime score: {}", pAccount->GetCrime());
		VInfo.Add("Gold capacity: {}", pAccount->GetGoldCapacity());
		VInfo.Add("Has house: {}", pAccount->HasHouse() ? "yes" : "no");
		VInfo.Add("Has guild: {}", pAccount->HasGuild() ? "yes" : "no");
		VInfo.Add("In group: {}", pAccount->HasGroup() ? "yes" : "no");
		VoteWrapper::AddEmptyline(ClientID);

		// Ranking information
		VoteWrapper VRating(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Rating information");
		VRating.Add("Rating: {}({~})", pAccount->GetRatingSystem().GetRating(), pAccount->GetRatingSystem().GetRankName());
		VRating.Add("Progress to {~}: {}/{}", pAccount->GetRatingSystem().GetNextRankName(),
			pAccount->GetRatingSystem().GetRankPointsProgress(), pAccount->GetRatingSystem().GetRankPointsRequired());
		VRating.Add("Played: {}", pAccount->GetRatingSystem().GetPlayed());
		VRating.Add("Win: {} / Losses: {}", pAccount->GetRatingSystem().GetWins(), pAccount->GetRatingSystem().GetLosses());
		VRating.Add("Win rate: {~.2}%", pAccount->GetRatingSystem().GetWinRate());
		VoteWrapper::AddEmptyline(ClientID);

		auto addLevelingInfo = [&](VoteWrapper& Wrapper, const CProfession* pProfession, const std::string& name)
		{
			const auto expNeed = pProfession->GetExpForNextLevel();
			const auto progress = translate_to_percent(expNeed, pProfession->GetExperience());
			const auto progressBar = mystd::string::progressBar(100, progress, 5, "\u25B0", "\u25B1");
			Wrapper.MarkList().Add("{} [Lv{} {}] - {~.2}%", name, pProfession->GetLevel(), progressBar, progress);
		};

		// Leveling information (war)
		VoteWrapper VLevelingWar(ClientID, VWF_ALIGN_TITLE, "Leveling (War professions)");
		for(auto& Profession : pPlayer->Account()->GetProfessions())
		{
			if(Profession.IsProfessionType(PROFESSION_TYPE_WAR))
			{
				const auto AppendActiveStatus = pPlayer->Account()->GetActiveProfessionID() == Profession.GetProfessionID() ? "(A)" : "";
				const auto pProfessionName = std::string(GetProfessionName(Profession.GetProfessionID()));
				const auto Title = AppendActiveStatus + pProfessionName;
				addLevelingInfo(VLevelingWar , &Profession, Title);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// Leveling information (work)
		VoteWrapper VLevelingOther(ClientID, VWF_ALIGN_TITLE, "Leveling (Other professions)");
		for(auto& Profession : pPlayer->Account()->GetProfessions())
		{
			if(Profession.IsProfessionType(PROFESSION_TYPE_OTHER))
			{
				const auto pProfessionName = std::string(GetProfessionName(Profession.GetProfessionID()));
				addLevelingInfo(VLevelingOther, &Profession, pProfessionName);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// Currency information
		const auto currencyItemIDs = CItemDescription::CollectIDs(ItemGroup::Currency, std::nullopt);
		VoteWrapper VCurrency(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Account Currency");
		VCurrency.Add("Bank: {}", pAccount->GetBankManager());
		for(int itemID : currencyItemIDs)
			VCurrency.Add("{}: {}", pPlayer->GetItem(itemID)->Info()->GetName(), pPlayer->GetItem(itemID)->GetValue());

		// Add backpage
		VoteWrapper::AddBackpage(ClientID);
	}

	// menu upgrades
	if(Menulist == MENU_UPGRADES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		const auto ActiveProfID = pPlayer->Account()->GetActiveProfessionID();

		// select war profession
		VoteWrapper VClassSelector(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Change class profession");
		VClassSelector.AddMenu(MENU_EQUIPMENT_PRESETS, MENU_UPGRADES, "Manage equipment presets");
		VClassSelector.AddMenu(MENU_UPGRADES_ATTRIBUTES_DETAIL, "Player Attributes: Details");
		if(const auto* pActiveProf = pPlayer->Account()->GetActiveProfession())
		{
			for(const auto& [ID, Percent] : pActiveProf->GetAmplifiers())
			{
				const auto pAttributeName = GS()->GetAttributeInfo(ID)->GetName();
				VClassSelector.Add("Class amplifier +{~.2}% to {}", Percent, pAttributeName);
			}
		}
		VClassSelector.AddLine();
		for(const auto& Prof : pPlayer->Account()->GetProfessions())
		{
			if(Prof.IsProfessionType(PROFESSION_TYPE_WAR))
			{
				const char* StrActiveFlag = (ActiveProfID == Prof.GetProfessionID()) ? "\u2713" : "\u2715";
				const char* pProfName = GetProfessionName(Prof.GetProfessionID());
				const auto expNeed = Prof.GetExpForNextLevel();
				const int progress = round_to_int(translate_to_percent(expNeed, Prof.GetExperience()));
				const auto progressBar = mystd::string::progressBar(100, progress, 5, "\u25B0", "\u25B1");

				VClassSelector.AddOption("SELECT_CLASS", MakeAnyList(Prof.GetProfessionID()), "({}) {} [Lv{} {} {~.1}%] ({}P)",
					StrActiveFlag, pProfName, Prof.GetLevel(), progressBar, progress, Prof.GetUpgradePoint());
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// professions
		VoteWrapper VProfessions(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT, "\u2696 Upgrade professions");
		for(const auto& Prof : pPlayer->Account()->GetProfessions())
		{
			const auto ProfessionID = Prof.GetProfessionID();
			VProfessions.AddMenu(MENU_UPGRADES, (int)ProfessionID, "Profession {} ({}P)", GetProfessionName(ProfessionID), Prof.GetUpgradePoint());
		}

		// list upgrades by profession
		if(const auto ProfID = pPlayer->m_VotesData.GetExtraID())
		{
			// check valid profession
			auto* pProf = pPlayer->Account()->GetProfession((ProfessionIdentifier)ProfID.value());
			if(!pProf)
			{
				VoteWrapper::AddBackpage(ClientID);
				return true;
			}

			// add profession upgrades
			VoteWrapper::AddEmptyline(ClientID);
			AddMenuProfessionUpgrades(pPlayer, pProf);
		}

		// Add back page
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_SKILLS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		const auto ActiveProfID = pPlayer->Account()->GetActiveProfessionID();
		const auto spAmount = pPlayer->GetItem(itSkillPoint)->GetValue();
		const auto GetSkillStateIcon = [spAmount](const CSkillDescription* pSkillDesc, const CSkill* pSkill) -> const char*
		{
			if(pSkill->IsLearned())
				return "\u2714";
			if(spAmount >= pSkillDesc->GetPriceSP())
				return "\u2726";
			return "\u2022";
		};
		const auto AddSkillSection = [&](VoteWrapper& VSection, ProfessionIdentifier ProfID)
		{
			int Total = 0;
			int Learned = 0;
			int AvailableToLearn = 0;
			for(const auto& [ID, pSkillDesc] : CSkillDescription::Data())
			{
				if(pSkillDesc->GetProfessionID() != ProfID)
					continue;

				const auto* pSkill = pPlayer->GetSkill(ID);
				Total++;
				if(pSkill->IsLearned())
					Learned++;
				else if(spAmount >= pSkillDesc->GetPriceSP())
					AvailableToLearn++;
			}

			VSection.Add("Learned: {}/{}  |  Ready to learn: {}", Learned, Total, AvailableToLearn);
			VSection.AddLine();

			for(const auto& [ID, pSkillDesc] : CSkillDescription::Data())
			{
				if(pSkillDesc->GetProfessionID() != ProfID)
					continue;

				const auto* pSkill = pPlayer->GetSkill(ID);
				const char* pIcon = GetSkillStateIcon(pSkillDesc, pSkill);
				VSection.AddMenu(MENU_SKILL_SELECT, ID, "{} {} - {}SP | {~}",
					pIcon, pSkillDesc->GetName(), pSkillDesc->GetPriceSP(), pSkill->GetStringLevelStatus());
			}
		};

		VoteWrapper VSkillsInfo(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2654 Skills");
		VSkillsInfo.Add("Active class: {}", GetProfessionName(ActiveProfID));
		VSkillsInfo.Add("Available SP: {}", spAmount);
		VoteWrapper::AddEmptyline(ClientID);

		// class skills
		VoteWrapper VActiveSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Class skills");
		AddSkillSection(VActiveSkills, ActiveProfID);
		VoteWrapper::AddEmptyline(ClientID);

		// universal skills
		VoteWrapper VUniversalSkills(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2694 Universal skills");
		AddSkillSection(VUniversalSkills, ProfessionIdentifier::None);
		VoteWrapper::AddEmptyline(ClientID);

		// Add back page
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_UPGRADES_ATTRIBUTES_DETAIL)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_UPGRADES);

		const std::map<AttributeGroup, std::string> vMapNames =
		{
			{ AttributeGroup::Tank, "Tank-related" },
			{ AttributeGroup::Dps, "Dps-related" },
			{ AttributeGroup::Healer, "Healer-related" },
			{ AttributeGroup::Weapon, "Weapons-related" },
			{ AttributeGroup::DamageType, "Damage-related" },
			{ AttributeGroup::Job, "Job-related" },
			{ AttributeGroup::Other, "Other" }
		};


		VoteWrapper VSelector(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "Select an Attribute Group");
		for(auto& [group, title] : vMapNames)
		{
			const auto groupOpt = pPlayer->m_VotesData.GetExtraID();
			const char* pSelector = GetSelectorStringByCondition(groupOpt && (AttributeGroup)(*groupOpt) == group);
			VSelector.AddMenu(MENU_UPGRADES_ATTRIBUTES_DETAIL, (int)group, "{}{SELECTOR}", title, pSelector);
		}
		VoteWrapper::AddEmptyline(ClientID);

		if(const auto groupOpt = pPlayer->m_VotesData.GetExtraID())
		{
			const auto group = (AttributeGroup)(*groupOpt);
			ShowPlayerAttributesByGroup(pPlayer, vMapNames.at(group), group);
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
	}

	// settings
	if(Menulist == MENU_SETTINGS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Settings Information");
		VInfo.Add("Here you can change the settings of your account.");
		VoteWrapper::AddEmptyline(ClientID);

		// game settings
		VoteWrapper VMain(ClientID, VWF_OPEN, "\u2699 Main settings");
		VMain.AddMenu(MENU_SETTINGS_LANGUAGE, "Settings language");
		VMain.AddMenu(MENU_SETTINGS_ACCOUNT, "Settings account");

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// settings account
	if(Menulist == MENU_SETTINGS_ACCOUNT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SETTINGS);

		// information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Account settings Information");
		VInfo.Add("Some of the settings become valid after death or reconnect.");
		VoteWrapper::AddEmptyline(ClientID);

		// account settings
		VoteWrapper VAccount(ClientID, VWF_OPEN, "\u2699 Account settings");
		const auto& PlayerItems = CPlayerItem::Data()[ClientID];
		for(const auto& [ItemID, ItemData] : PlayerItems)
		{
			if(ItemData.Info()->IsGroup(ItemGroup::Settings) && ItemData.HasItem())
			{
				const char* Status = ItemData.GetSettings() ? "Enabled" : "Disabled";
				VAccount.AddOption("TOGGLE_SETTING", ItemID, "[{}] {}", Status, ItemData.Info()->GetName());
			}
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// language selection
	if(Menulist == MENU_SETTINGS_LANGUAGE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_SETTINGS);

		// language information
		const char* pPlayerLanguage = pPlayer->GetLanguage();
		VoteWrapper VLanguageInfo(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "Languages Information");
		VLanguageInfo.Add("Here you can choose the language.");
		VLanguageInfo.Add("Active language: [{~}]", pPlayerLanguage);
		VLanguageInfo.AddLine();
		VLanguageInfo.Add("Playing with the selected language");
		VLanguageInfo.Add("will automatically collect strings");
		VLanguageInfo.Add("that are missing translation.");
		VLanguageInfo.AddLine();
		VLanguageInfo.Add("These strings will later");
		VLanguageInfo.Add("be available on the website.");
		VLanguageInfo.Add("{~}", g_Config.m_SvWebTranslateLink);
		VLanguageInfo.AddLine();
		VLanguageInfo.Add("Any user will be able");
		VLanguageInfo.Add("to translate them there.");
		VoteWrapper::AddEmptyline(ClientID);

		// languages
		VoteWrapper VLanguages(ClientID, VWF_OPEN|VWF_STYLE_STRICT, "Available languages");
		for(int i = 0; i < Server()->Localization()->m_pLanguages.size(); i++)
		{
			// do not show the language that is already selected by the player in the selection lists
			if(str_comp(pPlayerLanguage, Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
				continue;

			// add language selection
			auto* pLanguage = Server()->Localization()->m_pLanguages[i];
			const char* pLanguageName = pLanguage->GetName();
			const bool NonDefault = str_comp(pLanguage->GetFilename(), "en") != 0;
			VLanguages.AddOption("SELECT_LANGUAGE", i, "Select language \"{~}\"", pLanguageName);
			if(NonDefault)
				VLanguages.Add("- Number of lines collected: {} / Completed: {}%", pLanguage->GetCollectedLines(), pLanguage->GetCompletionPercent());
			VLanguages.AddLine();
		}

		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

void CAccountManager::ShowPlayerAttributesByGroup(CPlayer* pPlayer, std::string_view Title, AttributeGroup Group) const
{
	const auto ClientID = pPlayer->GetCID();
	VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, Title.data());
	for(auto& [ID, pInfo] : CAttributeDescription::Data())
	{
		if(pInfo->IsGroup(Group))
		{
			if(const auto PercentOpt = pPlayer->GetTotalAttributeChance(ID))
			{
				VGroup.Add("{}: {~.2}%", pInfo->GetName(), (*PercentOpt));
				continue;
			}

			const int AttributeSize = pPlayer->GetTotalAttributeValue(ID);
			VGroup.Add("{}: {}", pInfo->GetName(), AttributeSize);
		}
	}
}

bool CAccountManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// select language command
	if(PPSTR(pCmd, "SELECT_LANGUAGE") == 0)
	{
        const int LanguageID = GetIfExists<int>(Extras, 0, NOPE);
		const char* pSelectedLanguage = Server()->Localization()->m_pLanguages[LanguageID]->GetFilename();
		Server()->SetClientLanguage(ClientID, pSelectedLanguage);
		GS()->Chat(ClientID, "You have chosen a language \"{}\".", pSelectedLanguage);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_SETTINGS_LANGUAGE);
		Core()->SaveAccount(pPlayer, SAVE_LANGUAGE);
		return true;
	}

	// upgrade command
	if(PPSTR(pCmd, "UPGRADE") == 0)
	{
        const auto ProfessionID = GetIfExists<ProfessionIdentifier>(Extras, 0, ProfessionIdentifier::None);
        const auto AttributeID = GetIfExists<AttributeIdentifier>(Extras, 1, AttributeIdentifier::Unknown);

		// check valid profession
		auto* pProfession = pPlayer->Account()->GetProfession(ProfessionID);
		if(!pProfession)
			return false;

		// check valid attribute
		const auto* pAttributeInfo = GS()->GetAttributeInfo(AttributeID);
		if(!pAttributeInfo)
			return false;

		// upgrade
		if(pProfession->Upgrade(AttributeID, ReasonNumber, pAttributeInfo->GetUpgradePrice()))
		{
			const auto nowValue = pProfession->GetAttributeValue(AttributeID);
			const auto pProfessionName = GetProfessionName(ProfessionID);

			g_EventListenerManager.Notify<IEventListener::PlayerProfessionUpgrade>(pPlayer, (int)AttributeID);
			GS()->Chat(ClientID, "[{}] Attribute '{}' enhanced to '{}p'!", pProfessionName, pAttributeInfo->GetName(), nowValue);
			GS()->CreatePlayerSound(ClientID, SOUND_SFX_UPGRADE);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// select class
	if(PPSTR(pCmd, "SELECT_CLASS") == 0)
	{
        const auto ProfessionID = GetIfExists<ProfessionIdentifier>(Extras, 0, ProfessionIdentifier::None);

		// can't change profession in dungeon
		if(GS()->IsWorldType(WorldType::Dungeon))
		{
			GS()->Chat(ClientID, "You can't change profession in dungeons");
			return true;
		}

		// check spam
		if((pPlayer->m_aPlayerTick[LastDamage] + Server()->TickSpeed() * 5) > Server()->Tick())
		{
			GS()->Chat(ClientID, "Wait a couple of seconds, your player is currently in combat or taking damage");
			return true;
		}

		// change class
		pPlayer->Account()->ChangeProfession(ProfessionID);
		pPlayer->GetCharacter()->UpdateEquippedStats();
		pPlayer->m_VotesData.ResetExtraID();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		Core()->SaveAccount(pPlayer, SAVE_PROFESSION);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_CHANGE_PROFESSION);
		return true;
	}

	return false;
}

void CAccountManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	int ClientID = pPlayer->GetCID();

	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_BANK_MANAGER, MOTD_MENU_BANK_MANAGER)
	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_CASINO_ACTION, MOTD_MENU_CASINO)

	// selector language by action tile
	const auto TrySelectLanguageInActionZone = [&](const char* pActionZone, const char* pLanguageFile, const char* pLanguageName)
	{
		// select language
		auto SelectLanguage = [&]()
		{
			Server()->SetClientLanguage(ClientID, pLanguageFile);
			GS()->Chat(ClientID, "You have chosen a language \"{}\".", pLanguageName);
			Core()->SaveAccount(pPlayer, SAVE_LANGUAGE);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_SETTINGS_LANGUAGE);
		};

		const bool InPlayerZone = pChr->GetTiles()->IsActiveActionZone(pActionZone, EActionZonePosition::PLAYER);
		const bool InMouseZone = pChr->GetTiles()->IsActiveActionZone(pActionZone, EActionZonePosition::MOUSE);
		const bool LanguageChanged = str_comp(pPlayer->GetLanguage(), pLanguageFile) != 0;
		if(LanguageChanged && (InPlayerZone || (InMouseZone && Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE))))
			SelectLanguage();
		if(InMouseZone)
		{
			GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, Server()->TickSpeed() / 2,
				"Language: {}\nPress fire (mouse) to select.", pLanguageName);
		}

		// leavce
		if(pChr->GetTiles()->IsLeaveActionZone(pActionZone, EActionZonePosition::MOUSE))
			GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, 1, "");
	};
	TrySelectLanguageInActionZone("LANG_EN", "en", "English");
	TrySelectLanguageInActionZone("LANG_RU", "ru", "Russian");
	TrySelectLanguageInActionZone("LANG_CN", "cn", "Chinese");
}

bool CAccountManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	int ClientID = pPlayer->GetCID();

	// motd menu bank
	if(Menulist == MOTD_MENU_BANK_MANAGER)
	{
		// initialize variables
		const int CurrentGold = pPlayer->Account()->GetGold();
		const BigInt CurrentBankGold = pPlayer->Account()->GetBankManager();
		const BigInt TotalGold = pPlayer->Account()->GetTotalGold();

		MotdMenu MBonuses(ClientID,  "Here you can securely store your gold. Remember, gold in the Bank is protected and will never be lost, even in death.");
		MBonuses.AddText("Bank Management \u2697");
		MBonuses.AddSeparateLine();
		MBonuses.AddText("Gold: {$}", CurrentGold);
		MBonuses.AddText("Bank: {$}", CurrentBankGold);
		MBonuses.AddText("Total: {$}", TotalGold);
		MBonuses.AddText("Commision rate: {}%", g_Config.m_SvBankCommissionRate);
		MBonuses.AddSeparateLine();
		MBonuses.AddOption("BANK_DEPOSIT", "Deposit All").Pack(CurrentGold);
		MBonuses.Send(MOTD_MENU_BANK_MANAGER);
		return true;
	}

	// motd menu about bonuses
	if(Menulist == MOTD_MENU_BONUSES_INFO)
	{
		int Position = 1;
		const auto& rBonusManager = pPlayer->Account()->GetBonusManager();
		MotdMenu MBonuses(ClientID, "World rates, random events, bonus items, and potions add together; a positive bonus adds at least 1 point.");
		MBonuses.AddText("Active bonuses \u2696");
		MBonuses.AddSeparateLine();
		for(const auto& Bonus : rBonusManager.GetTemporaryBonuses())
		{
			int Days, Hours, Minutes, Seconds;
			Bonus.GetRemainingTimeFormatted(&Days, &Hours, &Minutes, &Seconds);

			MBonuses.AddText("{}. {} ({}) - {~.2}%", Position,
				CMultiplierManager::GetTypeName(Bonus.Type), CMultiplierManager::GetSourceName(Bonus.Source), Bonus.Amount);
			MBonuses.AddText("Time left: {}d {}h {}m {}s.", Days, Hours, Minutes, Seconds);
			MBonuses.AddSeparateLine();
			++Position;
		}

		if(Position == 1)
			MBonuses.AddText("No active personal bonuses.");

		const auto& vWorldMultipliers = GS()->m_Multipliers.GetContributions();
		if(!vWorldMultipliers.empty())
		{
			MBonuses.AddSeparateLine();
			MBonuses.AddText("World and random-event sources:");
			for(const auto& Multiplier : vWorldMultipliers)
				MBonuses.AddText("- {}: {} +{~.2}%", Multiplier.m_Name,
					CMultiplierManager::GetTypeName(Multiplier.m_Type), Multiplier.m_Percent);
		}

		bool HasCombinedRates = false;
		for(const auto Type : rBonusManager.GetActiveMultiplierTypes())
		{
			const float BonusPercent = rBonusManager.GetTotalBonusPercentage(Type);
			if(BonusPercent <= 0.0f)
				continue;

			if(!HasCombinedRates)
			{
				MBonuses.AddSeparateLine();
				MBonuses.AddText("Effective combined rates:");
				HasCombinedRates = true;
			}
			const auto Rate = 100.f + BonusPercent;
			MBonuses.AddText("- {}: {~.2}%", CMultiplierManager::GetTypeName(Type), Rate);
		}

		MBonuses.AddBackpage();
		MBonuses.AddSeparateLine();
		MBonuses.Send(MOTD_MENU_BONUSES_INFO);
		return true;
	}

	// motd menu about wanted
	if(Menulist == MOTD_MENU_WANTED_INFO)
	{
		bool hasWanted = false;
		MotdMenu MWanted(ClientID, "A list of wanted players for whom bounties have been assigned. To get the bounty you need to kill a player from the list.");
		MWanted.AddText("Wanted players \u2694");
		MWanted.AddSeparateLine();
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPl = GS()->GetPlayer(i, true);
			if(!pPl || !pPl->Account()->IsCrimeMaxedOut())
				continue;

			CPlayerItem* pItemGold = pPl->GetItem(itGold);
			const int Reward = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldOnDeath), pItemGold->GetValue());
			MWanted.AddText("{~}", Server()->ClientName(i));
			MWanted.AddText("Reward: {$} gold", Reward);
			MWanted.AddText("Last seen in : {}", Server()->GetWorldName(pPl->GetCurrentWorldID()));
			MWanted.AddSeparateLine();
			hasWanted = true;
		}

		if(!hasWanted)
		{
			MWanted.AddText("No wanted players!");
			MWanted.AddSeparateLine();
		}

		MWanted.AddBackpage();
		MWanted.AddSeparateLine();
		MWanted.Send(MOTD_MENU_WANTED_INFO);
		return true;
	}

	// motd menu personal assistant
	if(Menulist == MOTD_MENU_PERSONAL_ASSISTANT)
	{
		// The assistant counts the letters, so the menu is built when the answer
		// is in instead of waiting for it here.
		auto pAccount = pPlayer->Account();
		auto pContext = DbAsync::MakeContext(ClientID);
		Core()->MailboxManager()->CountMails(pAccount->GetID()).Then([pContext](Db::CResultSet&& Result)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			auto* pAccount = pPlayer->Account();
			const auto gold = pAccount->GetGold();
			const auto goldCapacity = pAccount->GetGoldCapacity();
			const auto bankGold = pAccount->GetBankManager();
			const int mailCount = Result.HasRows() ? Result.GetInt("Letters") : 0;

			// personal assistant
			MotdMenu MAssistant(pContext->GetClientID(), MTFLAG_CLOSE_BUTTON, "Quick tips update as you progress.");
			MAssistant.AddText("Profession: {}", GetProfessionName(pAccount->GetActiveProfessionID()));
			MAssistant.AddText("Level {}.", pAccount->GetLevel());
			MAssistant.AddText("Gold: {$}/{$}", gold, goldCapacity);
			MAssistant.AddText("Bank: {$}", bankGold);
			MAssistant.AddText("World: {}", pContext->Server()->GetWorldName(pPlayer->GetCurrentWorldID()));
			MAssistant.AddSeparateLine();

			// recommendations
			MAssistant.AddText("Recommendations:");
			bool hasRecommendation = false;
			const auto crimeScore = pAccount->GetCrime();
			const auto totalProfUP = pAccount->GetTotalProfessionsUpgradePoints();

			if(mailCount > 0)
			{
				MAssistant.AddText("- You have {} mail waiting for you.", mailCount);
				hasRecommendation = true;
			}
			if(goldCapacity > 0 && gold >= (goldCapacity * 8) / 10)
			{
				MAssistant.AddText("- Consider depositing gold to avoid loss.");
				hasRecommendation = true;
			}
			if(crimeScore > 0)
			{
				MAssistant.AddText("- Your crime score attracts bounty hunters.");
				hasRecommendation = true;
			}
			if(totalProfUP > 0)
			{
				MAssistant.AddText("- Spend {} upgrade points to strengthen your profession.", totalProfUP);
				hasRecommendation = true;
			}
			if(!hasRecommendation)
				MAssistant.AddText("- Everything looks good. Keep it up!");
			MAssistant.AddSeparateLine();

			// quick actions
			MAssistant.AddText("Quick actions:");
			MAssistant.AddMenu(MOTD_MENU_BONUSES_INFO, NOPE, "\u2696 Active bonuses");
			MAssistant.AddMenu(MOTD_MENU_WANTED_INFO, NOPE, "\u2694 Wanted list");
			MAssistant.AddMenu(MOTD_MENU_WIKI_INFO, NOPE, "\u2605 Wiki guide");
			MAssistant.AddSeparateLine();
			MAssistant.Send(MOTD_MENU_PERSONAL_ASSISTANT);
		});
		return true;
	}

	// motd menu casino dice
	if (Menulist == MOTD_MENU_CASINO)
	{
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (!pController)
			return true;

		CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
		if (!pGame)
			return false;

		CDiceDuelGame* pDiceGame = dynamic_cast<CDiceDuelGame*>(pGame);
		CRouletteGame* pRouletteGame = dynamic_cast<CRouletteGame*>(pGame);
		if (!pDiceGame && !pRouletteGame)
			return true;

		const auto* pCurrency = pPlayer->GetItem(pPlayer->m_CurrencyCasinoItemID);
		const int TimeLeft = (pGame->GetTicksLeft() / Server()->TickSpeed());
		const bool bIsJoined = pGame->IsPlayerJoined(pPlayer->GetCID());
		const char* pMenuDesc = pDiceGame
			? "Roll the dice and test your luck!\nThe more you bet, the higher your chances of winning."
			: "Spin the wheel of fortune!\nThe more you bet, the higher your chances of winning.";
		const char* pMenuTitle = pDiceGame ? "\u2696 Casino: Dice Duel" : "\u2696 Casino: Roulette";
		MotdMenu MCasino(ClientID, MTFLAG_ALWAYS_UPDATE, pMenuDesc);
		MCasino.AddText(pMenuTitle);
		MCasino.AddSeparateLine();

		MCasino.AddText("Enter the amount bet:");
		MCasino.AddMenu(MOTD_MENU_CASINO_CURRENCY_SELECT, NOPE, "Currency: {} ({$})", pCurrency->Info()->GetName(), 
			(pCurrency->GetID() == itGold) ? pPlayer->Account()->GetTotalGold() : pCurrency->GetValue());
		MCasino.AddField(BET_FIELD_AMOUNT, MTTEXTINPUTFLAG_ONLY_NUMERIC);
		MCasino.AddSeparateLine();

		if (pDiceGame)
		{
			if (TimeLeft > 0 && pDiceGame->GetState() == EDiceGameState::LOBBY)
				MCasino.AddText("Starting in: {} sec.", TimeLeft);
			else if (pDiceGame->GetState() == EDiceGameState::ROLLING)
				MCasino.AddText("Rolling the dice...");
			else if (pDiceGame->GetState() == EDiceGameState::SHOW_RESULT)
				MCasino.AddText("Waiting to start round...");
			else
				MCasino.AddText("Waiting for players...");
		}
		else if (pRouletteGame)
		{
			if (TimeLeft > 0 && pRouletteGame->GetState() == ERouletteState::LOBBY)
				MCasino.AddText("Starting in: {} sec.", TimeLeft);
			else if (pRouletteGame->GetState() == ERouletteState::SPINNING)
				MCasino.AddText("Spinning the wheel...");
			else if (pRouletteGame->GetState() == ERouletteState::SHOW_RESULT)
				MCasino.AddText("Waiting to start round...");
			else
				MCasino.AddText("Waiting for players...");
		}

		if (bIsJoined)
		{
			MCasino.AddOption("CASINO_BET_LEAVE", "Leave the game");
		}
		else
		{
			if (pDiceGame)
			{
				MCasino.AddOption("DICE_BET_ENTER", "Bet on High (8-12)").Pack(EDiceBetType::HIGH, 7);
				MCasino.AddOption("DICE_BET_ENTER", "Bet on Equal (7)").Pack(EDiceBetType::EQUAL, 7);
				MCasino.AddOption("DICE_BET_ENTER", "Bet on Low (2-6)").Pack(EDiceBetType::LOW, 7);
			}
			else if (pRouletteGame)
			{
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on range 1-12").Pack(ERouletteBetKind::RANGE, (int)ERouletteRange::LOW);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on range 13-24").Pack(ERouletteBetKind::RANGE, (int)ERouletteRange::MID);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on range 25-36").Pack(ERouletteBetKind::RANGE, (int)ERouletteRange::HIGH);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on Red").Pack(ERouletteBetKind::COLOR, (int)ERouletteColor::RED);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on Green").Pack(ERouletteBetKind::COLOR, (int)ERouletteColor::GREEN);
				MCasino.AddOption("ROULETTE_BET_ENTER", "Bet on Black").Pack(ERouletteBetKind::COLOR, (int)ERouletteColor::BLACK);
			}
		}

		MCasino.Send(MOTD_MENU_CASINO);
		return true;
	}

	if (Menulist == MOTD_MENU_CASINO_CURRENCY_SELECT)
	{
		MotdMenu MCasino(ClientID, MTFLAG_ALWAYS_UPDATE);
		for(auto& [ItemID, ItemData] : CPlayerItem::Data()[ClientID])
		{
			if(ItemData.Info()->IsGroup(ItemGroup::Currency))
			{
				MCasino.AddOption("CASINO_CURRENCY_SELECT", "{} ({$})", ItemData.Info()->GetName(), 
					(ItemID == itGold ? pPlayer->Account()->GetTotalGold() : ItemData.GetValue())).Pack(ItemID);
			}
		}
		MCasino.AddBackpage();
		MCasino.Send(MOTD_MENU_CASINO_CURRENCY_SELECT);
		return true;
	}

	return false;
}

bool CAccountManager::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd)
{
	// casino bet currency select
	if (strcmp(pCmd, "CASINO_CURRENCY_SELECT") == 0)
	{
		const auto& [ItemID] = pMotdData->GetCurrent()->Unpack<int>();
		pPlayer->m_CurrencyCasinoItemID = ItemID;
		pPlayer->CloseMotdMenu();
		GS()->SendMenuMotd(pPlayer, MOTD_MENU_CASINO);
		return true;
	}

	// casino bet leave
	if (strcmp(pCmd, "CASINO_BET_LEAVE") == 0)
	{
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (!pController)
			return true;

		CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
		if (pGame && pGame->Leave(pPlayer->GetCID()))
			GS()->Chat(pPlayer->GetCID(), "You have left the dice game.");
		return true;
	}

	// roulette bet currency select
	if (strcmp(pCmd, "ROULETTE_BET_ENTER") == 0)
	{
		const auto& [Kind, Value] = pMotdData->GetCurrent()->Unpack<ERouletteBetKind, int>();
		const auto Bet = pMotdData->GetFieldStr(BET_FIELD_AMOUNT);
		if (!Bet.has_value())
		{
			GS()->Chat(pPlayer->GetCID(), "Please enter a bet amount.");
			return true;
		}

		const auto BetAmount = clamp(str_toint(Bet.value().c_str()), 0, 1000000000);
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (pController)
		{
			CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
			CRouletteGame* pTable = dynamic_cast<CRouletteGame*>(pGame);
			if (!pTable || !pTable->Join(pPlayer->GetCID(), BetAmount, Kind, Value))
				GS()->Chat(pPlayer->GetCID(), "Failed to join the roulette game. Please check your gold and try again.");
		}
		return true;
	}

	// dice bet high
	if (strcmp(pCmd, "DICE_BET_ENTER") == 0)
	{
		const auto Bet = pMotdData->GetFieldStr(BET_FIELD_AMOUNT);
		if(!Bet.has_value())
		{
			GS()->Chat(pPlayer->GetCID(), "Please enter a bet amount.");
			return true;
		}

		const auto& [BetType, Threshold] = pMotdData->GetCurrent()->Unpack<EDiceBetType, int>();
		const auto BetAmount = clamp(str_toint(Bet.value().c_str()), 0, 1000000000);
		auto* pController = dynamic_cast<CGameControllerDefault*>(GS()->m_pController);
		if (pController)
		{
			CCasinoGame* pGame = pController->GetCasinoGameInRange(pPlayer->m_ViewPos);
			CDiceDuelGame* pTable = dynamic_cast<CDiceDuelGame*>(pGame);
			if (!pTable || !pTable->Join(pPlayer->GetCID(), BetAmount, BetType, Threshold))
				GS()->Chat(pPlayer->GetCID(), "Failed to join the dice game. Please check your gold and try again.");
		}
		return true;
	}

	// deposit bank gold
	if(strcmp(pCmd, "BANK_DEPOSIT") == 0)
	{
		auto* pGold = pPlayer->GetItem(itGold);
		if(pGold->GetValue() <= 100)
		{
			GS()->Chat(pPlayer->GetCID(), "You need at least '100 gold' to deposit.");
			return true;
		}

		const auto& [Value] = pMotdData->GetCurrent()->Unpack<int>();
		if(pGold->GetValue() < Value)
		{
			GS()->Chat(pPlayer->GetCID(), "You can max deposit '{} gold'.", pGold->GetValue());
			return true;
		}

		const auto CommisionValue = translate_to_percent_rest(Value, g_Config.m_SvBankCommissionRate);
		const auto FinalValue = Value - CommisionValue;

		pGold->Remove(Value);
		pPlayer->Account()->AddGoldToBank(FinalValue);
		GS()->Chat(pPlayer->GetCID(), "You have successfully deposited '{} gold'. Commision '{} gold'.", FinalValue, CommisionValue);
		GS()->Chat(pPlayer->GetCID(), "Commision '{} gold'.", CommisionValue);
		return true;
	}

	return false;
}

std::string CAccountManager::BuildGuestCredential(const char* pNickname)
{
	return std::string("Guest-") + pNickname;
}

bool CAccountManager::IsGuestCredentialForNickname(std::string_view Login, const char* pNickname)
{
	return Login == BuildGuestCredential(pNickname);
}

std::string CAccountManager::HashPassword(const std::string& Password, const std::string& Salt)
{
	std::string plaintext = Salt + Password + Salt;
	SHA256_CTX sha256Ctx;
	sha256_init(&sha256Ctx);
	sha256_update(&sha256Ctx, plaintext.c_str(), plaintext.length());
	const SHA256_DIGEST digest = sha256_finish(&sha256Ctx);

	char hash[SHA256_MAXSTRSIZE];
	sha256_str(digest, hash, sizeof(hash));
	return std::string(hash);
}

AccountCodeResult CAccountManager::RegisterAccount(int ClientID, const char* pLogin, const char* pPassword)
{
	if(!CheckLoginPasswordLength(GS(), ClientID, pLogin, pPassword))
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;

	GS()->Chat(ClientID, "Registration in progress. Please wait..."); // the queries can take seconds
	return RegisterAccountRaw(ClientID, pLogin, pPassword);
}

AccountCodeResult CAccountManager::RegisterAccountRaw(int ClientID, const char* pLogin, const char* pPassword)
{
	char aAddrStr[64] {};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	DbRegistration::StartRegistration(this, ClientID, pLogin, pPassword, Server()->ClientName(ClientID), aAddrStr);
	return AccountCodeResult::AOP_REGISTER_OK;
}

AccountCodeResult CAccountManager::RegisterGuestAccount(int ClientID, const char* pNickname)
{
	const auto GuestCredential = CAccountManager::BuildGuestCredential(pNickname);
	RegisterAccountRaw(ClientID, GuestCredential.c_str(), GuestCredential.c_str());
	return AccountCodeResult::AOP_REGISTER_OK;
}

AccountCodeResult CAccountManager::LoginAccount(int ClientID, const char* pLogin, const char* pPassword)
{
	if(!CheckLoginPasswordLength(GS(), ClientID, pLogin, pPassword))
		return AccountCodeResult::AOP_MISMATCH_LENGTH_SYMBOLS;

	return LoginAccountRaw(ClientID, pLogin, pPassword);
}

AccountCodeResult CAccountManager::LoginAccountRaw(int ClientID, const char* pLogin, const char* pPassword)
{
	DbAuthorization::StartAuthorization(this, ClientID, pLogin, pPassword, Server()->ClientName(ClientID));
	return AccountCodeResult::AOP_LOGIN_OK;
}

bool CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitilize, int64_t ClaimDeadline, std::function<void(bool)> OnLoaded)
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// The client may already be in another world: the manager of that world owns
	// the player object the answers have to be applied to.
	if(!GS()->IsPlayerInWorld(pPlayer->GetCID()))
	{
		auto* pActiveGS = static_cast<CGS*>(Instance::GameServerPlayer(pPlayer->GetCID()));
		auto* pActivePlayer = pActiveGS ? pActiveGS->GetPlayer(pPlayer->GetCID(), false) : nullptr;
		if(!pActivePlayer)
			return false;
		return pActiveGS->Core()->AccountManager()->LoadAccount(pActivePlayer, FirstInitilize, ClaimDeadline, std::move(OnLoaded));
	}

	// Broadcast a message to the player with their current location
	auto* pAccount = pPlayer->Account();
	const int ClientID = pPlayer->GetCID();
	const auto RateExp = 100 + round_to_int(GS()->m_Multipliers.GetSourcePercent(MultiplierType::Experience, MultiplierSource::World));
	const auto RateGold = 100 + round_to_int(GS()->m_Multipliers.GetSourcePercent(MultiplierType::Gold, MultiplierSource::World));
	GS()->Broadcast(ClientID, BroadcastPriority::VeryImportant, 300, "You are currently positioned at {}({})!\n- World rates: Exp {}% | Gold {}%",
		Server()->GetWorldName(GS()->GetWorldID()), (GS()->IsAllowedPVP() ? "PVE/PVP" : "PVE"), RateExp, RateGold);

	// Check if it is not the first initialization
	if(!FirstInitilize)
	{
		Core()->MailboxManager()->NotifyUnreadMails(pPlayer);
		pAccount->GetBonusManager().SendInfoAboutActiveBonuses();
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		if(OnLoaded)
			OnLoaded(true);
		return true;
	}

	// Collect account-owned and component-owned data work before starting the
	// plan. Login hooks remain separate from stage registration; independent
	// reads still run concurrently, behind one readiness barrier.
	auto pContext = DbAsync::MakeContext(ClientID);
	auto* pManager = this;
	pAccount->RegisterLoadStages();
	Core()->RegisterAccountLoadStages(pPlayer);
	Core()->OnPlayerLogin(pPlayer);
	pAccount->RunLoadStages([pContext, pManager, ClaimDeadline, OnLoaded = std::move(OnLoaded)](bool Success) mutable
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!Success || !pPlayer)
		{
			if(OnLoaded)
				OnLoaded(false);
			return;
		}

		pManager->FinishAccountLoad(pPlayer, ClaimDeadline);
		if(OnLoaded)
			OnLoaded(true);
	});
	return true;
}

void CAccountManager::FinishAccountLoad(CPlayer* pPlayer, int64_t ClaimDeadline)
{
	auto* pAccount = pPlayer->Account();
	const int ClientID = pPlayer->GetCID();

	// The parts of the account are all in, so the expired ones can go now: the
	// removal clears the equipment slot that still names such an item, which is
	// only possible once the professions of the account are loaded.
	Core()->InventoryManager()->RemoveExpiredItems(pPlayer);

	// initialize default item's & settings
	auto InitSettingsItem = [pPlayer](const std::unordered_map<int, int>& pItems)
	{
		for(auto& [id, defaultValue] : pItems)
		{
			if(auto pItem = pPlayer->GetItem(id))
			{
				if(!pItem->HasItem())
					pItem->Add(1, defaultValue, 0, false);
			}
		}
	};

	InitSettingsItem(
		{
			{ itHammer, 1 },
			{ itShowEquipmentDescription, 0 },
			{ itShowCriticalDamage, 1 },
			{ itShowQuestStarNavigator, 1 },
			{ itShowDetailGainMessages, 0 },
			{ itShowOnlyFunctionModules, 1 },
			{ itShowItemSourceHint, 0 },
		});

	// update player time periods
	Core()->OnHandlePlayerTimePeriod(pPlayer);
	if(ClaimDeadline && time_get() >= ClaimDeadline)
	{
		Server()->Kick(ClientID, "Account session reservation expired");
		return;
	}
	pAccount->SetReadyToPlay(true);

	// the rating belongs to the loaded account, so the client is updated with it
	Server()->UpdateAccountBase(pAccount->GetID(), Server()->ClientName(ClientID), pAccount->GetRatingSystem().GetRating());

	// notify about rank
	const int Rank = Server()->GetAccountRank(pAccount->GetID());
	GS()->Chat(-1, "'{~}' logged to account. Rank '#{}[{~}]' ({~})", Server()->ClientName(ClientID), Rank,
		Server()->ClientCountryIsoCode(ClientID), pPlayer->Account()->GetRatingSystem().GetRankName());

	// Change player's world ID to the latest correct world ID
	const int LatestCorrectWorldID = GetLastVisitedWorldID(pPlayer);
	if(LatestCorrectWorldID != GS()->GetWorldID())
	{
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return;
	}
}

void CAccountManager::SetPinCode(int ClientID, const char* pCurrentPasswordOrPin, const char* pNewPin, bool IsChangingPin)
{
	// check valid player
	auto* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		GS()->Chat(ClientID, "You must be logged in to set or change a PIN code.");
		return;
	}

	// check valid pin length
	const int LengthNewPin = str_length(pNewPin);
	if(LengthNewPin < LENGTH_PIN_MIN || LengthNewPin > LENGTH_PIN_MAX)
	{
		GS()->Chat(ClientID, "PIN code must be {} to {} digits long.", LENGTH_PIN_MIN, LENGTH_PIN_MAX);
		return;
	}
	for(int i = 0; i < LengthNewPin; ++i)
	{
		if(!isdigit(pNewPin[i]))
		{
			GS()->Chat(ClientID, "PIN code must consist of digits only.");
			return;
		}
	}

	// One statement answers everything the checks below need: whether a PIN is
	// set (and which one) and whether the entered password is the account one.
	const int AccountID = pPlayer->Account()->GetID();
	const std::string NewPin = pNewPin;
	const std::string Current = pCurrentPasswordOrPin ? pCurrentPasswordOrPin : "";
	auto pContext = DbAsync::MakeContext(ClientID);
	AccountDb::ReadPinCodeAndPassword(AccountID)
		.Then([pContext, AccountID, IsChangingPin, NewPin, Current](Db::CResultSet&& Account)
	{
		if(!pContext->GetPlayer())
			return;

		auto* pGS = pContext->GS();
		const int ClientID = pContext->GetClientID();
		if(!Account.HasRows())
		{
			pGS->Chat(ClientID, "Error retrieving account data.");
			return;
		}

		const std::string PinInDb = Account.GetString("PinCode");
		const bool PinIsSet = !PinInDb.empty();

		if(IsChangingPin)
		{
			// is not set need set pin
			if(!PinIsSet)
			{
				pGS->Chat(ClientID, "You don't have a PIN code set.");
				pGS->Chat(ClientID, "Use '/set_pin' to set your PIN.");
				return;
			}

			// check valid pin
			if(PinInDb != Current)
			{
				pGS->Chat(ClientID, "The current PIN code you entered is incorrect.");
				return;
			}
			if(Current == NewPin)
			{
				pGS->Chat(ClientID, "The new PIN cannot be the same as the old PIN.");
				return;
			}
		}
		else
		{
			// is set need change pin
			if(PinIsSet)
			{
				pGS->Chat(ClientID, "You already have a PIN code set.");
				pGS->Chat(ClientID, "Use '/change_pin' to update your PIN.");
				return;
			}

			// check valid password
			if(Account.GetString("Password") != HashPassword(Current.c_str(), Account.GetString("PasswordSalt")))
			{
				pGS->Chat(ClientID, "The account password you entered is incorrect.");
				return;
			}
		}

		// save new pin-code
		AccountDb::SavePinCode(AccountID, NewPin);
		pGS->Chat(ClientID, "Your PIN code has been successfully {}!", Instance::Localize(ClientID, IsChangingPin ? "changed" : "set"));
	});
}

void CAccountManager::ChangePassword(int ClientID, const char* pOldPassword, const char* pNewPassword, const char* pPinCode)
{
	// check valid account
	auto* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer || !pPlayer->IsAuthed())
	{
		GS()->Chat(ClientID, "You must be logged in to change your password.");
		return;
	}

	// check valid length and password same
	const int LengthOldPass = str_length(pOldPassword);
	const int LengthNewPass = str_length(pNewPassword);
	if(LengthOldPass < LENGTH_LOGPASS_MIN || LengthOldPass > LENGTH_LOGPASS_MAX || LengthNewPass < LENGTH_LOGPASS_MIN || LengthNewPass > LENGTH_LOGPASS_MAX)
	{
		GS()->Chat(ClientID, "The old and new passwords must each contain '{} - {} characters'.", LENGTH_LOGPASS_MIN, LENGTH_LOGPASS_MAX);
		return;
	}
	if(str_comp(pOldPassword, pNewPassword) == 0)
	{
		GS()->Chat(ClientID, "The new password cannot be the same as the old password.");
		return;
	}

	// require pin code
	if(pPinCode == nullptr || *pPinCode == '\0')
	{
		GS()->Chat(ClientID, "PIN code is required to change your password.");
		return;
	}

	// check valid and length pin code
	const int LengthPin = str_length(pPinCode);
	if(LengthPin < LENGTH_PIN_MIN || LengthPin > LENGTH_PIN_MAX)
	{
		GS()->Chat(ClientID, "Invalid PIN code format.");
		return;
	}
	for(int i = 0; i < LengthPin; ++i)
	{
		if(!isdigit(pPinCode[i]))
		{
			GS()->Chat(ClientID, "PIN code must consist of digits only.");
			return;
		}
	}

	// One statement answers both questions: the PIN of the account and its
	// password hash. The checks and the hash of the new password follow in the
	// answer, on the game thread.
	const int AccountID = pPlayer->Account()->GetID();
	const std::string OldPassword = pOldPassword;
	const std::string NewPassword = pNewPassword;
	const std::string Pin = pPinCode;
	auto pContext = DbAsync::MakeContext(ClientID);
	AccountDb::ReadPinCodeAndPassword(AccountID)
		.Then([pContext, AccountID, OldPassword, NewPassword, Pin](Db::CResultSet&& Account)
	{
		if(!pContext->GetPlayer())
			return;

		auto* pGS = pContext->GS();
		const int ClientID = pContext->GetClientID();
		if(!Account.HasRows())
		{
			pGS->Chat(ClientID, "Error retrieving account data.");
			return;
		}

		// check is pincode set
		const std::string PinFromDb = Account.GetString("PinCode");
		if(PinFromDb.empty())
		{
			pGS->Chat(ClientID, "For security, you must first set up a PIN code.");
			pGS->Chat(ClientID, "Use '/set_pin' to set your PIN.");
			return;
		}

		// check valid pincode
		if(PinFromDb != Pin)
		{
			pGS->Chat(ClientID, "The PIN code you entered is incorrect.");
			return;
		}

		// check valid password
		if(Account.GetString("Password") != HashPassword(OldPassword.c_str(), Account.GetString("PasswordSalt")))
		{
			pGS->Chat(ClientID, "The old password you entered is incorrect.");
			return;
		}

		// update new password
		char aNewSalt[32] = { 0 };
		secure_random_password(aNewSalt, sizeof(aNewSalt), 24);
		const std::string HashedNewPassword = HashPassword(NewPassword.c_str(), aNewSalt);
		AccountDb::SavePassword(AccountID, HashedNewPassword, aNewSalt);

		// information
		pGS->Chat(ClientID, "Your password has been successfully changed!");
		pGS->Chat(ClientID, "Please remember your new login details.");
		pGS->Chat(ClientID, "New password: '{}'.", NewPassword.c_str());
	});
}

void CAccountManager::ChangeNickname(std::string NewNickname, int ClientID)
{
	// The name is only changed when the database says it is free; the player is
	// told in the answer, so nothing waits for the lookup.
	auto* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;
	const int AccountID = pPlayer->Account()->GetID();

	auto pContext = DbAsync::MakeContext(ClientID);
	AccountDb::FindNicknameOwner(NewNickname)
		.Then([pContext, AccountID, NewNickname](Db::CResultSet&& Existing)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		auto* pGS = pContext->GS();
		const int ClientID = pContext->GetClientID();
		if(Existing.HasRows())
		{
			pGS->Chat(ClientID, "This nickname is already in use.");
			return;
		}

		AccountDb::SaveNickname(AccountID, NewNickname);
		pGS->Server()->SetClientName(ClientID, NewNickname.c_str());
		pGS->Chat(ClientID, "Your nickname has been successfully updated.");
	});
}

void CAccountManager::UseVoucher(int ClientID, const char* pVoucher) const
{
	auto* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	// The voucher decides what the player gets, so it is read, checked and
	// redeemed in the answer; nothing waits for it here.
	const std::string VoucherCode = pVoucher;
	auto pContext = DbAsync::MakeContext(ClientID);
	AccountDb::FindVoucher(pPlayer->Account()->GetID(), VoucherCode)
		.Then([pContext, VoucherCode](Db::CResultSet&& Voucher)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		auto* pGS = pContext->GS();
		const int ClientID = pContext->GetClientID();
		if(!Voucher.HasRows())
		{
			pGS->Chat(ClientID, "The voucher code '{}' does not exist.", VoucherCode.c_str());
			return;
		}

		const int VoucherID = Voucher.GetInt("ID");
		const int ValidUntil = Voucher.GetInt("ValidUntil");

		// The voucher decides what the player gets, so a document that cannot be
		// read gives nothing: the parsing helper reports it and hands back an
		// empty object instead of throwing inside the answer.
		nlohmann::json JsonData = nlohmann::json::object();
		mystd::json::parse(Voucher.GetString("Data"), [&JsonData](nlohmann::json& Json) { JsonData = Json; });

		if(ValidUntil > 0 && ValidUntil < time(0))
		{
			pGS->Chat(ClientID, "The voucher code '{}' has expired.", VoucherCode.c_str());
			return;
		}

		if(Voucher.GetBoolean("used"))
		{
			pGS->Chat(ClientID, "This voucher has already been redeemed.");
			return;
		}

		const int Exp = JsonData.value("exp", 0);
		const int Money = JsonData.value("money", 0);

		if(Exp > 0)
			pPlayer->Account()->AddExperience(Exp);
		if(Money > 0)
			pPlayer->Account()->AddGold(Money);

		if(JsonData.find("items") != JsonData.end() && JsonData["items"].is_array())
		{
			for(const nlohmann::json& Item : JsonData["items"])
			{
				const int ItemID = Item.value("id", -1);
				const int Value = Item.value("value", 0);
				if(Value > 0 && CItemDescription::Data().contains(ItemID))
					pPlayer->GetItem(ItemID)->Add(Value);
			}
		}

		pGS->Core()->SaveAccount(pPlayer, SAVE_STATS);
		pGS->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);

		AccountDb::RedeemVoucher(VoucherID, pPlayer->Account()->GetID(), (int64_t)time(0));
		pGS->Chat(ClientID, "You have successfully redeemed the voucher '{}'.", VoucherCode.c_str());
	});
}

void CAccountManager::BanAccount(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason)
{
	if(!pPlayer || !pPlayer->Account())
		return;

	// The ban is written unless one is active already; the player is removed in
	// the answer, on the game thread.
	const int AccountID = pPlayer->Account()->GetID();
	const int ClientID = pPlayer->GetCID();
	const int64_t DurationSeconds = Time.duration().count();
	auto pContext = DbAsync::MakeContext(ClientID);
	AccountDb::ReadActiveBan(AccountID)
		.Then([pContext, AccountID, ClientID, DurationSeconds, Reason](Db::CResultSet&& ActiveBan)
	{
		auto* pGS = pContext->GS();
		if(!pGS)
			return;
		auto* pConsole = pGS->Console();
		if(!ActiveBan.Ok())
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Could not check the account's current ban");
			return;
		}
		if(ActiveBan.HasRows())
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "This account is already banned");
			return;
		}

		AccountDb::CreateBan(AccountID, DurationSeconds, Reason).Then([pContext, ClientID](Db::CResultSet&& Created)
		{
			auto* pGS = pContext->GS();
			if(!pGS)
				return;
			if(!Created.Ok() || !Created.Changed())
			{
				pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "The ban could not be saved");
				return;
			}

			pContext->Server()->Kick(ClientID, "Your account was banned");
			pGS->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully banned!");
		});
	});
}

void CAccountManager::UnBanAccount(int BanId) const
{
	// Search for the ban and lift it in the answer.
	AccountDb::ReadActiveBanByID(BanId)
		.Then([BanId](Db::CResultSet&& ActiveBan)
	{
		auto* pConsole = static_cast<CGS*>(Instance::GameServer(INITIALIZER_WORLD_ID))->Console();
		if(!ActiveBan.HasRows())
		{
			// the ban does not exist or has already run out
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Ban is not valid anymore or does not exist!");
			return;
		}

		AccountDb::LiftBan(BanId);
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully unbanned!");
	});
}

void CAccountManager::BansAccount(std::function<void(std::vector<AccBan>&&)> OnLoaded) const
{
	AccountDb::ListActiveBans()
		.Then([OnLoaded = std::move(OnLoaded)](Db::CResultSet&& Bans)
	{
		std::vector<AccBan> vBans;
		vBans.reserve(100);
		while(Bans.Next())
		{
			int ID = Bans.GetInt("Id");
			int AccountID = Bans.GetInt("AccountId");
			std::string BannedUntil = Bans.GetString("BannedUntil");
			std::string PlayerNickname = Instance::Server()->GetAccountNickname(AccountID);
			std::string Reason = Bans.GetString("Reason");
			vBans.emplace_back(ID, BannedUntil, std::move(PlayerNickname), std::move(Reason));
		}

		if(OnLoaded)
			OnLoaded(std::move(vBans));
	});
}

int CAccountManager::GetLastVisitedWorldID(CPlayer* pPlayer) const
{
	const auto pWorldIterator = std::ranges::find_if(pPlayer->Account()->m_aHistoryWorld, [&](int WorldID)
	{
		if(GS()->GetWorldData(WorldID))
		{
			int RequiredLevel = GS()->GetWorldData(WorldID)->GetRequiredLevel();
			return Server()->IsWorldType(WorldID, WorldType::Default) && pPlayer->Account()->GetLevel() >= RequiredLevel;
		}
		return false;
	});
	return pWorldIterator != pPlayer->Account()->m_aHistoryWorld.end() ? *pWorldIterator : g_Config.m_SvShowWorldWhenConnect;
}
