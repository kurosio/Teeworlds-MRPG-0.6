#ifndef ENGINE_SERVER_ASYNC_H
#define ENGINE_SERVER_ASYNC_H

#include <cstddef>
#include <functional>

namespace Async
{
	// Runs work on the generic worker pool, never on a database executor. Done is
	// posted to the server's main-thread pump after Work finishes.
	void Run(std::function<void()> Work, std::function<void()> Done = {});

	// Waits for accepted work to finish before captured resources are destroyed.
	void Drain();

	void PostToMainThread(std::function<void()> Task);
	void RunMainThreadTasks();
	size_t PendingMainThreadTasks();
	void StartMainThreadTasks();
	void StopMainThreadTasks();
}

#endif // ENGINE_SERVER_ASYNC_H
