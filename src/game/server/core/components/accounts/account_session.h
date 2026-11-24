/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_SESSION_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_SESSION_H

#include "account_db.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Who may play an account, stored in tw_account_sessions.
//
// The row holds the token of the one session allowed to play. Every change is a
// compare-and-swap: it names the token it expects and matches nothing if that
// token is no longer there. A decision made on an old read is therefore refused
// by the database instead of silently overwriting a newer owner.
namespace AccountSession
{
	constexpr int CLAIM_SECONDS = 30; // time limit of one login, from its start
	constexpr int HEARTBEAT_STALE_SECONDS = 45; // older heartbeat (or reservation) counts as dead

	// One session: the account and the token it logged in with.
	using SKey = AccountDb::SSessionKey;
	// The row as it was read.
	using SRecord = AccountDb::SSessionRecord;

	// A login's attempt to take an account: the new token and the row it replaces.
	struct SClaim
	{
		int m_AccountID {};
		int m_ClientID {};
		std::string m_Token {};
		int64_t m_Deadline {}; // time_get() limit for the whole login
		SRecord m_Previous {};
	};

	using ReadCallback = std::function<void(std::optional<SRecord>)>; // nullopt: the read failed
	using DoneCallback = std::function<void(bool)>; // true: the write matched a row
	using RowsCallback = std::function<void(bool, const std::vector<SKey>&)>; // success, then rows

	std::string GenerateToken();

	// Reads the current owner from the remote database, where the row arbitrates
	// between servers. This is only a snapshot; TryClaim checks it again.
	void ReadRecord(int AccountID, ReadCallback Done);

	// Takes the account if the row still holds the token we read and it may be taken
	// (active, or its reservation is stale). A refusal leaves the row untouched.
	void TryClaim(const SClaim& Claim, DoneCallback Done);

	// Turns the reservation into an active session (heartbeat set).
	void Activate(const SClaim& Claim, DoneCallback Done = {});

	// Gives the claim back: restores the previous row, or removes ours. Only matches
	// our own unfinished reservation, so it is a no-op if the claim never landed.
	void Release(const SClaim& Claim, DoneCallback Done = {});

	// Removes an active session on disconnect; matches only that token.
	void Remove(const SKey& Key);

	// Refreshes the heartbeat of the given active sessions, in batches.
	void RefreshHeartbeats(const std::vector<SKey>& vKeys);

	// Tokens currently stored for the given accounts in the remote database. The
	// callback reports success separately so a failed read is not mistaken for no rows.
	void FetchTokens(const std::vector<int>& vAccountIDs, RowsCallback Done);
}

#endif
