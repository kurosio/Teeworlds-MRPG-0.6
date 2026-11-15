/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_auth.h"

#include <base/system.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/tools/db_async_context.h>

#include <optional>
#include <utility>
#include <vector>

namespace
{
	// The primary can be remote (SQL bridge over the WAN), so a login takes seconds.
	constexpr int64_t AUTH_PROGRESS_FIRST_SECONDS = 5;
	constexpr int64_t AUTH_PROGRESS_REPEAT_SECONDS = 15;
	constexpr int CLAIM_ATTEMPTS = 3;

	// Everything a login asks for, in one read of the primary: the replica can be
	// behind a fresh registration or a ban, and account data read from it can be
	// older than what the previous session wrote.
	constexpr const char* AUTH_COLUMNS =
		"d.ID, d.Nick AS AccountNickname, "
		"(a.ID IS NOT NULL AND a.Username = '{}') AS CredentialsFound, "
		"a.Username, a.TimeoutCode, a.LoginDate, a.Language, a.PinCode, a.Password, a.PasswordSalt, "
		"(b.AccountId IS NOT NULL) AS HasBan, b.BannedUntil, b.Reason, "
		"(s.AccountID IS NOT NULL) AS SessionExists, "
		"IFNULL(s.ServerID, 0) AS SessionServerID, IFNULL(s.ClientID, 0) AS SessionClientID, "
		"IFNULL(s.SessionToken, '') AS SessionToken, IFNULL(s.Heartbeat, 0) AS SessionHeartbeat, "
		"UNIX_TIMESTAMP() AS CurrentUnixTime, "
		"IFNULL(d.CrimeScore, 0) AS CrimeScore, IFNULL(d.WorldID, 0) AS WorldID, IFNULL(d.Bank, '0') AS Bank, "
		"IFNULL(d.DailyStamp, 0) AS DailyStamp, IFNULL(d.WeekStamp, 0) AS WeekStamp, IFNULL(d.MonthStamp, 0) AS MonthStamp, "
		"IFNULL(d.EquippedSlots, '') AS EquippedSlots, IFNULL(d.EquipmentPresets, '[]') AS EquipmentPresets, d.ProfessionID";

	constexpr const char* AUTH_TABLES =
		"tw_accounts_data d "
		"LEFT JOIN tw_accounts a ON a.ID = d.ID "
		"LEFT JOIN tw_accounts_bans b ON b.AccountId = a.ID AND CURRENT_TIMESTAMP() < b.BannedUntil "
		"LEFT JOIN tw_account_sessions s ON s.AccountID = d.ID";

	struct SAuthPayload
	{
		CAccountManager* m_pAccountManager {};
		std::string m_Login {};
		std::string m_Password {};
		std::string m_Language {};
		std::string m_LoginDate {};
		std::string m_PinCode {};
		CAccountData::SAccountSnapshot m_Snapshot {};
		AccountSession::SClaim m_Claim {};
		int m_Attempts {};
		int64_t m_Start {};
	};

	using CContextPtr = std::shared_ptr<DbAsync::CContext<SAuthPayload>>;

	CAccountData::SAccountSnapshot ReadSnapshot(ResultPtr pRes)
	{
		CAccountData::SAccountSnapshot Snapshot;
		Snapshot.m_CrimeScore = pRes->getInt("CrimeScore");
		Snapshot.m_WorldID = pRes->getInt("WorldID");
		Snapshot.m_Bank = pRes->getString("Bank");
		Snapshot.m_DailyStamp = pRes->getInt64("DailyStamp");
		Snapshot.m_WeekStamp = pRes->getInt64("WeekStamp");
		Snapshot.m_MonthStamp = pRes->getInt64("MonthStamp");
		Snapshot.m_EquippedSlots = pRes->getString("EquippedSlots");
		Snapshot.m_EquipmentPresets = pRes->getString("EquipmentPresets");
		Snapshot.m_ProfessionID = pRes->getInt("ProfessionID");
		return Snapshot;
	}
}

class CLogin
{
public:
	static void Start(CAccountManager* pManager, int ClientID, std::string Login, std::string Password, std::string Nickname)
	{
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
		if(!pPlayer || !pPlayer->GetSharedData().m_pAuthorizationAttempt.expired() || pPlayer->Account()->GetID() > 0)
			return;

		SAuthPayload Payload {};
		Payload.m_pAccountManager = pManager;
		Payload.m_Login = std::move(Login);
		Payload.m_Password = std::move(Password);
		Payload.m_Claim.m_ClientID = ClientID;
		Payload.m_Claim.m_Token = AccountSession::GenerateToken();
		Payload.m_Claim.m_Deadline = time_get() + AccountSession::CLAIM_SECONDS * time_freq();
		Payload.m_Start = time_get();
		auto pContext = DbAsync::MakeContext<SAuthPayload>(ClientID, std::move(Payload));

		AccountAuth::StartProgress(pPlayer->GetSharedData(), pContext, false);

		const std::string Columns = fmt_default(AUTH_COLUMNS, pContext->Data().m_Login);
		auto pAuth = Database->Prepare<DB::SELECT>(Columns.c_str(), AUTH_TABLES, "WHERE d.Nick = '{}' LIMIT 1", Nickname);
		pAuth->PrimaryRead();
		pAuth->AtExecute([pContext](ResultPtr pRes) { OnAuthRecord(pContext, pRes); });
	}

private:
	static CPlayer* Player(const CContextPtr& pContext)
	{
		auto* pPlayer = pContext->GetPlayer(false);
		if(!pPlayer || pPlayer->GetSharedData().m_pAuthorizationAttempt.lock().get() != pContext.get()
			|| pPlayer->Account()->GetID() > 0)
			return nullptr;
		return pPlayer;
	}

	static void Fail(const CContextPtr& pContext, const char* pMessage, const char* pHint = nullptr)
	{
		if(auto* pGS = pContext->GS())
		{
			pGS->Chat(pContext->GetClientID(), pMessage);
			if(pHint)
				pGS->Chat(pContext->GetClientID(), pHint);
		}
	}

	static void OnAuthRecord(const CContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = Player(pContext);
		if(!pPlayer)
			return;

		// A failed query says nothing about the nickname: only an answered one may.
		if(!pRes || !*pRes)
		{
			Fail(pContext, "Login failed: the account could not be checked.", "Please try again a bit later.");
			return;
		}

		if(!pRes->next())
		{
			Fail(pContext, "Login failed: account with this nickname was not found.");
			return;
		}

		auto& Data = pContext->Data();
		Data.m_Claim.m_AccountID = pRes->getInt("ID");
		Data.m_Language = pRes->getString("Language");
		Data.m_LoginDate = pRes->getString("LoginDate");
		Data.m_PinCode = pRes->getString("PinCode");
		Data.m_Snapshot = ReadSnapshot(pRes);

		if(!pRes->getBoolean("CredentialsFound")
			|| pRes->getString("Password") != CAccountManager::HashPassword(Data.m_Password, pRes->getString("PasswordSalt")))
		{
			Fail(pContext, "Login failed: incorrect login or password.");
			return;
		}

		// All entry points, including /login, must enforce guest protection.
		const auto TimeoutCode = pRes->getString("TimeoutCode");
		if(!TimeoutCode.empty()
			&& CAccountManager::IsGuestCredentialForNickname(pRes->getString("Username"), pRes->getString("AccountNickname").c_str())
			&& TimeoutCode != pPlayer->GetSharedData().m_TimeoutCode)
		{
			Fail(pContext, "Invalid timeout code. Please try /timeout <code> again.");
			return;
		}

		if(pRes->getBoolean("HasBan"))
		{
			Fail(pContext, "Account is temporarily blocked.");
			if(auto* pGS = pContext->GS())
				pGS->Chat(pContext->GetClientID(), "Until: '{}'. Reason: '{}'.", pRes->getString("BannedUntil"), pRes->getString("Reason"));
			return;
		}

		if(pRes->getBoolean("SessionExists"))
		{
			Data.m_Claim.m_Previous = AccountSession::DecodeRecord(pRes->getInt("SessionServerID"), pRes->getInt("SessionClientID"),
				pRes->getString("SessionToken"), pRes->getInt64("SessionHeartbeat"), pRes->getInt64("CurrentUnixTime"));
		}
		TryClaim(pContext);
	}

	static void TryClaim(const CContextPtr& pContext)
	{
		AccountSession::TryClaim(pContext->Data().m_Claim, [pContext](bool Claimed) { OnClaimed(pContext, Claimed); });
	}

	static void OnClaimed(const CContextPtr& pContext, bool Claimed)
	{
		if(Claimed && Player(pContext) && time_get() < pContext->Data().m_Claim.m_Deadline)
		{
			OpenAccount(pContext);
			return;
		}

		// Refused, lost or too late. Give back whatever the write did (a no-op unless
		// our token is stored), then decide whether to try again.
		AccountSession::Release(pContext->Data().m_Claim, [pContext](bool) { OnReleased(pContext); });
	}

	static void OnReleased(const CContextPtr& pContext)
	{
		if(!Player(pContext))
			return;

		auto& Data = pContext->Data();
		if(time_get() >= Data.m_Claim.m_Deadline)
		{
			Fail(pContext, "Login failed: session reservation expired. Please try again.");
			return;
		}

		if(++Data.m_Attempts >= CLAIM_ATTEMPTS)
		{
			Fail(pContext, "Login failed: another login is in progress.");
			return;
		}

		// The row changed under us: read the owner again and retry the claim.
		AccountSession::ReadRecord(Data.m_Claim.m_AccountID, [pContext](std::optional<AccountSession::SRecord> Owner)
		{
			if(!Player(pContext))
				return;

			// Without a successful read the previous owner is unknown: do not claim blind.
			if(!Owner)
			{
				Fail(pContext, "Login failed: session state could not be read.", "Please try again a bit later.");
				return;
			}

			pContext->Data().m_Claim.m_Previous = *Owner;
			TryClaim(pContext);
		});
	}

	static void OpenAccount(const CContextPtr& pContext)
	{
		auto* pPlayer = Player(pContext);
		auto& Data = pContext->Data();
		const auto& Claim = Data.m_Claim;
		if(!pPlayer || time_get() >= Claim.m_Deadline)
		{
			AccountSession::Release(Claim);
			return;
		}

		if(!pPlayer->GetSharedData().m_GuestLogin.empty() && Data.m_Login != pPlayer->GetSharedData().m_GuestLogin)
			pPlayer->GetSharedData().m_GuestLogin.clear();

		pPlayer->Account()->Init(Claim.m_AccountID, pContext->GetClientID(), Data.m_Login.c_str(), Data.m_Language, Data.m_LoginDate, Data.m_Snapshot);
		pPlayer->Account()->SetSessionToken(Claim.m_Token);
		dbg_msg("Account", "Login of '%s' authorized in %dms (client %d).", Data.m_Login.c_str(),
			static_cast<int>((time_get() - Data.m_Start) * 1000 / time_freq()), pContext->GetClientID());
		Data.m_pAccountManager->BeginSessionTakeover(pPlayer, Claim, Data.m_PinCode.empty() && !pPlayer->IsGuestLogin());
	}
};

void AccountAuth::Start(CAccountManager* pManager, int ClientID, std::string Login, std::string Password, std::string Nickname)
{
	CLogin::Start(pManager, ClientID, std::move(Login), std::move(Password), std::move(Nickname));
}

void AccountAuth::StartProgress(CAccountSharedData& Shared, const std::shared_ptr<void>& pAttempt, bool Registration)
{
	Shared.m_pAuthorizationAttempt = pAttempt;
	Shared.m_AuthIsRegistration = Registration;
	Shared.m_AuthProgressTick = time_get() + AUTH_PROGRESS_FIRST_SECONDS * time_freq();
}

void AccountAuth::TickProgress()
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
