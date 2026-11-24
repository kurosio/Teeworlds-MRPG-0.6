/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "account_session.h"

#include <base/system.h>
#include <game/server/gamecontext.h>

namespace AccountSession
{
	namespace
	{
		constexpr int TOKEN_LENGTH = 48;

		// The game level claim prepared for the database layer: the token, the row
		// it is allowed to replace, and the server it comes from.
		AccountDb::SSessionClaim ToDbClaim(const SClaim& Claim)
		{
			AccountDb::SSessionClaim DbClaim;
			DbClaim.m_AccountID = Claim.m_AccountID;
			DbClaim.m_ClientID = Claim.m_ClientID;
			DbClaim.m_ServerID = g_Config.m_SvSyncServerID;
			DbClaim.m_Token = Claim.m_Token;
			DbClaim.m_Previous = Claim.m_Previous;
			return DbClaim;
		}

		// A write that matched a row changed the owner; one that matched nothing
		// lost a race.
		Db::CRequest::CCallback WriteDone(DoneCallback Done)
		{
			return [Done = std::move(Done)](Db::CResultSet&& Result)
			{
				if(Done)
					Done(Result.Changed());
			};
		}
	}

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
		AccountDb::ClaimSession(ToDbClaim(Claim), HEARTBEAT_STALE_SECONDS).Then(WriteDone(std::move(Done)));
	}

	void Activate(const SClaim& Claim, DoneCallback Done)
	{
		AccountDb::ActivateSession(ToDbClaim(Claim)).Then(WriteDone(std::move(Done)));
	}

	void Release(const SClaim& Claim, DoneCallback Done)
	{
		AccountDb::ReleaseSession(ToDbClaim(Claim)).Then(WriteDone(std::move(Done)));
	}

	void Remove(const SKey& Key)
	{
		AccountDb::RemoveSession(Key);
	}

	void RefreshHeartbeats(const std::vector<SKey>& vKeys)
	{
		if(vKeys.empty())
			return;
		AccountDb::RefreshSessionHeartbeats(vKeys);
	}

	void FetchTokens(const std::vector<int>& vAccountIDs, RowsCallback Done)
	{
		if(vAccountIDs.empty())
		{
			Done(true, {});
			return;
		}

		AccountDb::FetchSessionTokens(vAccountIDs).Then([Done = std::move(Done)](Db::CResultSet&& Result)
		{
			// A failed query says nothing about ownership: the caller must not
			// acknowledge a takeover or disconnect any session.
			if(!Result.Ok())
			{
				Done(false, {});
				return;
			}
			Done(true, AccountDb::ParseSessionTokens(Result));
		});
	}
}
