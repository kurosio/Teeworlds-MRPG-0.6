/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_auth.h"
#include "account_db.h"
#include "account_session.h"

#include <base/hash_ctxt.h>
#include <base/system.h>
#include <game/server/gamecontext.h>
#include <game/server/core/tools/db_async_context.h>

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

namespace
{
	constexpr int ACCOUNT_SESSION_CLAIM_ATTEMPTS = 3;

	CGS* GetGameServer(int ClientID)
	{
		return static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
	}

	void StartAuthorizationAttempt(CAccountSharedData& Shared, const std::shared_ptr<void>& pAttempt)
	{
		Shared.m_pAuthorizationAttempt = pAttempt;
		Shared.m_AuthStartedAt = time_get();
	}

	bool CheckLoginPasswordLength(int ClientID, const char* pLogin, const char* pPassword)
	{
		const int LengthLogin = str_length(pLogin);
		const int LengthPassword = str_length(pPassword);
		if(LengthLogin <= AccountAuth::LENGTH_LOGPASS_MAX && LengthLogin >= AccountAuth::LENGTH_LOGPASS_MIN
			&& LengthPassword <= AccountAuth::LENGTH_LOGPASS_MAX && LengthPassword >= AccountAuth::LENGTH_LOGPASS_MIN)
			return true;

		if(auto* pGS = GetGameServer(ClientID))
			pGS->Chat(ClientID, "The username and password must each contain '{} - {} characters'.", AccountAuth::LENGTH_LOGPASS_MIN, AccountAuth::LENGTH_LOGPASS_MAX);
		return false;
	}

	// -----------------------------------------------------------------------
	// login pipeline
	// -----------------------------------------------------------------------
	struct SLoginPayload
	{
		std::string m_Login {};
		std::string m_Password {};
		std::string m_Language {};
		std::string m_LoginDate {};
		std::string m_PinCode {};
		AccountSession::SClaim m_Claim {};
		int m_Attempts {};
		// The account row is read while the session of the account is taken, so
		// the login does not spend a round trip on it after the claim.
		Db::CRequest m_AccountRequest {};
	};
	using CLoginContextPtr = std::shared_ptr<DbAsync::CContext<SLoginPayload>>;

	void ReadOwnerAndClaim(const CLoginContextPtr& pContext);
	void OnReadOwner(const CLoginContextPtr& pContext, std::optional<AccountSession::SRecord> Owner);
	void OnClaimed(const CLoginContextPtr& pContext, bool Claimed);
	void OnReleased(const CLoginContextPtr& pContext);
	void LoadAccountData(const CLoginContextPtr& pContext);
	void OnLoadAccountData(const CLoginContextPtr& pContext, Db::CResultSet&& Result);

	CPlayer* GetLoginPlayer(const CLoginContextPtr& pContext)
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
	void OnCredentialsChecked(const CLoginContextPtr& pContext, Db::CResultSet&& Result)
	{
		auto* pPlayer = GetLoginPlayer(pContext);
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

		if(!Credentials.m_CredentialsFound ||
			Credentials.m_Password != AccountAuth::HashPassword(pContext->Data().m_Password, Credentials.m_PasswordSalt))
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: incorrect login or password.");
			return;
		}

		// All entry points, including /login, must enforce guest protection.
		if(AccountAuth::IsGuestCredentialForNickname(Credentials.m_Username, Credentials.m_Nickname.c_str()) &&
			!Credentials.m_TimeoutCode.empty() && Credentials.m_TimeoutCode != pPlayer->GetSharedData().m_TimeoutCode)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Invalid timeout code. Please try /timeout <code> again.");
			return;
		}

		if(Credentials.m_HasBan)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Account is temporarily blocked.");
			pContext->GS()->Chat(pContext->GetClientID(), "Until: '{}'. Reason: '{}'.", Credentials.m_BannedUntil, Credentials.m_BanReason);
			return;
		}

		auto& Data = pContext->Data();
		Data.m_Claim.m_AccountID = Credentials.m_AccountID;
		Data.m_Language = Credentials.m_Language;
		Data.m_LoginDate = Credentials.m_LoginDate;
		Data.m_PinCode = Credentials.m_PinCode;

		// The row the account is created from does not depend on the session, so
		// it is asked for now: its answer is ready by the time the claim is ours.
		Data.m_AccountRequest = AccountDb::LoadAccount(Credentials.m_AccountID);
		ReadOwnerAndClaim(pContext);
	}

	// Reads the owner and tries to take the account. Repeated if a claim is refused.
	void ReadOwnerAndClaim(const CLoginContextPtr& pContext)
	{
		AccountSession::ReadRecord(pContext->Data().m_Claim.m_AccountID,
			[pContext](std::optional<AccountSession::SRecord> Owner) { OnReadOwner(pContext, Owner); });
	}

	void OnReadOwner(const CLoginContextPtr& pContext, std::optional<AccountSession::SRecord> Owner)
	{
		auto* pPlayer = GetLoginPlayer(pContext);
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

	void OnClaimed(const CLoginContextPtr& pContext, bool Claimed)
	{
		const auto& Claim = pContext->Data().m_Claim;
		if(Claimed && GetLoginPlayer(pContext) && time_get() < Claim.m_Deadline)
		{
			LoadAccountData(pContext);
			return;
		}

		// Refused, lost or too late. Give back whatever the write did (a no-op unless
		// our token is stored), then decide whether to try again.
		AccountSession::Release(Claim, [pContext](bool) { OnReleased(pContext); });
	}

	void OnReleased(const CLoginContextPtr& pContext)
	{
		auto* pPlayer = GetLoginPlayer(pContext);
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

	void LoadAccountData(const CLoginContextPtr& pContext)
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

	void OnLoadAccountData(const CLoginContextPtr& pContext, Db::CResultSet&& Result)
	{
		auto* pPlayer = GetLoginPlayer(pContext);
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
		// Keep the authorization context alive through takeover and account
		// loading so the shared weak authorization token remains valid.
		AccountSession::BeginTakeover(pPlayer, Claim, Data.m_PinCode.empty() && !pPlayer->IsGuestLogin(), pContext);
	}

	void StartLogin(int ClientID, const std::string& Login, const std::string& Pass, const std::string& Nick)
	{
		auto* pGS = GetGameServer(ClientID);
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
		if(!pPlayer || !pPlayer->GetSharedData().m_pAuthorizationAttempt.expired() || pPlayer->Account()->GetID() > 0)
			return;

		SLoginPayload Payload {};
		Payload.m_Login = Login;
		Payload.m_Password = Pass;
		Payload.m_Claim.m_ClientID = ClientID;
		Payload.m_Claim.m_ServerID = g_Config.m_SvSyncServerID;
		Payload.m_Claim.m_Token = AccountSession::GenerateToken();
		Payload.m_Claim.m_Deadline = time_get() + AccountSession::CLAIM_SECONDS * time_freq();
		auto pContext = DbAsync::MakeContext<SLoginPayload>(ClientID, std::move(Payload));

		StartAuthorizationAttempt(pPlayer->GetSharedData(), pContext);

		// The nickname, the password hash check and the ban arrive in one row.
		AccountDb::Authenticate(Nick, Login).Then([pContext](Db::CResultSet&& Result)
		{
			OnCredentialsChecked(pContext, std::move(Result));
		});
	}

	// -----------------------------------------------------------------------
	// registration pipeline
	// -----------------------------------------------------------------------
	struct SRegisterPayload
	{
		int m_InitID {};
		std::string m_Login {};
		std::string m_Password {};
		std::string m_PasswordHash {};
		std::string m_PasswordSalt {};
		std::string m_Nickname {};
		std::string m_RegisteredIP {};
		std::string m_Language {};
	};
	using CRegisterContextPtr = std::shared_ptr<DbAsync::CContext<SRegisterPayload>>;

	void CleanupAccountOnError(const CRegisterContextPtr& pContext, const char* pStage)
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

	void AuthorizeAfterSuccess(const CRegisterContextPtr& pContext)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;
		pPlayer->GetSharedData().m_pAuthorizationAttempt.reset();
		const auto& Data = pContext->Data();
		dbg_msg("Account", "Registration of '%s' stored the account (client %d, account id %d).", Data.m_Nickname.c_str(), pContext->GetClientID(), Data.m_InitID);
		pContext->Server()->UpdateAccountBase(Data.m_InitID, Data.m_Nickname, g_Config.m_SvMinRating);
		StartLogin(pContext->GetClientID(), Data.m_Login, Data.m_Password, Data.m_Nickname);
	}

	void CreateAccount(const CRegisterContextPtr& pContext)
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

	void OnLookupNickname(const CRegisterContextPtr& pContext, Db::CResultSet&& Result)
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
			if(!AccountAuth::IsGuestCredentialForNickname(CurrentLogin, Data.m_Nickname.c_str()))
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

	void StartRegistration(int ClientID, const std::string& Login, const std::string& Pass, const std::string& Nick, const std::string& RegisterIP)
	{
		auto* pGS = GetGameServer(ClientID);
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
		if(!pPlayer || !pPlayer->GetSharedData().m_pAuthorizationAttempt.expired())
			return;

		char aSalt[32] = { 0 };
		secure_random_password(aSalt, sizeof(aSalt), 24);
		SRegisterPayload Payload {};
		Payload.m_Login = Login;
		Payload.m_Password = Pass;
		Payload.m_PasswordHash = AccountAuth::HashPassword(Pass, aSalt);
		Payload.m_PasswordSalt = aSalt;
		Payload.m_Nickname = Nick;
		Payload.m_RegisteredIP = RegisterIP;
		Payload.m_Language = pPlayer->GetLanguage();
		auto pContext = DbAsync::MakeContext<SRegisterPayload>(ClientID, std::move(Payload));
		StartAuthorizationAttempt(pPlayer->GetSharedData(), pContext);

		AccountDb::LookupNickname(Nick).Then([pContext](Db::CResultSet&& Result)
		{
			OnLookupNickname(pContext, std::move(Result));
		});
	}
}

namespace AccountAuth
{
	void Register(int ClientID, const char* pLogin, const char* pPassword)
	{
		if(!CheckLoginPasswordLength(ClientID, pLogin, pPassword))
			return;

		if (auto* pGS = GetGameServer(ClientID))
		{
			char aAddrStr[64]{};
			pGS->Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
			pGS->Chat(ClientID, "Registration in progress. Please wait...");
			StartRegistration(ClientID, pLogin, pPassword, pGS->Server()->ClientName(ClientID), aAddrStr);
		}
	}

	void Login(int ClientID, const char* pLogin, const char* pPassword)
	{
		if(!CheckLoginPasswordLength(ClientID, pLogin, pPassword))
			return;

		if (auto* pGS = GetGameServer(ClientID))
		{
			pGS->Chat(ClientID, "Login in progress. Please wait...");
			StartLogin(ClientID, pLogin, pPassword, pGS->Server()->ClientName(ClientID));
		}
	}

	void RegisterGuest(int ClientID, const char* pNickname)
	{
		if (auto* pGS = GetGameServer(ClientID))
		{
			const auto GuestCredential = BuildGuestCredential(pNickname);
			char aAddrStr[64]{};
			pGS->Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
			pGS->Chat(ClientID, "Registration in progress. Please wait...");
			StartRegistration(ClientID, GuestCredential, GuestCredential, pGS->Server()->ClientName(ClientID), aAddrStr);
		}
	}

	void LoginGuest(int ClientID)
	{
		if (auto* pGS = GetGameServer(ClientID))
		{
			auto* pPlayer = pGS->GetPlayer(ClientID, false);
			if(!pPlayer)
				return;
			
			const std::string GuestLogin = pPlayer->GetSharedData().m_GuestLogin;
			if(GuestLogin.empty())
				return;

			pGS->Chat(ClientID, "Login in progress. Please wait...");
			StartLogin(ClientID, GuestLogin, GuestLogin, pGS->Server()->ClientName(ClientID));
		}
	}

	std::string HashPassword(const std::string& Password, const std::string& Salt)
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

	std::string BuildGuestCredential(const char* pNickname)
	{
		return std::string("Guest-") + pNickname;
	}

	bool IsGuestCredentialForNickname(std::string_view Login, const char* pNickname)
	{
		return Login == BuildGuestCredential(pNickname);
	}

	bool IsValidPinLength(std::string_view Pin)
	{
		return (int)Pin.length() >= LENGTH_PIN_MIN && (int)Pin.length() <= LENGTH_PIN_MAX;
	}

	bool IsDigitsOnly(std::string_view Text)
	{
		for(const char Character : Text)
			if(!isdigit(Character))
				return false;
		return true;
	}
}
