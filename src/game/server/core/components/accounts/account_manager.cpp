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

#include <game/server/core/components/inventory/inventory_manager.h>
#include <game/server/core/components/mails/mailbox_manager.h>
#include <game/server/core/components/worlds/world_data.h>
#include <game/server/core/tools/db_async_context.h>

#include <teeother/components/localization.h>

#include <algorithm>
#include <mutex>

constexpr int LENGTH_LOGPASS_MAX = 12;
constexpr int LENGTH_LOGPASS_MIN = 4;
constexpr int LENGTH_PIN_MAX = 6;
constexpr int LENGTH_PIN_MIN = 4;
constexpr int BET_FIELD_AMOUNT = 0;
constexpr int AUTH_FIELD_PASSWORD = 1;

namespace
{
	constexpr const char* ACCOUNT_SESSIONS_TABLE = "tw_account_sessions";
	constexpr const char* ACCOUNT_SESSION_TAKEOVER_REASON = "Account session was taken over";
	constexpr int ACCOUNT_SESSION_TOKEN_LENGTH = 48;
	constexpr int ACCOUNT_SESSION_TAKEOVER_TIMEOUT_SECONDS = 10;
	constexpr int ACCOUNT_SESSION_HEARTBEAT_STALE_SECONDS = 45;

	std::string GenerateSessionToken()
	{
		char aToken[65] {};
		secure_random_password(aToken, sizeof(aToken), ACCOUNT_SESSION_TOKEN_LENGTH);
		return std::string(aToken);
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
		int m_AccountID{};
		std::string m_Language{};
		std::string m_LoginDate{};
		std::string m_PinCode{};
		std::string m_SessionToken{};
		int m_PreviousServerID{};
		int m_PreviousClientID{};
		std::string m_PreviousToken{};
		int m_PreviousHeartbeat{};
		int m_PreviousHeartbeatAge{ -1 };
	};
	using CAuthorizationContextPtr = std::shared_ptr<DbAsync::CContext<CAuthorizationPayload>>;

	static void OnCheckNickname(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!pRes->next())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: account with this nickname was not found.");
			return;
		}

		auto& Data = pContext->Data();
		Data.m_AccountID = pRes->getInt("ID");
		auto pCheck = Database->Prepare<DB::SELECT>("ID, LoginDate, Language, PinCode, Password, PasswordSalt",
			"tw_accounts", "WHERE Username = '{}' AND ID = '{}'", Data.m_Login, Data.m_AccountID);
		pCheck->AtExecute([pContext](ResultPtr pCheckRes) { OnCheckCredentials(pContext, pCheckRes); });
	}

	static void OnCheckCredentials(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!pRes->next() || (pRes->getString("Password") != CAccountManager::HashPassword(pContext->Data().m_Password, pRes->getString("PasswordSalt"))))
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: incorrect login or password.");
			return;
		}

		auto& Data = pContext->Data();
		Data.m_Language = pRes->getString("Language");
		Data.m_LoginDate = pRes->getString("LoginDate");
		Data.m_PinCode = pRes->getString("PinCode");
		auto pBan = Database->Prepare<DB::SELECT>("BannedUntil, Reason", "tw_accounts_bans", "WHERE AccountId = '{}' AND current_timestamp() < `BannedUntil`", Data.m_AccountID);
		pBan->AtExecute([pContext](ResultPtr pBanRes) { OnCheckBans(pContext, pBanRes); });
	}

	static void OnCheckBans(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(pRes->next())
		{
			const auto BannedUntil = pRes->getString("BannedUntil");
			const auto Reason = pRes->getString("Reason");
			pContext->GS()->Chat(pContext->GetClientID(), "Account is temporarily blocked.");
			pContext->GS()->Chat(pContext->GetClientID(), "Until: '{}'. Reason: '{}'.", BannedUntil, Reason);
			return;
		}

		auto& Data = pContext->Data();
		auto pPrevious = Database->Prepare<DB::SELECT>("ServerID, ClientID, SessionToken, Heartbeat, UNIX_TIMESTAMP() AS CurrentUnixTime", ACCOUNT_SESSIONS_TABLE,
			"WHERE AccountID = '{}'", Data.m_AccountID);
		pPrevious->AtExecute([pContext](ResultPtr pPreviousRes) { OnReadSessionOwner(pContext, pPreviousRes); });
	}

	static void OnReadSessionOwner(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		auto& Data = pContext->Data();
		const bool HasPrevious = pRes->next();
		Data.m_PreviousServerID = HasPrevious ? pRes->getInt("ServerID") : 0;
		if(HasPrevious)
		{
			Data.m_PreviousClientID = pRes->getInt("ClientID");
			Data.m_PreviousToken = pRes->getString("SessionToken");
			Data.m_PreviousHeartbeat = pRes->getInt("Heartbeat");
			const int CurrentUnixTime = pRes->getInt("CurrentUnixTime");
			if(Data.m_PreviousHeartbeat > 1 && CurrentUnixTime >= Data.m_PreviousHeartbeat)
				Data.m_PreviousHeartbeatAge = CurrentUnixTime - Data.m_PreviousHeartbeat;
		}

		Database->Execute<DB::INSERT>([pContext](bool Success) { OnClaimSession(pContext, Success); },
			ACCOUNT_SESSIONS_TABLE,
			"(AccountID, ServerID, ClientID, SessionToken, Heartbeat, UpdatedAt) VALUES ('{}', '{}', '{}', '{}', UNIX_TIMESTAMP(), NOW()) ON DUPLICATE KEY UPDATE ServerID = VALUES(ServerID), ClientID = VALUES(ClientID), SessionToken = VALUES(SessionToken), Heartbeat = UNIX_TIMESTAMP(), UpdatedAt = NOW()",
			Data.m_AccountID, g_Config.m_SvSyncServerID, pContext->GetClientID(), Data.m_SessionToken);
	}

	static void OnClaimSession(const CAuthorizationContextPtr& pContext, bool Success)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!Success)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: account session could not be reserved.");
			return;
		}

		const auto& Data = pContext->Data();
		auto pCheck = Database->Prepare<DB::SELECT>("ServerID, SessionToken", ACCOUNT_SESSIONS_TABLE,
			"WHERE AccountID = '{}' FOR UPDATE", Data.m_AccountID);
		pCheck->AtExecute([pContext](ResultPtr pRes) { OnValidateClaim(pContext, pRes); });
	}

	static void OnValidateClaim(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		const auto& Data = pContext->Data();
		if(!pRes->next() || pRes->getString("SessionToken") != Data.m_SessionToken)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: another login took over this account.");
			return;
		}

		auto pData = Database->Prepare<DB::SELECT>("*", "tw_accounts_data", "WHERE ID = '{}'", Data.m_AccountID);
		pData->AtExecute([pContext](ResultPtr pDataRes) { OnLoadAccountData(pContext, pDataRes); });
	}


	static void OnLoadAccountData(const CAuthorizationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer)
			return;

		if(!pRes->next())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Login failed: account data could not be loaded.");
			pContext->GS()->Chat(pContext->GetClientID(), "Please reconnect and try again.");
			return;
		}

		const auto& Data = pContext->Data();
		if(!pPlayer->GetSharedData().m_GuestLogin.empty() && Data.m_Login != pPlayer->GetSharedData().m_GuestLogin)
			pPlayer->GetSharedData().m_GuestLogin.clear();

		pPlayer->Account()->Init(Data.m_AccountID, pContext->GetClientID(), Data.m_Login.c_str(), Data.m_Language, Data.m_LoginDate, std::move(pRes));
		pPlayer->Account()->SetSessionToken(Data.m_SessionToken);
		Data.m_pAccountManager->BeginSessionTakeover(pPlayer, Data.m_SessionToken, Data.m_PreviousServerID, Data.m_PreviousClientID, Data.m_PreviousToken, Data.m_PreviousHeartbeat, Data.m_PreviousHeartbeatAge, Data.m_PinCode.empty() && !pPlayer->IsGuestLogin());
	}

public:
	static void StartAuthorization(CAccountManager* pMgr, int ClientID, std::string Login, std::string Pass, std::string Nick)
	{
		auto pContext = DbAsync::MakeContext<DbAuthorization::CAuthorizationPayload>(ClientID,
			DbAuthorization::CAuthorizationPayload { pMgr, Login, Pass, 0, {}, {}, {}, GenerateSessionToken(), 0, 0, {}, 0, -1 });
		auto pAuth = Database->Prepare<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", Nick);
		pAuth->AtExecute([pContext](ResultPtr pRes) { DbAuthorization::OnCheckNickname(pContext, pRes); });
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
	};
	using CRegistrationContextPtr = std::shared_ptr<DbAsync::CContext<CRegistrationPayload>>;

	static void CleanupAccountOnError(const CRegistrationContextPtr& pContext)
	{
		const auto& Data = pContext->Data();
		if(Data.m_InitID > 0)
			Database->Execute<DB::REMOVE>("tw_accounts", "WHERE ID = '{}'", Data.m_InitID);
		else
		{
			const auto cClearLogin = CSqlString<64>(Data.m_Login.c_str());
			Database->Execute<DB::REMOVE>("tw_accounts", "WHERE Username = '{}' AND Password = '{}' AND PasswordSalt = '{}' ORDER BY ID DESC LIMIT 1",
				cClearLogin.cstr(), Data.m_PasswordHash.c_str(), Data.m_PasswordSalt.c_str());
		}

		pContext->GS()->Chat(pContext->GetClientID(), "Registration failed due to server error.");
		pContext->GS()->Chat(pContext->GetClientID(), "Please try again a bit later.");
	}

	static void OnLookup(const CRegistrationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pGS = pContext->GS();
		const auto& Data = pContext->Data();
		const int ClientID = pContext->GetClientID();
		const auto pPlayer = pContext->GetPlayer();

		if(pRes->next() && pPlayer)
		{
			const auto AccountID = pRes->getInt("ID");
			const auto CurrentLogin = pRes->getString("Username");
			const auto TimeoutCode = pRes->getString("TimeoutCode");
			if(!CAccountManager::IsGuestCredentialForNickname(CurrentLogin, Data.m_Nickname.c_str()))
			{
				pGS->Chat(ClientID, "This nickname is already linked to another account.");
				pGS->Chat(ClientID, "Change nickname or contact support for recovery.");
				pGS->Chat(ClientID, "Discord: \"{~}\".", g_Config.m_SvDiscordInviteLink);
				return;
			}

			if(!TimeoutCode.empty() && !pPlayer->IsAuthed())
			{
				pGS->Chat(ClientID, "This nickname is protected by timeout code.");
				pGS->Chat(ClientID, "First complete authorization of the previous account.");
				return;
			}

			Database->Execute<DB::UPDATE>([pContext](bool Updated)
			{
				if(!Updated)
				{
					pContext->GS()->Chat(pContext->GetClientID(), "Registration update failed.");
					pContext->GS()->Chat(pContext->GetClientID(), "Please try again later.");
					return;
				}

				pContext->GS()->Chat(pContext->GetClientID(), "Registration completed successfully.");
				pContext->GS()->Chat(pContext->GetClientID(), "Remember your login and password.");
				pContext->GS()->Chat(pContext->GetClientID(), "Your nickname is your unique game identifier.");
			}, "tw_accounts", "Username = '{}', Password = '{}', PasswordSalt = '{}' WHERE ID = '{}'", Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, AccountID);
			return;
		}

		auto pReg = Database->Prepare<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", Data.m_Nickname);
		pReg->AtExecute([pContext](ResultPtr pRes) { DbRegistration::OnCheckNickname(pContext, pRes); });
	}

	static void OnCheckNickname(const CRegistrationContextPtr& pContext, ResultPtr pRes)
	{
		auto* pGS = pContext->GS();
		if(pRes->next())
		{
			pGS->Chat(pContext->GetClientID(), "This nickname is already linked to another account.");
			pGS->Chat(pContext->GetClientID(), "Change nickname or contact support for recovery.");
			pGS->Chat(pContext->GetClientID(), "Discord: \"{~}\".", g_Config.m_SvDiscordInviteLink);
			return;
		}

		const auto& Data = pContext->Data();
		Database->Execute<DB::INSERT>([pContext](bool Updated) { OnInsertAccount(pContext, Updated); },
			"tw_accounts", "(Username, Password, PasswordSalt, RegisterDate, RegisteredIP) VALUES ('{}', '{}', '{}', UTC_TIMESTAMP(), '{}')",
			Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, Data.m_RegisteredIP);

	}

	static void OnInsertAccount(const CRegistrationContextPtr& pContext, bool Updated)
	{
		if(!Updated)
		{
			CleanupAccountOnError(pContext);
			return;
		}

		const auto& Data = pContext->Data();
		auto pInitID = Database->Prepare<DB::SELECT>("ID", "tw_accounts",
			"WHERE Username = '{}' AND Password = '{}' AND PasswordSalt = '{}' ORDER BY ID DESC LIMIT 1",
			Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt);
		pInitID->AtExecute([pContext](ResultPtr pRes) { OnResolveAccountID(pContext, pRes); });
	}

	static void OnResolveAccountID(const CRegistrationContextPtr& pContext, ResultPtr pRes)
	{
		if(!pRes->next())
		{
			CleanupAccountOnError(pContext);
			return;
		}

		pContext->Data().m_InitID = pRes->getInt("ID");
		Database->Execute<DB::INSERT>([pContext](bool Updated) { OnInsertAccountData(pContext, Updated); },
			"tw_accounts_data", "(ID, Nick) VALUES ('{}', '{}')", pContext->Data().m_InitID, pContext->Data().m_Nickname);
	}

	static void OnInsertAccountData(const CRegistrationContextPtr& pContext, bool Success)
	{
		if(!Success)
		{
			CleanupAccountOnError(pContext);
			return;
		}

		AuthorizeAfterSuccess(pContext);
	}

	static void AuthorizeAfterSuccess(const CRegistrationContextPtr& pContext)
	{
		const auto& Data = pContext->Data();
		pContext->Server()->UpdateAccountBase(Data.m_InitID, Data.m_Nickname, g_Config.m_SvMinRating);
		DbAuthorization::StartAuthorization(Data.m_pAccountManager, pContext->GetClientID(), Data.m_Login, Data.m_Password, Data.m_Nickname);
	}

public:
	static void StartRegistration(CAccountManager* pMgr, int ClientID, std::string Login, std::string Pass, std::string Nick, std::string RegisterIP)
	{
		char aSalt[32] = { 0 };
		secure_random_password(aSalt, sizeof(aSalt), 24);
		const auto AccountPasswordHash = CAccountManager::HashPassword(Pass, aSalt);
		auto pContext = DbAsync::MakeContext<DbRegistration::CRegistrationPayload>(ClientID,
			DbRegistration::CRegistrationPayload { pMgr, 0, Login, Pass, AccountPasswordHash, aSalt, Nick, RegisterIP });
		auto pLookup = Database->Prepare<DB::SELECT>("a.ID, a.Username, a.TimeoutCode", "tw_accounts_data d JOIN tw_accounts a ON a.ID = d.ID", "WHERE d.Nick = '{}' LIMIT 1", Nick);
		pLookup->AtExecute([pContext](ResultPtr pRes) { OnLookup(pContext, pRes); });
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
	{
		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		m_vPendingLogins.erase(std::remove_if(m_vPendingLogins.begin(), m_vPendingLogins.end(),
			[ClientID](const SPendingLogin& Pending) { return Pending.m_ClientID == ClientID; }), m_vPendingLogins.end());
		m_vSessionEvents.erase(std::remove(m_vSessionEvents.begin(), m_vSessionEvents.end(), ClientID), m_vSessionEvents.end());
	}

	int AccountID = 0;
	std::string SessionToken;
	if(const auto It = CAccountData::ms_aData.find(ClientID); It != CAccountData::ms_aData.end())
	{
		AccountID = It->second.GetID();
		SessionToken = It->second.GetSessionToken();
	}

	if(AccountID > 0 && !SessionToken.empty())
	{
		Database->Execute<DB::REMOVE>(ACCOUNT_SESSIONS_TABLE,
			"WHERE AccountID = '{}' AND SessionToken = '{}'", AccountID, SessionToken);
	}

	CAccountSharedData::ms_aPlayerSharedData.erase(ClientID);
	CAccountData::ms_aData.erase(ClientID);
}

void CAccountManager::BeginSessionTakeover(CPlayer* pPlayer, const std::string& Token, int PreviousServerID, int PreviousClientID, const std::string& PreviousToken, int PreviousHeartbeat, int PreviousHeartbeatAge, bool RecommendPin)
{
	if(!pPlayer || Token.empty())
		return;

	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	if(AccountID <= 0)
		return;

	QueueLocalSessionKicks(AccountID, Token);

	SPendingLogin Pending {};
	Pending.m_ClientID = ClientID;
	Pending.m_AccountID = AccountID;
	Pending.m_Token = Token;
	Pending.m_PreviousServerID = PreviousServerID;
	Pending.m_PreviousClientID = PreviousClientID;
	Pending.m_PreviousToken = PreviousToken;
	Pending.m_PreviousHeartbeat = PreviousHeartbeat;
	Pending.m_PreviousHeartbeatAge = PreviousHeartbeatAge;
	Pending.m_StartTick = Server()->Tick();
	Pending.m_RecommendPin = RecommendPin;

	{
		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		m_vPendingLogins.push_back(Pending);
		if(PreviousServerID > 0 && PreviousServerID != g_Config.m_SvSyncServerID)
			m_vSessionEvents.push_back(ClientID);
	}

	GS()->Chat(ClientID, "Verifying account session...");
}

void CAccountManager::QueueLocalSessionKicks(int AccountID, const std::string& Token)
{
	std::vector<SPendingKick> vKicks;
	for(const auto& [ClientID, Data] : CAccountData::ms_aData)
	{
		if(Data.GetID() == AccountID && !Data.GetSessionToken().empty() && Data.GetSessionToken() != Token)
			vKicks.push_back({ClientID, AccountID, Data.GetSessionToken()});
	}

	if(vKicks.empty())
		return;

	std::lock_guard<std::mutex> Lock(m_SessionMutex);
	for(auto& Kick : vKicks)
		m_vPendingKicks.push_back(std::move(Kick));
}

void CAccountManager::SendSessionEvents()
{
	std::vector<int> vClients;
	std::lock_guard<std::mutex> Lock(m_SessionMutex);
	vClients.swap(m_vSessionEvents);

	for(const int ClientID : vClients)
	{
		auto It = std::find_if(m_vPendingLogins.begin(), m_vPendingLogins.end(),
			[ClientID](const SPendingLogin& Pending) { return Pending.m_ClientID == ClientID; });
		if(It == m_vPendingLogins.end())
			continue;

		const int EventID = Sync->Push(syncevents::ACCOUNT_SESSION_TAKEOVER, GS()->GetWorldID(), It->m_AccountID);
		if(EventID > 0)
			It->m_EventID = EventID;
		else
			It->m_RelayFailed = true;
	}
}

bool CAccountManager::HasAnyLocalSession(int AccountID) const
{
	for(const auto& [ClientID, Data] : CAccountData::ms_aData)
	{
		if(Data.GetID() == AccountID && !Data.GetSessionToken().empty())
			return true;
	}
	return false;
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

void CAccountManager::CompleteLogin(int ClientID, int AccountID, const std::string& Token, bool RecommendPin, int PreviousServerID)
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
	auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
	if(!pGS || !pPlayer || !pPlayer->IsAuthed())
		return;

	const auto It = CAccountData::ms_aData.find(ClientID);
	if(It == CAccountData::ms_aData.end() || It->second.GetID() != AccountID || It->second.GetSessionToken() != Token)
		return;

	pGS->Chat(ClientID, "Login successful. Welcome back!");
	if(PreviousServerID > 0 && PreviousServerID != g_Config.m_SvSyncServerID)
		pGS->Chat(ClientID, "Warning: The previous account session on another server was terminated.");
	if(RecommendPin)
	{
		pGS->Chat(ClientID, "Security recommendation: set a PIN code.");
		pGS->Chat(ClientID, "Command: /set_pin <current_password> <new_pin>.");
	}

	if(pPlayer->m_pMotdMenu)
		pPlayer->CloseMotdMenu();
	pGS->m_pController->DoTeamChange(pPlayer);
	pGS->Core()->AccountManager()->LoadAccount(pPlayer, true);
	g_EventListenerManager.Notify<IEventListener::PlayerLogin>(pPlayer, pPlayer->Account());
}

void CAccountManager::ProcessPendingLogins()
{
	const int TimeoutTicks = Server()->TickSpeed() * ACCOUNT_SESSION_TAKEOVER_TIMEOUT_SECONDS;
	std::vector<SPendingLogin> vTimedOut;
	std::vector<SPendingLogin> vRelayFailed;
	std::vector<SPendingLogin> vCompleted;

	auto QueueFailure = [&](const SPendingLogin& Pending, bool RelayFailure)
	{
		if(RelayFailure)
			vRelayFailed.push_back(Pending);
		else
			vTimedOut.push_back(Pending);
	};

	{
		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		for(auto It = m_vPendingLogins.begin(); It != m_vPendingLogins.end();)
		{
			auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(It->m_ClientID));
			auto* pPlayer = pGS ? pGS->GetPlayer(It->m_ClientID, false) : nullptr;
			if(!pGS || !pPlayer || !pPlayer->IsAuthed())
			{
				It = m_vPendingLogins.erase(It);
				continue;
			}

			const bool NeedRemoteAck = It->m_PreviousServerID > 0 && It->m_PreviousServerID != g_Config.m_SvSyncServerID;
			const bool TimedOut = Server()->Tick() - It->m_StartTick >= TimeoutTicks;
			const bool WaitingForRemote = NeedRemoteAck && !It->m_RemoteAck;

			if(HasOtherLocalSession(It->m_AccountID, It->m_Token))
			{
				if(TimedOut || It->m_RelayFailed)
				{
					QueueFailure(*It, It->m_RelayFailed);
					It = m_vPendingLogins.erase(It);
				}
				else
					++It;
				continue;
			}

			if(WaitingForRemote)
			{
				if(TimedOut || It->m_RelayFailed)
				{
					const int ElapsedSeconds = (Server()->Tick() - It->m_StartTick) / Server()->TickSpeed();
					const bool PreviousHeartbeatStale = It->m_PreviousHeartbeatAge >= 0
						&& It->m_PreviousHeartbeatAge + ElapsedSeconds >= ACCOUNT_SESSION_HEARTBEAT_STALE_SECONDS;
					if(PreviousHeartbeatStale)
						vCompleted.push_back(*It);
					else
						QueueFailure(*It, It->m_RelayFailed);

					It = m_vPendingLogins.erase(It);
				}
				else
					++It;
				continue;
			}

			if(TimedOut || It->m_RelayFailed)
			{
				if(It->m_RemoteAck)
					vCompleted.push_back(*It);
				else
					QueueFailure(*It, It->m_RelayFailed);

				It = m_vPendingLogins.erase(It);
				continue;
			}

			vCompleted.push_back(*It);
			It = m_vPendingLogins.erase(It);
		}
	}

	for(const auto& Pending : vTimedOut)
	{
		RestorePreviousSession(Pending);
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(Pending.m_ClientID));
		if(!pGS)
			continue;

		pGS->Chat(Pending.m_ClientID, "Login failed: session takeover was not confirmed.");
		pGS->Server()->Kick(Pending.m_ClientID, "Session takeover was not confirmed");
	}

	for(const auto& Pending : vRelayFailed)
	{
		RestorePreviousSession(Pending);
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(Pending.m_ClientID));
		if(!pGS)
			continue;

		pGS->Chat(Pending.m_ClientID, "Login failed: session takeover relay is unavailable.");
		pGS->Server()->Kick(Pending.m_ClientID, "Session takeover relay unavailable");
	}

	for(const auto& Pending : vCompleted)
		CompleteLogin(Pending.m_ClientID, Pending.m_AccountID, Pending.m_Token, Pending.m_RecommendPin, Pending.m_PreviousServerID);
}

void CAccountManager::RestorePreviousSession(const SPendingLogin& Pending)
{
	if(Pending.m_PreviousServerID <= 0 || Pending.m_PreviousToken.empty())
		return;

	Database->Execute<DB::UPDATE>(ACCOUNT_SESSIONS_TABLE,
		"ServerID = '{}', ClientID = '{}', SessionToken = '{}', Heartbeat = '{}', UpdatedAt = NOW() WHERE AccountID = '{}' AND ServerID = '{}' AND SessionToken = '{}'",
		Pending.m_PreviousServerID, Pending.m_PreviousClientID, Pending.m_PreviousToken, Pending.m_PreviousHeartbeat,
		Pending.m_AccountID, g_Config.m_SvSyncServerID, Pending.m_Token);
}

void CAccountManager::UpdateLocalSessionHeartbeats()
{
	for(const auto& [ClientID, Data] : CAccountData::ms_aData)
	{
		if(Data.GetID() <= 0 || Data.GetSessionToken().empty())
			continue;

		Database->Execute<DB::UPDATE>(ACCOUNT_SESSIONS_TABLE,
			"Heartbeat = UNIX_TIMESTAMP(), UpdatedAt = NOW() WHERE AccountID = '{}' AND SessionToken = '{}'",
			Data.GetID(), Data.GetSessionToken());
	}
}

void CAccountManager::ValidateLocalSessions(int AccountID, int AckEventID, int AckWorldID, int AckTargetServerID)
{
	if(AccountID <= 0)
		return;

	auto pCheck = Database->Prepare<DB::SELECT>("ServerID, SessionToken", ACCOUNT_SESSIONS_TABLE,
		"WHERE AccountID = '{}' FOR UPDATE", AccountID);
	pCheck->AtExecute([this, AccountID, AckEventID, AckWorldID, AckTargetServerID](ResultPtr pRes)
	{
		if(!pRes->next())
			return;

		const auto SessionToken = pRes->getString("SessionToken");
		if(SessionToken.empty())
			return;

		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		m_vSessionValidations.push_back({AccountID, SessionToken, AckEventID, AckWorldID, AckTargetServerID});
	});
}

void CAccountManager::OnSyncEvent(std::string EventName, const CServerSync::SRequest& Req)
{
	const std::string AckEvent = std::string(syncevents::ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX) + std::to_string(g_Config.m_SvSyncServerID);
	if(EventName == AckEvent)
	{
		if(Req.Value <= 0)
			return;

		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		for(auto& Pending : m_vPendingLogins)
		{
			if(Pending.m_EventID == Req.Value && Pending.m_PreviousServerID == Req.ServerID)
			{
				Pending.m_RemoteAck = true;
				break;
			}
		}
		return;
	}

	if(EventName != syncevents::ACCOUNT_SESSION_TAKEOVER || Req.Value <= 0)
		return;

	if(!HasAnyLocalSession(Req.Value))
	{
		const std::string AckTarget = std::string(syncevents::ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX) + std::to_string(Req.ServerID);
		Sync->Push(AckTarget.c_str(), Req.WorldID, Req.ID);
		return;
	}

	ValidateLocalSessions(Req.Value, Req.ID, Req.WorldID, Req.ServerID);
}

void CAccountManager::OnTick()
{
	ProcessPendingKicks();
	SendSessionEvents();
	ProcessPendingLogins();

	if(GS()->GetWorldID() != INITIALIZER_WORLD_ID)
		return;

	const int RefreshSeconds = g_Config.m_SvAccountSessionRefresh;
	if(RefreshSeconds <= 0)
		return;

	const int64_t Now = time_get() / time_freq();
	if(m_LastSessionCheck != 0 && Now - m_LastSessionCheck < RefreshSeconds)
		return;
	m_LastSessionCheck = (int)Now;

	UpdateLocalSessionHeartbeats();

	std::vector<int> vAccounts;
	for(const auto& [ClientID, Data] : CAccountData::ms_aData)
	{
		if(Data.GetID() <= 0 || Data.GetSessionToken().empty())
			continue;

		if(std::find(vAccounts.begin(), vAccounts.end(), Data.GetID()) == vAccounts.end())
			vAccounts.push_back(Data.GetID());
	}

	for(const int AccountID : vAccounts)
		ValidateLocalSessions(AccountID);
}

void CAccountManager::ProcessPendingKicks()
{
	std::vector<SSessionValidation> vValidations;
	{
		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		vValidations.swap(m_vSessionValidations);
	}

	std::vector<SPendingKick> vKicks;
	for(const auto& Validation : vValidations)
	{
		for(const auto& [ClientID, Data] : CAccountData::ms_aData)
		{
			if(Data.GetID() == Validation.m_AccountID && !Data.GetSessionToken().empty()
				&& Data.GetSessionToken() != Validation.m_Token)
			{
				vKicks.push_back({ClientID, Data.GetID(), Data.GetSessionToken()});
			}
		}
	}

	{
		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		for(auto& Kick : vKicks)
			m_vPendingKicks.push_back(std::move(Kick));
	}

	std::vector<SPendingKick> vPendingKicks;
	{
		std::lock_guard<std::mutex> Lock(m_SessionMutex);
		vPendingKicks.swap(m_vPendingKicks);
	}

	for(const auto& Kick : vPendingKicks)
	{
		auto It = CAccountData::ms_aData.find(Kick.m_ClientID);
		if(It == CAccountData::ms_aData.end() || It->second.GetID() != Kick.m_AccountID
			|| It->second.GetSessionToken() != Kick.m_Token)
		{
			continue;
		}

		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(Kick.m_ClientID));
		if(!pGS || !pGS->GetPlayer(Kick.m_ClientID, false))
			continue;

		// Stop the old session from acting during its final disconnect tick.
		It->second.SetReadyToPlay(false);
		pGS->Server()->Kick(Kick.m_ClientID, ACCOUNT_SESSION_TAKEOVER_REASON);
	}

	for(const auto& Validation : vValidations)
	{
		if(Validation.m_AckEventID > 0)
		{
			const std::string AckTarget = std::string(syncevents::ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX) + std::to_string(Validation.m_AckTargetServerID);
			Sync->Push(AckTarget.c_str(), Validation.m_AckWorldID, Validation.m_AckEventID);
		}
	}
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
		const auto currencyItemIDs = CInventoryManager::GetItemsCollection(ItemGroup::Currency, std::nullopt);
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
		auto* pAccount = pPlayer->Account();
		const auto gold = pAccount->GetGold();
		const auto goldCapacity = pAccount->GetGoldCapacity();
		const auto bankGold = pAccount->GetBankManager();

		// personal assistant
		MotdMenu MAssistant(ClientID, MTFLAG_CLOSE_BUTTON, "Quick tips update as you progress.");
		MAssistant.AddText("Profession: {}", GetProfessionName(pAccount->GetActiveProfessionID()));
		MAssistant.AddText("Level {}.", pAccount->GetLevel());
		MAssistant.AddText("Gold: {$}/{$}", gold, goldCapacity);
		MAssistant.AddText("Bank: {$}", bankGold);
		MAssistant.AddText("World: {}", Server()->GetWorldName(pPlayer->GetCurrentWorldID()));
		MAssistant.AddSeparateLine();

		// recommendations
		MAssistant.AddText("Recommendations:");
		bool hasRecommendation = false;
		const int mailCount = Core()->MailboxManager()->GetMailCount(pAccount->GetID());
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

	return RegisterAccountRaw(ClientID, pLogin, pPassword);
}

AccountCodeResult CAccountManager::RegisterAccountRaw(int ClientID, const char* pLogin, const char* pPassword)
{
	const auto cClearLogin = CSqlString<32>(pLogin);
	const auto cClearPass = CSqlString<32>(pPassword);
	const auto cClearNick = CSqlString<32>(Server()->ClientName(ClientID));

	char aAddrStr[64] {};
	Server()->GetClientAddr(ClientID, aAddrStr, sizeof(aAddrStr));
	DbRegistration::StartRegistration(this, ClientID, cClearLogin.cstr(), cClearPass.cstr(), cClearNick.cstr(), aAddrStr);
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
	const auto sqlStrLogin = CSqlString<64>(pLogin);
	const auto sqlStrPass = CSqlString<64>(pPassword);
	const auto sqlStrNick = CSqlString<32>(Server()->ClientName(ClientID));
	DbAuthorization::StartAuthorization(this, ClientID, sqlStrLogin.cstr(), sqlStrPass.cstr(), sqlStrNick.cstr());
	return AccountCodeResult::AOP_LOGIN_OK;
}

void CAccountManager::TryLoginGuestByTimeoutCode(int ClientID, const char* pNickname, const char* pCode, const char* pGuestLogin)
{
	const auto cNick = CSqlString<32>(pNickname);
	const auto cCode = CSqlString<64>(pCode);
	const auto cGuest = CSqlString<64>(pGuestLogin);
	auto pCheck = Database->Prepare<DB::SELECT>("a.ID", "tw_accounts a JOIN tw_accounts_data d ON d.ID = a.ID",
		"WHERE d.Nick = '{}' AND a.Username = '{}' AND a.TimeoutCode = '{}' LIMIT 1", cNick.cstr(), cGuest.cstr(), cCode.cstr());
	pCheck->AtExecute([this, ClientID, GuestLogin = std::string(pGuestLogin)](ResultPtr pRes)
	{
		auto* pPlayer = GS()->GetPlayer(ClientID, false);
		if(!pPlayer || pPlayer->IsAuthed())
			return;

		if(!pRes->next())
		{
			GS()->Chat(ClientID, "Invalid timeout code. Please try /timeout <code> again.");
			return;
		}

		LoginAccountRaw(ClientID, GuestLogin.c_str(), GuestLogin.c_str());
		GS()->Chat(ClientID, mystd::aesthetic::wrapLinePillar(12).c_str());
		GS()->Chat(ClientID, "Timeout authorization successful.");
		GS()->Chat(ClientID, "Do not change timeout code, or you may lose access.");
		GS()->Chat(ClientID, "Recommended next step: /register <login> <pass>.");
		GS()->Chat(ClientID, mystd::aesthetic::wrapLinePillar(12).c_str());
	});
}

void CAccountManager::LoadAccount(CPlayer* pPlayer, bool FirstInitilize)
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	// load same player context
	if(!GS()->IsPlayerInWorld(pPlayer->GetCID()))
	{
		auto* pActiveGS = static_cast<CGS*>(Instance::GameServerPlayer(pPlayer->GetCID()));
		if(auto* pActivePlayer = pActiveGS->GetPlayer(pPlayer->GetCID(), false))
			pActiveGS->Core()->AccountManager()->LoadAccount(pActivePlayer, FirstInitilize);
		return;
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
		const int Letters = Core()->MailboxManager()->GetMailCount(pAccount->GetID());
		if(Letters > 0)
		{
			GS()->Chat(ClientID, "You have '{} unread letters'.", Letters);
		}

		pAccount->GetBonusManager().SendInfoAboutActiveBonuses();
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		return;
	}

	// on player login
	Core()->OnPlayerLogin(pPlayer);

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
	pAccount->SetReadyToPlay(true);

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

	// fetch current pin code
	const auto AccountID = pPlayer->Account()->GetID();
	std::string CurrentPinInDb;
	bool bPinCurrentlySet = GetAccountPin(AccountID, CurrentPinInDb);

	if(IsChangingPin)
	{
		// is not set need set pin
		if(!bPinCurrentlySet)
		{
			GS()->Chat(ClientID, "You don't have a PIN code set.");
			GS()->Chat(ClientID, "Use '/set_pin' to set your PIN.");
			return;
		}

		// check valid pin
		if(CurrentPinInDb != pCurrentPasswordOrPin)
		{
			GS()->Chat(ClientID, "The current PIN code you entered is incorrect.");
			return;
		}
		if(str_comp(pCurrentPasswordOrPin, pNewPin) == 0)
		{
			GS()->Chat(ClientID, "The new PIN cannot be the same as the old PIN.");
			return;
		}
	}
	else
	{
		// is set need change pin
		if(bPinCurrentlySet)
		{
			GS()->Chat(ClientID, "You already have a PIN code set.");
			GS()->Chat(ClientID, "Use '/change_pin' to update your PIN.");
			return;
		}

		// check valid password
		ResultPtr pResAccount = Database->Execute<DB::SELECT>("Password, PasswordSalt", "tw_accounts", "WHERE ID = '{}'", AccountID);
		if(!pResAccount->next())
		{
			GS()->Chat(ClientID, "Error retrieving account data.");
			return;
		}

		const auto DbPassword = pResAccount->getString("Password");
		const auto DbSalt = pResAccount->getString("PasswordSalt");
		const CSqlString<32> sqlCurrentPassword(pCurrentPasswordOrPin);
		if(DbPassword != HashPassword(sqlCurrentPassword.cstr(), DbSalt))
		{
			GS()->Chat(ClientID, "The account password you entered is incorrect.");
			return;
		}
	}

	// save new pin-code
	const CSqlString<16> sqlNewPin(pNewPin);
	Database->Execute<DB::UPDATE>("tw_accounts", "PinCode = '{}' WHERE ID = '{}'", sqlNewPin.cstr(), AccountID);
	GS()->Chat(ClientID, "Your PIN code has been successfully {}!", Instance::Localize(ClientID, IsChangingPin ? "changed" : "set"));
	return;
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

	// get pincode
	std::string PinFromDb;
	const int AccountID = pPlayer->Account()->GetID();
	bool bPinIsSet = GetAccountPin(AccountID, PinFromDb);

	// check is pincode set
	if(!bPinIsSet)
	{
		GS()->Chat(ClientID, "For security, you must first set up a PIN code.");
		GS()->Chat(ClientID, "Use '/set_pin' to set your PIN.");
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

	// check valid pincode
	if(PinFromDb != pPinCode)
	{
		GS()->Chat(ClientID, "The PIN code you entered is incorrect.");
		return;
	}

	// get current password
	ResultPtr pResAccount = Database->Execute<DB::SELECT>("Password, PasswordSalt", "tw_accounts", "WHERE ID = '{}'", AccountID);
	if(!pResAccount->next())
	{
		GS()->Chat(ClientID, "Error retrieving account data.");
		return;
	}

	// check valid password
	const auto CurrentDbPassword = pResAccount->getString("Password");
	const auto CurrentDbSalt = pResAccount->getString("PasswordSalt");
	const CSqlString<32> sqlOldPassword(pOldPassword);
	if(CurrentDbPassword != HashPassword(sqlOldPassword.cstr(), CurrentDbSalt))
	{
		GS()->Chat(ClientID, "The old password you entered is incorrect.");
		return;
	}

	// update new password
	char aNewSalt[32] = { 0 };
	secure_random_password(aNewSalt, sizeof(aNewSalt), 24);
	const CSqlString<32> sqlNewPassword(pNewPassword);
	const std::string HashedNewPassword = HashPassword(sqlNewPassword.cstr(), aNewSalt);
	Database->Execute<DB::UPDATE>("tw_accounts", "Password = '{}', PasswordSalt = '{}' WHERE ID = '{}'", HashedNewPassword, aNewSalt, AccountID);

	// information
	GS()->Chat(ClientID, "Your password has been successfully changed!");
	GS()->Chat(ClientID, "Please remember your new login details.");
	GS()->Chat(ClientID, "New password: '{}'.", sqlNewPassword.cstr());
	return;
}

bool CAccountManager::ChangeNickname(const std::string& newNickname, int ClientID) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if(!pPlayer)
		return false;

	// check newnickname
	const auto cClearNick = CSqlString<32>(newNickname.c_str());
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_data", "WHERE Nick = '{}'", cClearNick.cstr());
	if(pRes->next())
		return false;

	Database->Execute<DB::UPDATE>("tw_accounts_data", "Nick = '{}' WHERE ID = '{}'", cClearNick.cstr(), pPlayer->Account()->GetID());
	Server()->SetClientName(ClientID, newNickname.c_str());
	return true;
}

void CAccountManager::UseVoucher(int ClientID, const char* pVoucher) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	char aSelect[256];
	const auto cVoucherCode = CSqlString<32>(pVoucher);
	str_format(aSelect, sizeof(aSelect), "v.*, IF((SELECT r.ID FROM tw_voucher_redeemed r WHERE CASE v.Multiple WHEN 1 THEN r.VoucherID = v.ID AND r.UserID = %d ELSE r.VoucherID = v.ID END) IS NULL, FALSE, TRUE) AS used", pPlayer->Account()->GetID());

	ResultPtr pResVoucher = Database->Execute<DB::SELECT>(aSelect, "tw_voucher v", "WHERE v.Code = '{}'", cVoucherCode.cstr());
	if(pResVoucher->next())
	{
		const int VoucherID = pResVoucher->getInt("ID");
		const int ValidUntil = pResVoucher->getInt("ValidUntil");
		nlohmann::json JsonData = nlohmann::json::parse(pResVoucher->getString("Data").c_str());

		if(ValidUntil > 0 && ValidUntil < time(0))
		{
			GS()->Chat(ClientID, "The voucher code '{}' has expired.", pVoucher);
			return;
		}

		if(pResVoucher->getBoolean("used"))
		{
			GS()->Chat(ClientID, "This voucher has already been redeemed.");
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

		GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
		GS()->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);

		Database->Execute<DB::INSERT>("tw_voucher_redeemed", "(VoucherID, UserID, TimeCreated) VALUES ({}, {}, {})", VoucherID, pPlayer->Account()->GetID(), (int)time(0));
		GS()->Chat(ClientID, "You have successfully redeemed the voucher '{}'.", pVoucher);
		return;
	}

	GS()->Chat(ClientID, "The voucher code '{}' does not exist.", pVoucher);
}

bool CAccountManager::BanAccount(CPlayer* pPlayer, CTimePeriod Time, const std::string& Reason)
{
	// Check if the account is already banned
	ResultPtr pResBan = Database->Execute<DB::SELECT>("BannedUntil", "tw_accounts_bans", "WHERE AccountId = '{}' AND current_timestamp() < `BannedUntil`", pPlayer->Account()->GetID());
	if(pResBan->next())
	{
		// Print message and return false if the account is already banned
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "This account is already banned");
		return false;
	}

	// Ban the account
	Database->Execute<DB::INSERT>("tw_accounts_bans", "(AccountId, BannedUntil, Reason) VALUES ('{}', '{}', '{}')",
		pPlayer->Account()->GetID(), std::string("current_timestamp + " + Time.asSqlInterval()), Reason);
	GS()->Server()->Kick(pPlayer->GetCID(), "Your account was banned");
	m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully banned!");
	return true;
}

bool CAccountManager::UnBanAccount(int BanId) const
{
	// Search for ban using the specified BanId
	ResultPtr pResBan = Database->Execute<DB::SELECT>("AccountId", "tw_accounts_bans", "WHERE Id = '{}' AND current_timestamp() < `BannedUntil`", BanId);
	if(pResBan->next())
	{
		// If the ban exists and the current timestamp is still less than the BannedUntil timestamp, unban the account
		Database->Execute<DB::UPDATE>("tw_accounts_bans", "BannedUntil = current_timestamp WHERE Id = '{}'", BanId);
		m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Successfully unbanned!");
		return true;
	}

	// If the ban does not exist or the current timestamp is already greater than the BannedUntil timestamp, print an error message
	m_GameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Ban is not valid anymore or does not exist!");
	return false;
}

// Function: BansAccount
std::vector<CAccountManager::AccBan> CAccountManager::BansAccount() const
{
	constexpr std::size_t capacity = 100;
	std::vector<AccBan> out;
	out.reserve(capacity);

	/*
		Execute a SELECT query on the "tw_accounts_bans" table in the database,
		retrieving the columns "Id", "BannedUntil", "Reason", and "AccountId"
		where the current timestamp is less than the "BannedUntil" value
	*/
	ResultPtr pResBan = Database->Execute<DB::SELECT>("Id, BannedUntil, Reason, AccountId", "tw_accounts_bans", "WHERE current_timestamp() < `BannedUntil`");
	while(pResBan->next())
	{
		int ID = pResBan->getInt("Id");
		int AccountID = pResBan->getInt("AccountId");
		std::string BannedUntil = pResBan->getString("BannedUntil");
		std::string PlayerNickname = Server()->GetAccountNickname(AccountID);
		std::string Reason = pResBan->getString("Reason");
		out.emplace_back(ID, BannedUntil, std::move(PlayerNickname), std::move(Reason));
	}

	return out;
}

bool CAccountManager::GetAccountPin(int AccountID, std::string& OutPinCode) const
{
	ResultPtr pResPin = Database->Execute<DB::SELECT>("PinCode", "tw_accounts", "WHERE ID = '{}'", AccountID);
	if(pResPin->next())
	{
		OutPinCode = pResPin->getString("PinCode");
		return !OutPinCode.empty();
	}

	OutPinCode.clear();
	return false;
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
