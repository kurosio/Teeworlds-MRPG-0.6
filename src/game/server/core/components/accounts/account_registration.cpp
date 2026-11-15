/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_registration.h"

#include <base/system.h>
#include <game/server/core/components/accounts/account_auth.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>

#include <utility>

namespace
{
	constexpr int PASSWORD_SALT_LENGTH = 24;

	struct SRegistrationPayload
	{
		CAccountManager* m_pAccountManager {};
		int m_InitID {};
		std::string m_Login {};
		std::string m_Password {};
		std::string m_PasswordHash {};
		std::string m_PasswordSalt {};
		std::string m_Nickname {};
		std::string m_RegisteredIP {};
		std::string m_Language {};
	};

	using CContextPtr = std::shared_ptr<DbAsync::CContext<SRegistrationPayload>>;
}

class CRegistration
{
public:
	static void Start(CAccountManager* pManager, int ClientID, std::string Login, std::string Password, std::string Nickname, std::string RegisterIP)
	{
		auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
		auto* pPlayer = pGS ? pGS->GetPlayer(ClientID, false) : nullptr;
		if(!pPlayer || !pPlayer->GetSharedData().m_pAuthorizationAttempt.expired())
			return;

		char aSalt[32] {};
		secure_random_password(aSalt, sizeof(aSalt), PASSWORD_SALT_LENGTH);

		SRegistrationPayload Payload {};
		Payload.m_pAccountManager = pManager;
		Payload.m_Login = std::move(Login);
		Payload.m_Password = std::move(Password);
		Payload.m_PasswordHash = CAccountManager::HashPassword(Payload.m_Password, aSalt);
		Payload.m_PasswordSalt = aSalt;
		Payload.m_Nickname = std::move(Nickname);
		Payload.m_RegisteredIP = std::move(RegisterIP);
		Payload.m_Language = pPlayer->GetLanguage();
		auto pContext = DbAsync::MakeContext<SRegistrationPayload>(ClientID, std::move(Payload));

		AccountAuth::StartProgress(pPlayer->GetSharedData(), pContext, true);

		auto pLookup = Database->Prepare<DB::SELECT>("d.ID, a.Username, a.TimeoutCode",
			"tw_accounts_data d LEFT JOIN tw_accounts a ON a.ID = d.ID",
			"WHERE d.Nick = '{}' LIMIT 1", pContext->Data().m_Nickname);
		pLookup->PrimaryRead();
		pLookup->AtExecute([pContext](ResultPtr pRes) { OnLookup(pContext, pRes); });
	}

private:
	static void Fail(const CContextPtr& pContext, const char* pStage)
	{
		auto& Data = pContext->Data();
		dbg_msg("Account", "Registration of '%s' failed at '%s' (client %d, account id %d).",
			Data.m_Nickname.c_str(), pStage, pContext->GetClientID(), Data.m_InitID);

		// The id is kept even when the player is gone: the cleanup removes exactly this row.
		if(Data.m_InitID > 0)
		{
			Database->Execute<DB::REMOVE>("tw_accounts", "WHERE ID = '{}'", Data.m_InitID);
		}
		else
		{
			Database->Execute<DB::REMOVE>("tw_accounts", "WHERE Username = '{}' AND Password = '{}' AND PasswordSalt = '{}' ORDER BY ID DESC LIMIT 1",
				Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt);
		}

		if(!pContext->GetPlayer())
			return;
		pContext->GS()->Chat(pContext->GetClientID(), "Registration failed due to server error.");
		pContext->GS()->Chat(pContext->GetClientID(), "Please try again a bit later.");
	}

	static void OnLookup(const CContextPtr& pContext, ResultPtr pRes)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		// A failed query says nothing about the nickname: only an answered one may
		// decide whether it is free.
		if(!pRes || !*pRes)
		{
			Fail(pContext, "lookup tw_accounts_data");
			return;
		}

		if(!pRes->next())
		{
			CreateAccount(pContext);
			return;
		}

		const auto& Data = pContext->Data();
		const auto AccountID = pRes->getInt("ID");
		const auto TimeoutCode = pRes->getString("TimeoutCode");
		if(!CAccountManager::IsGuestCredentialForNickname(pRes->getString("Username"), Data.m_Nickname.c_str()))
		{
			Chat(pContext, "This nickname is already linked to another account.");
			Chat(pContext, "Change nickname or contact support for recovery.");
			Chat(pContext, "Discord: \"{~}\".", g_Config.m_SvDiscordInviteLink);
			return;
		}

		if(!TimeoutCode.empty() && (!pPlayer->IsAuthed() || pPlayer->Account()->GetID() != AccountID || !pPlayer->Account()->IsReadyToPlay()))
		{
			Chat(pContext, "This nickname is protected by timeout code.");
			Chat(pContext, "First complete authorization of the previous account.");
			return;
		}

		TakeOverGuestAccount(pContext, AccountID);
	}

	// A guest account has no credentials of its own: the new login replaces them.
	static void TakeOverGuestAccount(const CContextPtr& pContext, int AccountID)
	{
		auto& Data = pContext->Data();
		Database->Execute<DB::UPDATE>([pContext](bool Updated)
		{
			if(!pContext->GetPlayer())
				return;
			if(!Updated)
			{
				dbg_msg("Account", "Registration of '%s' could not update the existing account (client %d).", pContext->Data().m_Nickname.c_str(), pContext->GetClientID());
				Chat(pContext, "Registration update failed.");
				Chat(pContext, "Please try again later.");
				return;
			}

			Chat(pContext, "Registration completed successfully.");
			Chat(pContext, "Remember your login and password.");
			Chat(pContext, "Your nickname is your unique game identifier.");
		}, "tw_accounts", "Username = '{}', Password = '{}', PasswordSalt = '{}' WHERE ID = '{}'",
			Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, AccountID);
	}

	static void CreateAccount(const CContextPtr& pContext)
	{
		const auto& Data = pContext->Data();
		// The id comes from the INSERT: selecting the row back can hit a stale replica.
		Database->ExecuteInsertId<DB::INSERT>([pContext](int64_t InsertID) { OnInsertAccount(pContext, InsertID); },
			"tw_accounts", "(Username, Password, PasswordSalt, RegisterDate, RegisteredIP, Language) VALUES ('{}', '{}', '{}', UTC_TIMESTAMP(), '{}', '{}')",
			Data.m_Login, Data.m_PasswordHash, Data.m_PasswordSalt, Data.m_RegisteredIP, Data.m_Language);
	}

	static void OnInsertAccount(const CContextPtr& pContext, int64_t InsertID)
	{
		pContext->Data().m_InitID = static_cast<int>(InsertID);
		if(InsertID <= 0)
		{
			Fail(pContext, "insert tw_accounts");
			return;
		}

		if(!pContext->GetPlayer())
		{
			Fail(pContext, "player gone after insert tw_accounts");
			return;
		}

		const auto& Data = pContext->Data();
		Database->Execute<DB::INSERT>([pContext](bool Inserted) { OnInsertAccountData(pContext, Inserted); },
			"tw_accounts_data", "(ID, Nick) VALUES ('{}', '{}')", Data.m_InitID, Data.m_Nickname);
	}

	static void OnInsertAccountData(const CContextPtr& pContext, bool Inserted)
	{
		if(!Inserted)
		{
			Fail(pContext, "insert tw_accounts_data");
			return;
		}

		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		pPlayer->GetSharedData().m_pAuthorizationAttempt.reset();
		const auto& Data = pContext->Data();
		dbg_msg("Account", "Registration of '%s' stored the account (client %d, account id %d).",
			Data.m_Nickname.c_str(), pContext->GetClientID(), Data.m_InitID);
		pContext->Server()->UpdateAccountBase(Data.m_InitID, Data.m_Nickname, g_Config.m_SvMinRating);

		// The stored credentials are ours: authorize the new account like any login.
		AccountAuth::Start(Data.m_pAccountManager, pContext->GetClientID(), Data.m_Login, Data.m_Password, Data.m_Nickname);
	}

	static void Chat(const CContextPtr& pContext, const char* pMessage)
	{
		if(auto* pGS = pContext->GS())
			pGS->Chat(pContext->GetClientID(), pMessage);
	}

	template<typename... Ts>
	static void Chat(const CContextPtr& pContext, const char* pMessage, const Ts&... Args)
	{
		if(auto* pGS = pContext->GS())
			pGS->Chat(pContext->GetClientID(), pMessage, Args...);
	}
};

void AccountRegistration::Start(CAccountManager* pManager, int ClientID, std::string Login, std::string Password, std::string Nickname, std::string RegisterIP)
{
	CRegistration::Start(pManager, ClientID, std::move(Login), std::move(Password), std::move(Nickname), std::move(RegisterIP));
}
