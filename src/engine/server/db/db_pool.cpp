#include "db_pool.h"

#include <base/system.h>

#include <algorithm>
#include <exception>

namespace Db
{
	struct CDbPool::CState
	{
		struct CIdleConnection
		{
			std::unique_ptr<IDbConnection> m_pConnection;
			int64_t m_LastUsed = 0;
		};

		mutable std::mutex m_Mutex;
		std::condition_variable m_Cv;
		std::vector<CIdleConnection> m_vIdle;
		// Connections that exist (idle + busy) or are being opened by an Acquire.
		int m_Open = 0;
		int m_Pending = 0;
		int m_Busy = 0;
		bool m_Shutdown = false;
		bool m_ConnectInProgress = false;
		// First tick of the current connect back-off window; 0 when there is none.
		int64_t m_ConnectBlockedUntil = 0;
		unsigned m_ConnectFailures = 0;
		struct SCounters
		{
			uint64_t m_Created = 0;
			uint64_t m_Reused = 0;
			uint64_t m_Discarded = 0;
			uint64_t m_Failed = 0;
			uint64_t m_AcquireTimeouts = 0;
		};
		SCounters m_Counters;

		bool CanConnect(const SPoolConfig& Config) const
		{
			return m_Open + m_Pending < Config.m_MaxConnections;
		}
	};

	namespace
	{
		void CloseNoThrow(IDbConnection* pConnection) noexcept
		{
			if(!pConnection)
				return;
			try
			{
				pConnection->Close();
			}
			catch(...)
			{
			}
		}

		long long NowMs()
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		}
	}

	CDbPool::CDbPool(IDbDriver& Driver, SPoolConfig Config)
		: m_Driver(Driver), m_Config(std::move(Config)), m_pState(std::make_shared<CState>())
	{
		m_Config.m_MaxConnections = std::max(1, m_Config.m_MaxConnections);
		m_Config.m_AcquireTimeoutMs = std::max(1, m_Config.m_AcquireTimeoutMs);
		m_Config.m_ConnectTimeoutMs = std::max(1, m_Config.m_ConnectTimeoutMs);
		m_Config.m_ConnectRetryDelayMs = std::max(0, m_Config.m_ConnectRetryDelayMs);
		m_Config.m_ConnectRetryMaxDelayMs = std::max(m_Config.m_ConnectRetryDelayMs, m_Config.m_ConnectRetryMaxDelayMs);
		m_Config.m_IdlePingMs = std::max(0, m_Config.m_IdlePingMs);
	}

	CDbPool::~CDbPool()
	{
		Shutdown(static_cast<int>(m_Config.m_AcquireTimeoutMs));
	}

	std::optional<CDbPool::CLease> CDbPool::Acquire(int TimeoutMs)
	{
		auto pState = m_pState;
		const long long Deadline = NowMs() + (TimeoutMs < 0 ? m_Config.m_AcquireTimeoutMs : TimeoutMs);

		while(true)
		{
			std::unique_ptr<IDbConnection> pConnection;
			int64_t LastUsed = 0;
			bool NeedConnect = false;

			{
				std::unique_lock<std::mutex> Lock(pState->m_Mutex);
				if(pState->m_Shutdown)
					return std::nullopt;
				if(NowMs() >= Deadline)
				{
					pState->m_Counters.m_AcquireTimeouts++;
					return std::nullopt;
				}

				// 1. Reuse an idle connection (most recently used first).
				if(!pState->m_vIdle.empty())
				{
					pConnection = std::move(pState->m_vIdle.back().m_pConnection);
					LastUsed = pState->m_vIdle.back().m_LastUsed;
					pState->m_vIdle.pop_back();
					pState->m_Busy++;
					pState->m_Counters.m_Reused++;
				}
				// 2. Open a new one, if the limit allows it.
				else if(pState->CanConnect(m_Config))
				{
					pState->m_Pending++;
					pState->m_Busy++;
					NeedConnect = true;
				}
			}

			if(pConnection)
			{
				// Liveness check outside the lock: it is a round trip. A connection
				// that sat idle long enough to have been dropped by the server gets
				// pinged before it is handed out; one that is still fresh does not.
				bool Usable = false;
				try
				{
					const bool NeedsPing = m_Config.m_IdlePingMs > 0 && NowMs() - LastUsed >= m_Config.m_IdlePingMs;
					Usable = NeedsPing ? pConnection->Ping() : pConnection->IsUsable();
				}
				catch(...)
				{
					Usable = false;
				}

				if(Usable)
				{
					// Shutdown may have started while IsUsable()/Ping() was running.
					// Make the handoff atomic with that state change so no new lease is
					// returned after Shutdown() has closed admission.
					std::lock_guard<std::mutex> Lock(pState->m_Mutex);
					if(!pState->m_Shutdown)
						return CLease(pState, std::move(pConnection));
				}

				// Dead connection (or shutdown raced the liveness check): drop it,
				// the loop continues with a fresh one only if admission is still open.
				if(DebugLoggingEnabled())
					dbg_msg("SQL Debug", "[%s] discarded an idle connection after liveness check (idle=%lldms)",
						m_Config.m_Endpoint.m_Name.c_str(), NowMs() - LastUsed);
				CloseNoThrow(pConnection.get());
				std::lock_guard<std::mutex> Lock(pState->m_Mutex);
				pState->m_Open = std::max(0, pState->m_Open - 1);
				pState->m_Busy = std::max(0, pState->m_Busy - 1);
				pState->m_Counters.m_Discarded++;
				pState->m_Cv.notify_all();
				continue;
			}

			if(NeedConnect)
			{
				// Several workers can be waiting for a connection at once. Recheck the
				// shared back-off after every wakeup: a short polling wait would let all
				// of them reconnect before the configured delay had elapsed.
				bool ReuseIdle = false;
				while(true)
				{
					std::unique_lock<std::mutex> Lock(pState->m_Mutex);
					if(pState->m_Shutdown)
					{
						pState->m_Pending--;
						pState->m_Busy--;
						pState->m_Cv.notify_all();
						return std::nullopt;
					}

					const long long Now = NowMs();
					if(Now >= Deadline)
					{
						pState->m_Pending--;
						pState->m_Busy--;
						pState->m_Counters.m_AcquireTimeouts++;
						pState->m_Cv.notify_all();
						return std::nullopt;
					}

					// A busy connection may have returned while this worker was
					// waiting to open another one. Prefer it over waiting for a
					// failed-connect backoff or the serialized connector.
					if (!pState->m_vIdle.empty())
					{
						pState->m_Pending--;
						pState->m_Busy--;
						ReuseIdle = true;
						pState->m_Cv.notify_all();
						break;
					}

					if(pState->m_ConnectInProgress)
					{
						pState->m_Cv.wait_for(Lock, std::chrono::milliseconds(Deadline - Now),
							[pState] { return pState->m_Shutdown || !pState->m_vIdle.empty() || !pState->m_ConnectInProgress; });
						continue;
					}

					const int64_t WaitUntil = pState->m_ConnectBlockedUntil;
					if(WaitUntil > Now)
					{
						pState->m_Cv.wait_for(Lock, std::chrono::milliseconds(std::min<long long>(WaitUntil - Now, Deadline - Now)),
							[pState, WaitUntil] { return pState->m_Shutdown || !pState->m_vIdle.empty() || pState->m_ConnectBlockedUntil != WaitUntil; });
						continue;
					}
					pState->m_ConnectInProgress = true;
					break;
				}

				if(ReuseIdle)
					continue;

				const long long ConnectStartedMs = NowMs();
				const long long Remaining = std::max<long long>(1, Deadline - ConnectStartedMs);
				const int ConnectTimeoutMs = static_cast<int>(std::min<long long>(Remaining, m_Config.m_ConnectTimeoutMs));
				if(DebugLoggingEnabled())
					dbg_msg("SQL Debug", "[%s] opening connection to %s (database=%s timeout=%dms)", m_Config.m_Endpoint.m_Name.c_str(),
						m_Config.m_Endpoint.Address().c_str(), m_Config.m_Endpoint.m_Database.c_str(), ConnectTimeoutMs);
				SError ConnectError;
				try
				{
					pConnection = m_Driver.Connect(m_Config.m_Endpoint, ConnectTimeoutMs, ConnectError);
				}
				catch(const std::exception&)
				{
					ConnectError = SError { 0, "", "database connection failed" };
				}
				catch(...)
				{
					ConnectError = SError { 0, "", "database connection failed" };
				}
				if(!pConnection && !ConnectError)
					ConnectError = SError { 0, "", "database connection failed" };
				if(pConnection)
				{
					bool Usable = false;
					try
					{
						Usable = pConnection->IsUsable();
					}
					catch(...)
					{
					}
					if(!Usable)
					{
						CloseNoThrow(pConnection.get());
						pConnection.reset();
						ConnectError = SError { 0, "", "new database connection was not usable" };
					}
				}
				if(DebugLoggingEnabled())
				dbg_msg("SQL Debug", "[%s] connection attempt %s after %lldms (host=%s database=%s)", m_Config.m_Endpoint.m_Name.c_str(),
					pConnection ? "succeeded" : "failed", NowMs() - ConnectStartedMs,
					m_Config.m_Endpoint.m_Host.c_str(), m_Config.m_Endpoint.m_Database.c_str());

				bool CloseNewConnection = false;
				{
					std::lock_guard<std::mutex> Lock(pState->m_Mutex);
					pState->m_ConnectInProgress = false;
					pState->m_Pending--;
					if(pConnection)
					{
						pState->m_Open++;
						pState->m_Counters.m_Created++;
						pState->m_ConnectBlockedUntil = 0;
						pState->m_ConnectFailures = 0;
						if(pState->m_Shutdown || NowMs() >= Deadline)
						{
							// Do not hand out a connection opened after shutdown or after
							// the requesting operation's acquisition deadline.
							pState->m_Counters.m_Discarded++;
							if(!pState->m_Shutdown)
								pState->m_Counters.m_AcquireTimeouts++;
							CloseNewConnection = true;
						}
						else
						{
							pState->m_Cv.notify_all();
							return CLease(pState, std::move(pConnection));
						}
					}
					else
					{
						pState->m_Busy--;
						pState->m_Counters.m_Failed++;
						if(NowMs() >= Deadline)
							pState->m_Counters.m_AcquireTimeouts++;
						if(pState->m_ConnectFailures < 32)
							++pState->m_ConnectFailures;
						if(ConnectError)
							dbg_msg("SQL", "[%s] connection unavailable (code=%d sqlstate=%s auth-error=%d server-unavailable=%d connect-elapsed=%lldms connect-budget=%dms); credentials and driver message suppressed",
								m_Config.m_Endpoint.m_Name.c_str(), ConnectError.m_Code,
								ConnectError.m_SqlState.empty() ? "-" : ConnectError.m_SqlState.c_str(),
								IsAuthenticationError(ConnectError.m_Code, ConnectError.m_SqlState),
								IsServerUnavailableError(ConnectError.m_Code, ConnectError.m_SqlState), NowMs() - ConnectStartedMs, ConnectTimeoutMs);
						if(m_Config.m_ConnectRetryDelayMs > 0)
						{
							long long BackoffMs = m_Config.m_ConnectRetryDelayMs;
						for(unsigned Attempt = 1; Attempt < pState->m_ConnectFailures && BackoffMs < m_Config.m_ConnectRetryMaxDelayMs; ++Attempt)
							BackoffMs = std::min<long long>(m_Config.m_ConnectRetryMaxDelayMs, BackoffMs * 2);
						pState->m_ConnectBlockedUntil = NowMs() + std::min<long long>(BackoffMs, m_Config.m_ConnectRetryMaxDelayMs);
					}
					}
					pState->m_Cv.notify_all();
				}

					if(CloseNewConnection)
					{
						// Keep the reserved slot counted until the late connection is closed.
						CloseNoThrow(pConnection.get());
					std::lock_guard<std::mutex> Lock(pState->m_Mutex);
					pState->m_Open = std::max(0, pState->m_Open - 1);
					pState->m_Busy = std::max(0, pState->m_Busy - 1);
					pState->m_Cv.notify_all();
				}
				if(m_Config.m_ConnectRetryDelayMs <= 0 || NowMs() >= Deadline)
					return std::nullopt;
				continue;
			}

			// 3. Wait for a connection to come back.
			const long long Now = NowMs();
			if(Now >= Deadline)
			{
				std::lock_guard<std::mutex> Lock(pState->m_Mutex);
				pState->m_Counters.m_AcquireTimeouts++;
				return std::nullopt;
			}

			std::unique_lock<std::mutex> Lock(pState->m_Mutex);
			// A release can happen between the earlier capacity check and this
			// wait. Recheck the condition under the lock to avoid a lost wakeup.
			pState->m_Cv.wait_for(Lock, std::chrono::milliseconds(std::max<long long>(1, Deadline - Now)),
				[&] { return pState->m_Shutdown || !pState->m_vIdle.empty() || pState->CanConnect(m_Config); });
		}
	}

	void CDbPool::CLease::Release()
	{
		if(!m_pConnection)
			return;
		auto pState = std::move(m_pState);
		auto pConnection = std::move(m_pConnection);
		bool Usable = false;
		try
		{
			Usable = pConnection->IsUsable();
		}
		catch(...)
		{
			Usable = false;
		}
		ReturnToPool(pState, std::move(pConnection), Usable);
	}

	void CDbPool::CLease::Discard()
	{
		if(!m_pConnection)
			return;
		auto pState = std::move(m_pState);
		auto pConnection = std::move(m_pConnection);
		ReturnToPool(pState, std::move(pConnection), false);
	}

	void CDbPool::CLease::ReturnToPool(const std::shared_ptr<CState>& pState, std::unique_ptr<IDbConnection> pConnection, bool Usable)
	{
		// The lease may outlive the pool object itself: the shared state keeps the
		// accounting alive, the connection is closed instead of leaking.
		bool CloseConnection = false;
		{
			std::lock_guard<std::mutex> Lock(pState->m_Mutex);
			bool Keep = Usable && !pState->m_Shutdown && pConnection;
			if(Keep)
			{
				try
				{
					pState->m_vIdle.emplace_back();
					pState->m_vIdle.back().m_LastUsed = NowMs();
					pState->m_vIdle.back().m_pConnection = std::move(pConnection);
				}
				catch(...)
				{
					// A failed idle-list allocation must not escape a lease destructor.
					Keep = false;
				}
			}

			CloseConnection = !Keep && pConnection != nullptr;
			if(CloseConnection)
				pState->m_Counters.m_Discarded++;
			else
				pState->m_Busy = std::max(0, pState->m_Busy - 1);
			pState->m_Cv.notify_all();
		}
		if(CloseConnection)
		{
			// Keep the slot counted until the network handle is actually closed;
			// otherwise another Acquire could exceed the configured physical cap.
			CloseNoThrow(pConnection.get());
			std::lock_guard<std::mutex> Lock(pState->m_Mutex);
			pState->m_Open = std::max(0, pState->m_Open - 1);
			pState->m_Busy = std::max(0, pState->m_Busy - 1);
			pState->m_Cv.notify_all();
		}
	}

	CDbPool::CLease::~CLease()
	{
		Release();
	}

	CDbPool::CLease::CLease(CLease&& Other) noexcept
		: m_pState(std::move(Other.m_pState)), m_pConnection(std::move(Other.m_pConnection))
	{
	}

	CDbPool::CLease& CDbPool::CLease::operator=(CLease&& Other) noexcept
	{
		if(this != &Other)
		{
			Release();
			m_pState = std::move(Other.m_pState);
			m_pConnection = std::move(Other.m_pConnection);
		}
		return *this;
	}

	void CDbPool::Shutdown(int WaitMs)
	{
		auto pState = m_pState;
		std::vector<CState::CIdleConnection> vIdle;
		{
			std::unique_lock<std::mutex> Lock(pState->m_Mutex);
			pState->m_Shutdown = true;
			pState->m_Cv.notify_all();

			if(WaitMs > 0)
			{
				pState->m_Cv.wait_for(Lock, std::chrono::milliseconds(WaitMs), [&pState] { return pState->m_Busy == 0; });
			}
			vIdle.swap(pState->m_vIdle);
			pState->m_Open -= static_cast<int>(vIdle.size());
		}

		for(auto& Idle : vIdle)
			CloseNoThrow(Idle.m_pConnection.get());
	}

	bool CDbPool::IsShutdown() const
	{
		std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
		return m_pState->m_Shutdown;
	}

	CDbPool::SStats CDbPool::Stats() const
	{
		std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
		SStats Stats;
		Stats.m_Open = m_pState->m_Open;
		Stats.m_Idle = static_cast<int>(m_pState->m_vIdle.size());
		Stats.m_Busy = m_pState->m_Busy;
		Stats.m_Created = m_pState->m_Counters.m_Created;
		Stats.m_Reused = m_pState->m_Counters.m_Reused;
		Stats.m_Discarded = m_pState->m_Counters.m_Discarded;
		Stats.m_Failed = m_pState->m_Counters.m_Failed;
		Stats.m_AcquireTimeouts = m_pState->m_Counters.m_AcquireTimeouts;
		return Stats;
	}
}
