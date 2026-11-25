/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_SESSION_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_SESSION_H

#include "account_db.h"

#include <engine/server/server_sync.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>

class CPlayer;

// Who may play an account, stored in tw_account_sessions.
//
// The row holds the token of the one session allowed to play. Every change is a
// compare-and-swap: it names the token it expects and matches nothing if that
// token is no longer there. A decision made on an old read is therefore refused
// by the database instead of silently overwriting a newer owner.
//
// The login pipeline (account_auth.cpp) reads the owner, claims the row and
// then hands the claim to BeginTakeover. From there this module drives the
// rest of the login: it ends the previous owner's sessions (here and on other
// servers), waits for the account load and activates the session. The account
// manager calls Tick, HandleSyncEvent and OnClientReset for it.
namespace AccountSession
{
	constexpr int CLAIM_SECONDS = 30; // time limit of one login, from its start
	constexpr int HEARTBEAT_STALE_SECONDS = 45; // older heartbeat (or reservation) counts as dead

	// One session: the account and the token it logged in with.
	using SKey = AccountDb::SSessionKey;
	// The row as it was read.
	using SRecord = AccountDb::SSessionRecord;
	// A login's attempt to take an account: the new token, the row it replaces
	// and the deadline of the whole login.
	using SClaim = AccountDb::SSessionClaim;

	using ReadCallback = std::function<void(std::optional<SRecord>)>; // nullopt: the read failed
	using DoneCallback = std::function<void(bool)>; // true: the write matched a row

	// --- used by the login pipeline -----------------------------------------
	std::string GenerateToken();

	// Reads the current owner from the remote database, where the row arbitrates
	// between servers. This is only a snapshot; TryClaim checks it again.
	void ReadRecord(int AccountID, ReadCallback Done);

	// Takes the account if the row still holds the token we read and it may be taken
	// (active, or its reservation is stale). A refusal leaves the row untouched.
	void TryClaim(const SClaim& Claim, DoneCallback Done);

	// Gives the claim back: restores the previous row, or removes ours. Only matches
	// our own unfinished reservation, so it is a no-op if the claim never landed.
	void Release(const SClaim& Claim, DoneCallback Done = {});

	// --- driven by the account manager component -----------------------------
	// Continues a login whose claim on the account has been written: ends the
	// previous owner's sessions, loads the account and activates the session.
	// pAttempt keeps the authorization guard valid through takeover and load.
	void BeginTakeover(CPlayer* pPlayer, const SClaim& Claim, bool RecommendPin, std::shared_ptr<void> pAttempt);

	// Completes waiting logins, gives up timed out ones and refreshes the
	// heartbeats of the local sessions. Called once per server tick.
	void Tick();

	// Session takeover requests and acknowledgements of other servers.
	void HandleSyncEvent(const std::string& EventName, const CServerSync::SRequest& Req);

	// A client disconnected: gives back its unfinished claims and removes its
	// active session row.
	void OnClientReset(int ClientID);
}

#endif
