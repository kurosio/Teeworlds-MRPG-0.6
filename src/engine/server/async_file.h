#ifndef ENGINE_SERVER_ASYNC_FILE_H
#define ENGINE_SERVER_ASYNC_FILE_H

// Whole-file I/O that never runs on the game thread.
//
// Accepted work runs on the generic worker pool (see async.h), independent of
// database connections. Writes and removals of the same file cannot overtake one
// another, so the file ends with the last queued content. Read answers carry
// content plus success, not-found, or failure status and run on the game thread.

#include <functional>
#include <string>

class IStorageEngine;

namespace AsyncFile
{
	enum class EReadStatus
	{
		Success,
		NotFound,
		Failed,
	};

	struct SReadResult
	{
		EReadStatus m_Status = EReadStatus::Failed;
		std::string m_Content {};
		std::string m_Error = "read failed";

		bool Ok() const { return m_Status == EReadStatus::Success; }
		bool Missing() const { return m_Status == EReadStatus::NotFound; }
	};

	using CReadCallback = std::function<void(SReadResult&&)>;

	void Read(IStorageEngine* pStorage, const char* pFilename, int Type, CReadCallback OnLoaded);
	void Write(IStorageEngine* pStorage, const char* pFilename, int Type, std::string Content);
	// Removes the file in the background, after any write queued before it.
	void Remove(IStorageEngine* pStorage, const char* pFilename, int Type);

	// The same for plain paths (no storage engine involved).
	void Read(const char* pFilename, CReadCallback OnLoaded);
	void Write(const char* pFilename, std::string Content);
	void Remove(const char* pFilename);
}

#endif // ENGINE_SERVER_ASYNC_FILE_H
