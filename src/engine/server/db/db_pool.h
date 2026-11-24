#ifndef ENGINE_SERVER_DB_DB_POOL_H
#define ENGINE_SERVER_DB_DB_POOL_H

// Connection pool of one database endpoint.
//
//  * the configured number of connections is a hard limit, however many threads
//    wait;
//  * a connection is owned by one RAII lease, which returns it on every path;
//  * idle connections are pinged before reuse and dropped when dead, so a
//    MariaDB restart does not leave broken connections behind;
//  * Acquire() is bounded by a timeout; Shutdown() refuses new leases, waits for
//    the active ones and is safe from any thread. A lease that outlives the pool
//    still closes its connection.

#include "db_driver.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Db
{
	struct SPoolConfig
	{
		SEndpoint m_Endpoint;
		// Connections this pool may hold at the same time (idle + busy).
		int m_MaxConnections = 2;
		// How long Acquire() waits for a free connection before it gives up.
		int m_AcquireTimeoutMs = 5000;
		// Maximum time spent opening one connection, including the driver's
		// connection-creation lock. It is further capped by Acquire()'s deadline.
		int m_ConnectTimeoutMs = 5000;
		// Base delay after a failed connection attempt. It grows exponentially
		// and is capped by m_ConnectRetryMaxDelayMs so repeated failures do not
		// hammer an unavailable server or permanently stop recovery.
		int m_ConnectRetryDelayMs = 500;
		int m_ConnectRetryMaxDelayMs = 30000;
		// Idle connections older than this are pinged before reuse. 0 disables
		// the ping.
		int m_IdlePingMs = 30000;
	};

	class CDbPool
	{
	private:
		struct CState;

	public:
		// A connection borrowed from the pool. Move-only; releasing happens in
		// the destructor, so no path can leak a connection or return it twice.
		class CLease
		{
		public:
			CLease() = default;
			~CLease();
			CLease(const CLease&) = delete;
			CLease& operator=(const CLease&) = delete;
			CLease(CLease&& Other) noexcept;
			CLease& operator=(CLease&& Other) noexcept;

			IDbConnection* Get() const { return m_pConnection.get(); }
			IDbConnection* operator->() const { return m_pConnection.get(); }
			IDbConnection& operator*() const { return *m_pConnection; }
			explicit operator bool() const { return m_pConnection != nullptr; }

			// Returns the connection now; the lease is empty afterwards.
			void Release();
			// Drops the connection instead of returning it to the pool; use it when
			// the connection is known to be broken.
			void Discard();

		private:
			friend class CDbPool;
			CLease(std::shared_ptr<CState> pState, std::unique_ptr<IDbConnection> pConnection)
				: m_pState(std::move(pState)), m_pConnection(std::move(pConnection))
			{
			}

			static void ReturnToPool(const std::shared_ptr<CState>& pState, std::unique_ptr<IDbConnection> pConnection, bool Usable);

			std::shared_ptr<CState> m_pState;
			std::unique_ptr<IDbConnection> m_pConnection;
		};

		struct SStats
		{
			int m_Open = 0; // idle + busy
			int m_Idle = 0;
			int m_Busy = 0;
			uint64_t m_Created = 0;
			uint64_t m_Reused = 0;
			uint64_t m_Discarded = 0;
			uint64_t m_Failed = 0;
			uint64_t m_AcquireTimeouts = 0;
		};

		CDbPool(IDbDriver& Driver, SPoolConfig Config);
		~CDbPool();

		CDbPool(const CDbPool&) = delete;
		CDbPool& operator=(const CDbPool&) = delete;

		// Borrows a connection. Empty on timeout, on a failed connect or after
		// Shutdown(). TimeoutMs < 0 uses the configured acquire timeout.
		std::optional<CLease> Acquire(int TimeoutMs = -1);

		// Closes idle connections and refuses new leases. Active leases may still
		// finish; the call waits up to WaitMs for them (0 = do not wait).
		void Shutdown(int WaitMs = 0);

		bool IsShutdown() const;
		const SPoolConfig& Config() const { return m_Config; }
		SStats Stats() const;

	private:
		IDbDriver& m_Driver;
		SPoolConfig m_Config;
		std::shared_ptr<CState> m_pState;
	};
}

#endif // ENGINE_SERVER_DB_DB_POOL_H
