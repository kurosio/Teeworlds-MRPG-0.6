/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_session.h"
#include "account_manager.h"

#include <base/system.h>
#include <game/server/gamecontext.h>
#include <game/server/core/tools/db_async_context.h>

#include <algorithm>
#include <vector>

namespace
{
	constexpr const char* ACCOUNT_SESSION_TAKEOVER_REASON = "Account session was taken over";
	constexpr int ACCOUNT_SESSION_TAKEOVER_TIMEOUT_SECONDS = 10;
	constexpr int TOKEN_LENGTH = 48;

	// A session logged in on this server.
	struct SLocalSession
	{
		int m_ClientID {};
		AccountSession::SKey m_Key {};
	};

	// A login waiting for the previous owner of the account to let go.
	struct SPendingLogin
	{
		int m_ClientID {};
		AccountSession::SClaim m_Claim {};
		bool m_RecommendPin {};
		int m_EventID {}; // takeover request to the previous owner's server, 0 if none
		bool m_RelayFailed {};
		bool m_RemoteAck {};
		int64_t m_StartTime {};
		std::shared_ptr<void> m_pAttempt {}; // keeps the weak authorization token valid
	};

	enum class EPendingState
	{
		Wait, // keep waiting
		Ready, // complete the login
		Failed, // reject the login, keep the previous owner
		Abandoned, // the player is gone without OnClientReset
	};

	// A login whose account load has been started: the answer of that load is the
	// last thing a login waits for, so it is watched like the pending stage.
	struct SLoadingLogin
	{
		int m_ClientID {};
		AccountSession::SClaim m_Claim {};
		int64_t m_StartedAt {};
	};

	// The logins in flight on this server. A client is in exactly one world, so
	// one list serves all worlds; the initializer world's manager drives it.
	std::vector<SPendingLogin> gs_vPendingLogins;
	std::vector<SLoadingLogin> gs_vLoadingLogins;
	int64_t gs_NextSessionCheck = 0;

	CGS* GetGameServer(int ClientID)
	{
		return static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
	}

	// A write that matched a row changed the owner; one that matched nothing
	// lost a race.
	Db::CRequest::CCallback WriteDone(AccountSession::DoneCallback Done)
	{
		return [Done = std::move(Done)](Db::CResultSet&& Result)
		{
			if(Done)
				Done(Result.Changed());
		};
	}

	// A previous owner on another server has to end its session before we play.
	bool NeedsRemoteTakeover(const AccountSession::SClaim& Claim)
	{
		return Claim.m_Previous.m_ServerID > 0 && Claim.m_Previous.m_ServerID != g_Config.m_SvSyncServerID;
	}

	std::vector<SLocalSession> CollectLocalSessions(int AccountID = 0)
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

	bool HasOtherLocalSession(int AccountID, const std::string& Token)
	{
		for(const auto& [ClientID, Data] : CAccountData::ms_aData)
		{
			if(Data.GetID() == AccountID && !Data.GetSessionToken().empty() && Data.GetSessionToken() != Token)
				return true;
		}
		return false;
	}

	void KickLocalSession(const SLocalSession& Session)
	{
		// Re-check: an earlier kick may already have changed this slot.
		const auto It = CAccountData::ms_aData.find(Session.m_ClientID);
		if(It == CAccountData::ms_aData.end() || It->second.GetID() != Session.m_Key.m_AccountID || It->second.GetSessionToken() != Session.m_Key.m_Token)
			return;

		auto* pGS = GetGameServer(Session.m_ClientID);
		if(!pGS || !pGS->GetPlayer(Session.m_ClientID, false))
			return;

		// Stop the old session from acting during its final disconnect tick.
		It->second.SetReadyToPlay(false);
		pGS->Server()->Kick(Session.m_ClientID, ACCOUNT_SESSION_TAKEOVER_REASON);
	}

	void KickOtherLocalSessions(int AccountID, const std::string& Token)
	{
		for(const auto& Session : CollectLocalSessions(AccountID))
		{
			if(Session.m_Key.m_Token != Token)
				KickLocalSession(Session);
		}
	}

	// The rows are the truth: a session whose token is no longer stored in the
	// database was taken over and has to end.
	void VerifyAgainstRows(const std::vector<SLocalSession>& vSessions, const std::vector<AccountSession::SKey>& vRows, const std::function<void()>& OnVerified)
	{
		for(const auto& Session : vSessions)
		{
			const auto It = std::find_if(vRows.begin(), vRows.end(),
				[&Session](const AccountSession::SKey& Row) { return Row.m_AccountID == Session.m_Key.m_AccountID; });
			if(It != vRows.end() && !It->m_Token.empty() && It->m_Token != Session.m_Key.m_Token)
				KickLocalSession(Session);
		}

		if(OnVerified)
			OnVerified();
	}

	void VerifySessions(std::vector<SLocalSession> vSessions, std::function<void()> OnVerified = {})
	{
		// Nothing to verify is a successful check, not a skipped one: a takeover
		// request must be acknowledged even when this server holds no session.
		if(vSessions.empty())
		{
			VerifyAgainstRows({}, {}, OnVerified);
			return;
		}

		std::vector<int> vAccountIDs;
		for(const auto& Session : vSessions)
			vAccountIDs.push_back(Session.m_Key.m_AccountID);

		AccountDb::FetchSessionTokens(vAccountIDs).Then([vSessions = std::move(vSessions), OnVerified](Db::CResultSet&& Result)
		{
			// A failed query says nothing about ownership: the caller must not
			// acknowledge a takeover or disconnect any session.
			if(!Result.Ok())
			{
				dbg_msg("Account", "could not verify local sessions against the database");
				return;
			}

			VerifyAgainstRows(vSessions, AccountDb::ParseSessionTokens(Result), OnVerified);
		});
	}

	void FailPendingLogin(const SPendingLogin& Pending)
	{
		AccountSession::Release(Pending.m_Claim);

		auto* pGS = GetGameServer(Pending.m_ClientID);
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

	// Forgets the load of a client and answers how long it took, 0 if it was not
	// watched (already finished, reset or given up).
	int64_t AckLoadingLogin(int ClientID)
	{
		const int64_t Now = time_get();
		for(auto It = gs_vLoadingLogins.begin(); It != gs_vLoadingLogins.end(); ++It)
		{
			if(It->m_ClientID != ClientID)
				continue;

			const int64_t Ms = (Now - It->m_StartedAt) * 1000 / time_freq();
			gs_vLoadingLogins.erase(It);
			return Ms;
		}
		return 0;
	}

	// Gives a claim back and ends the connection that waited for it.
	void AbortClaim(int ClientID, const AccountSession::SClaim& Claim)
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
		if(auto* pGS = GetGameServer(ClientID))
			pGS->Chat(ClientID, "Login failed: the account did not finish loading.");
		Instance::Server()->Kick(ClientID, "Account session reservation expired");
	}

	bool CompleteLogin(int ClientID, const AccountSession::SClaim& Claim, bool RecommendPin, const std::shared_ptr<void>& pAttempt)
	{
		auto* pGS = GetGameServer(ClientID);
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

		// The load is watched until it answers: the login fails with a message when
		// the deadline passes instead of waiting for an answer that is not coming.
		gs_vLoadingLogins.push_back({ ClientID, Claim, time_get() });

		// pAttempt is captured only to stay alive: the login (and its progress
		// notices) ends when this last step has answered, not before.
		const bool Started = pGS->Core()->AccountManager()->LoadAccount(pPlayer, true, Claim.m_Deadline,
			[pContext, Claim, RecommendPin, pAttempt](bool LoadSuccess)
			{
				const int64_t LoadMs = AckLoadingLogin(pContext->GetClientID());
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
					AbortClaim(pContext->GetClientID(), Claim);
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

				// Turns the reservation into an active session (heartbeat set).
				AccountDb::ActivateSession(Claim).Then([pContext, Token = Claim.m_Token](Db::CResultSet&& Result)
				{
					auto* pCurrentPlayer = pContext->GetPlayer();
					if(!Result.Changed() && pCurrentPlayer && pCurrentPlayer->Account()->GetSessionToken() == Token)
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

	EPendingState EvaluatePendingLogin(const SPendingLogin& Pending, int64_t Now)
	{
		auto* pGS = GetGameServer(Pending.m_ClientID);
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

	// A login ends in three stages: the pending one waits for the previous owner of
	// the account, this one waits for the account itself. Both are watched so a
	// stage that never answers cannot keep a session reservation indefinitely.
	void SweepLoadingLogins()
	{
		const int64_t Now = time_get();
		for(auto It = gs_vLoadingLogins.begin(); It != gs_vLoadingLogins.end();)
		{
			auto* pGS = GetGameServer(It->m_ClientID);
			auto* pPlayer = pGS ? pGS->GetPlayer(It->m_ClientID, false) : nullptr;
			if(!pPlayer)
			{
				// nobody is left to be told: only the reservation has to go back
				AccountSession::Release(It->m_Claim);
				It = gs_vLoadingLogins.erase(It);
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
			It = gs_vLoadingLogins.erase(It);

			dbg_msg("Account", "the account of client %d did not finish loading in %lld seconds: the login is given up",
				ClientID, (long long)ElapsedS);
			AbortClaim(ClientID, Claim);
		}
	}

	void ProcessPendingLogins()
	{
		const int64_t Now = time_get();
		std::vector<SPendingLogin> vReady;
		std::vector<SPendingLogin> vFailed;
		for(auto It = gs_vPendingLogins.begin(); It != gs_vPendingLogins.end();)
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
			It = gs_vPendingLogins.erase(It);
		}

		// Act only after the list is final: these calls may disconnect players.
		for(const auto& Pending : vFailed)
			FailPendingLogin(Pending);

		for(const auto& Pending : vReady)
		{
			if(CompleteLogin(Pending.m_ClientID, Pending.m_Claim, Pending.m_RecommendPin, Pending.m_pAttempt))
				continue;

			AccountSession::Release(Pending.m_Claim);
			const auto It = CAccountData::ms_aData.find(Pending.m_ClientID);
			if(It != CAccountData::ms_aData.end() && It->second.GetSessionToken() == Pending.m_Claim.m_Token)
				Instance::Server()->Kick(Pending.m_ClientID, "Account session reservation expired");
		}
	}
}

namespace AccountSession
{
	std::string GenerateToken()
	{
		char aToken[TOKEN_LENGTH + 1] {};
		secure_random_password(aToken, sizeof(aToken), TOKEN_LENGTH);
		return std::string(aToken);
	}

	void ReadRecord(int AccountID, ReadCallback Done)
	{
		AccountDb::ReadSession(AccountID).Then([Done = std::move(Done)](Db::CResultSet&& Result)
		{
			// "No row" is a valid answer; a failed query is not. Only the former may
			// lead to a claim of a free account.
			if(!Result.Ok())
			{
				Done(std::nullopt);
				return;
			}
			Done(AccountDb::ParseSession(Result, HEARTBEAT_STALE_SECONDS));
		});
	}

	void TryClaim(const SClaim& Claim, DoneCallback Done)
	{
		AccountDb::ClaimSession(Claim, HEARTBEAT_STALE_SECONDS).Then(WriteDone(std::move(Done)));
	}

	void Release(const SClaim& Claim, DoneCallback Done)
	{
		AccountDb::ReleaseSession(Claim).Then(WriteDone(std::move(Done)));
	}

	void BeginTakeover(CPlayer* pPlayer, const SClaim& Claim, bool RecommendPin, std::shared_ptr<void> pAttempt)
	{
		if(!pPlayer || Claim.m_Token.empty() || pPlayer->Account()->GetID() != Claim.m_AccountID)
		{
			// The reservation was written for a login that cannot continue: left
			// behind, it would refuse the next attempt of this account until the
			// heartbeat counts as stale.
			Release(Claim);
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
		Pending.m_pAttempt = std::move(pAttempt);
		if(NeedsRemoteTakeover(Claim))
		{
			// The other server ends its session and answers with an ack carrying this event ID.
			Pending.m_EventID = Sync->Push(syncevents::ACCOUNT_SESSION_TAKEOVER, pPlayer->GS()->GetWorldID(), Claim.m_AccountID);
			Pending.m_RelayFailed = Pending.m_EventID <= 0;
		}
		gs_vPendingLogins.push_back(Pending);

		pPlayer->GS()->Chat(Pending.m_ClientID, "Verifying account session...");
	}

	void Tick()
	{
		ProcessPendingLogins();
		SweepLoadingLogins();

		const int RefreshSeconds = g_Config.m_SvAccountSessionRefresh;
		if(RefreshSeconds <= 0)
			return;

		const int64_t Now = time_get();
		if(Now < gs_NextSessionCheck)
			return;
		gs_NextSessionCheck = Now + RefreshSeconds * time_freq();

		const auto vSessions = CollectLocalSessions();
		std::vector<SKey> vKeys;
		vKeys.reserve(vSessions.size());
		for(const auto& Session : vSessions)
			vKeys.push_back(Session.m_Key);

		AccountDb::RefreshSessionHeartbeats(vKeys);
		VerifySessions(vSessions);
	}

	void HandleSyncEvent(const std::string& EventName, const CServerSync::SRequest& Req)
	{
		const std::string AckEvent = std::string(syncevents::ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX) + std::to_string(g_Config.m_SvSyncServerID);
		if(EventName == AckEvent)
		{
			if(Req.Value <= 0)
				return;

			for(auto& Pending : gs_vPendingLogins)
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

	void OnClientReset(int ClientID)
	{
		// A login that never completed gives its reservation back.
		for(const auto& Pending : gs_vPendingLogins)
		{
			if(Pending.m_ClientID == ClientID)
				Release(Pending.m_Claim);
		}
		std::erase_if(gs_vPendingLogins, [ClientID](const SPendingLogin& Pending) { return Pending.m_ClientID == ClientID; });

		// The same for a login that already started to load its account.
		for(const auto& Loading : gs_vLoadingLogins)
		{
			if(Loading.m_ClientID == ClientID)
				Release(Loading.m_Claim);
		}
		std::erase_if(gs_vLoadingLogins, [ClientID](const SLoadingLogin& Loading) { return Loading.m_ClientID == ClientID; });

		// Only the row that still carries our token is removed; a newer owner's row stays.
		const auto It = CAccountData::ms_aData.find(ClientID);
		if(It != CAccountData::ms_aData.end() && It->second.GetID() > 0 && !It->second.GetSessionToken().empty() && It->second.IsReadyToPlay())
			AccountDb::RemoveSession({ It->second.GetID(), It->second.GetSessionToken() });
	}
}
