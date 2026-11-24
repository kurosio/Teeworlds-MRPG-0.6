#include "async.h"

#include <base/system.h>

#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace Async
{
	namespace
	{
		// Completions are applied only from the server's main-thread tick.
		std::mutex s_MainThreadMutex;
		std::queue<std::function<void()>> s_MainThreadTasks;
		bool s_MainThreadStopped = false;

		class CBackgroundPool
		{
		public:
			CBackgroundPool()
			{
				try
				{
					for(int i = 0; i < 2; ++i)
						m_vWorkers.emplace_back([this] { Worker(); });
				}
				catch(...)
				{
					{
						std::lock_guard<std::mutex> Lock(m_Mutex);
						m_Stopping = true;
					}
					m_Cv.notify_all();
					for(auto& WorkerThread : m_vWorkers)
						if(WorkerThread.joinable())
							WorkerThread.join();
					throw;
				}
			}

			~CBackgroundPool()
			{
				{
					std::lock_guard<std::mutex> Lock(m_Mutex);
					m_Stopping = true;
				}
				m_Cv.notify_all();
				for(auto& WorkerThread : m_vWorkers)
					if(WorkerThread.joinable())
						WorkerThread.join();
			}

			void Submit(std::function<void()> Work, std::function<void()> Done)
			{
				{
					std::lock_guard<std::mutex> Lock(m_Mutex);
					m_qTasks.emplace(std::move(Work), std::move(Done));
				}
				m_Cv.notify_one();
			}

			void Drain()
			{
				std::unique_lock<std::mutex> Lock(m_Mutex);
				m_IdleCv.wait(Lock, [this] { return m_qTasks.empty() && m_Active == 0; });
			}

		private:
			void Worker()
			{
				for(;;)
				{
					std::pair<std::function<void()>, std::function<void()>> Task;
					{
					std::unique_lock<std::mutex> Lock(m_Mutex);
					m_Cv.wait(Lock, [this] { return m_Stopping || !m_qTasks.empty(); });
					if(m_Stopping && m_qTasks.empty())
						return;
					Task = std::move(m_qTasks.front());
					m_qTasks.pop();
					++m_Active;
				}

				try
				{
					if(Task.first)
						Task.first();
				}
				catch(const std::exception& e)
				{
					dbg_msg("Async", "A background job failed: %s", e.what());
				}
				catch(...)
				{
					dbg_msg("Async", "A background job failed");
				}

				if(Task.second)
				{
					try
					{
						PostToMainThread(std::move(Task.second));
					}
					catch(const std::exception& e)
					{
						dbg_msg("Async", "Could not queue a background completion: %s", e.what());
					}
					catch(...)
					{
						dbg_msg("Async", "Could not queue a background completion");
					}
				}

				{
					std::lock_guard<std::mutex> Lock(m_Mutex);
					--m_Active;
				}
				m_IdleCv.notify_all();
				}
			}

			std::mutex m_Mutex;
			std::condition_variable m_Cv;
			std::condition_variable m_IdleCv;
			std::queue<std::pair<std::function<void()>, std::function<void()>>> m_qTasks;
			std::vector<std::thread> m_vWorkers;
			size_t m_Active = 0;
			bool m_Stopping = false;
		};

		CBackgroundPool& BackgroundPool()
		{
			static CBackgroundPool Pool;
			return Pool;
		}
	}

	void Run(std::function<void()> Work, std::function<void()> Done)
	{
		if(!Work)
		{
			if(Done)
				PostToMainThread(std::move(Done));
			return;
		}
		BackgroundPool().Submit(std::move(Work), std::move(Done));
	}

	void Drain()
	{
		BackgroundPool().Drain();
	}

	void PostToMainThread(std::function<void()> Task)
	{
		std::lock_guard<std::mutex> Lock(s_MainThreadMutex);
		if(!s_MainThreadStopped)
			s_MainThreadTasks.push(std::move(Task));
	}

	void RunMainThreadTasks()
	{
		std::queue<std::function<void()>> Tasks;
		{
			std::lock_guard<std::mutex> Lock(s_MainThreadMutex);
			Tasks.swap(s_MainThreadTasks);
		}

		while(!Tasks.empty())
		{
			try
			{
				Tasks.front()();
			}
			catch(const std::exception& e)
			{
				dbg_msg("Async Callback", "A main-thread completion failed: %s", e.what());
			}
			catch(...)
			{
				dbg_msg("Async Callback", "A main-thread completion failed");
			}
			Tasks.pop();
		}
	}

	size_t PendingMainThreadTasks()
	{
		std::lock_guard<std::mutex> Lock(s_MainThreadMutex);
		return s_MainThreadTasks.size();
	}

	void StartMainThreadTasks()
	{
		std::lock_guard<std::mutex> Lock(s_MainThreadMutex);
		s_MainThreadStopped = false;
	}

	void StopMainThreadTasks()
	{
		std::lock_guard<std::mutex> Lock(s_MainThreadMutex);
		s_MainThreadStopped = true;
		std::queue<std::function<void()>> Empty;
		s_MainThreadTasks.swap(Empty);
	}
}
