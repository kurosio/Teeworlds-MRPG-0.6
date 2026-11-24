#include "async_file.h"

#include <engine/server/async.h>
#include <engine/storage.h>

#include <base/system.h>

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace
{
	// One slot per file: an older write may still be running while newer content
	// arrives, so the content waits and the running task writes it next. A removal
	// takes the same path, so it cannot overtake content queued before it.
	struct SFileSlot
	{
		std::mutex m_Mutex;
		std::string m_Pending {};
		bool m_HasPending = false;
		bool m_HasRemove = false;
		bool m_Running = false;
	};

	struct STarget
	{
		IStorageEngine* m_pStorage = nullptr;
		std::string m_Filename {};
		int m_Type = IStorageEngine::TYPE_ABSOLUTE;
		// A plain path is opened directly instead of through the storage engine.
		bool m_Plain = false;
	};

	std::mutex g_SlotsMutex;
	std::unordered_map<std::string, std::shared_ptr<SFileSlot>> g_Slots;

	std::string SlotKey(const STarget& Target)
	{
		std::string Key = std::to_string(Target.m_Filename.size()) + ":" + Target.m_Filename + "|";
		Key += Target.m_Plain ? "plain" : "storage";
		Key += "|" + std::to_string(Target.m_Type);
		Key += "|" + std::to_string(reinterpret_cast<std::uintptr_t>(Target.m_pStorage));
		return Key;
	}

	std::shared_ptr<SFileSlot> QueueWrite(const std::string& Key, std::string Content)
	{
		std::lock_guard<std::mutex> SlotsLock(g_SlotsMutex);
		auto& pSlot = g_Slots[Key];
		if(!pSlot)
			pSlot = std::make_shared<SFileSlot>();
		std::lock_guard<std::mutex> SlotLock(pSlot->m_Mutex);
		pSlot->m_Pending = std::move(Content);
		pSlot->m_HasPending = true;
		pSlot->m_HasRemove = false;
		return pSlot;
	}

	std::shared_ptr<SFileSlot> QueueRemove(const std::string& Key)
	{
		std::lock_guard<std::mutex> SlotsLock(g_SlotsMutex);
		auto& pSlot = g_Slots[Key];
		if(!pSlot)
			pSlot = std::make_shared<SFileSlot>();
		std::lock_guard<std::mutex> SlotLock(pSlot->m_Mutex);
		pSlot->m_Pending.clear();
		pSlot->m_HasPending = false;
		pSlot->m_HasRemove = true;
		return pSlot;
	}

	IOHANDLE OpenWrite(const STarget& Target, const char* pFilename)
	{
		return Target.m_Plain ? io_open(pFilename, IOFLAG_WRITE)
						  : Target.m_pStorage->OpenFile(pFilename, IOFLAG_WRITE, Target.m_Type);
	}

	// The new content is written next to the file and renamed over it. A reader
	// that loads the same file at the same time - a world that starts, a login
	// that reads its account - therefore sees the old document or the new one,
	// never a file that is halfway overwritten.
	void WriteNow(const STarget& Target, const std::string& Content)
	{
		const bool Replace = Target.m_Plain || Target.m_Type == IStorageEngine::TYPE_ABSOLUTE;
		const std::string Temp = Target.m_Filename + ".tmp";
		const char* pFilename = Replace ? Temp.c_str() : Target.m_Filename.c_str();

		IOHANDLE File = OpenWrite(Target, pFilename);
		if(!File)
		{
			dbg_msg("AsyncFile", "could not write '%s'", Target.m_Filename.c_str());
			return;
		}

		if(!Content.empty())
			io_write(File, Content.data(), (unsigned)Content.size());
		io_close(File);

		if(Replace && fs_rename(Temp.c_str(), Target.m_Filename.c_str()) != 0)
		{
			dbg_msg("AsyncFile", "could not replace '%s'", Target.m_Filename.c_str());
			fs_remove(Temp.c_str());
		}
	}

	void RemoveNow(const STarget& Target)
	{
		if(Target.m_Plain)
			fs_remove(Target.m_Filename.c_str());
		else
			Target.m_pStorage->RemoveFile(Target.m_Filename.c_str(), Target.m_Type);
	}

	// Starts the drain loop of a slot unless it is already running.
	void EnsureDrain(const STarget& Target, const std::string& Key, const std::shared_ptr<SFileSlot>& pSlot)
	{
		{
			std::lock_guard<std::mutex> Lock(pSlot->m_Mutex);
			if(pSlot->m_Running)
				return;
			pSlot->m_Running = true;
		}

		Async::Run([Target, Key, pSlot]
		{
			while(true)
			{
				std::string Data;
				bool HasRemove = false;
				{
					// Queueing uses the same lock order. Erasing an idle slot here is
					// atomic with a producer finding or creating that slot.
					std::lock_guard<std::mutex> SlotsLock(g_SlotsMutex);
					std::lock_guard<std::mutex> SlotLock(pSlot->m_Mutex);
					if(!pSlot->m_HasPending && !pSlot->m_HasRemove)
					{
						pSlot->m_Running = false;
						const auto It = g_Slots.find(Key);
						if(It != g_Slots.end() && It->second == pSlot)
							g_Slots.erase(It);
						return;
					}

					Data = std::move(pSlot->m_Pending);
					HasRemove = pSlot->m_HasRemove;
					pSlot->m_HasPending = false;
					pSlot->m_HasRemove = false;
				}

				try
				{
					if(HasRemove)
						RemoveNow(Target);
					else
						WriteNow(Target, Data);
				}
				catch(const std::exception& e)
				{
					dbg_msg("AsyncFile", "file operation on '%s' failed: %s", Target.m_Filename.c_str(), e.what());
				}
				catch(...)
				{
					dbg_msg("AsyncFile", "file operation on '%s' failed", Target.m_Filename.c_str());
				}
			}
		});
	}

	STarget MakeTarget(IStorageEngine* pStorage, const char* pFilename, int Type, bool Plain)
	{
		STarget Target;
		Target.m_pStorage = pStorage;
		Target.m_Filename = pFilename ? pFilename : "";
		Target.m_Type = Type;
		Target.m_Plain = Plain;
		return Target;
	}
}

void AsyncFile::Read(IStorageEngine* pStorage, const char* pFilename, int Type, CReadCallback OnLoaded)
{
	if(!pStorage || !pFilename)
	{
		if(OnLoaded)
			OnLoaded(SReadResult {});
		return;
	}

	// The name is copied: the caller's buffer may be gone when the worker starts.
	const STarget Target = MakeTarget(pStorage, pFilename, Type, false);
	auto pResult = std::make_shared<SReadResult>();
	Async::Run([Target, pResult]
	{
		void* pData = nullptr;
		unsigned Size = 0;
		if(!Target.m_pStorage->ReadFile(Target.m_Filename.c_str(), Target.m_Type, &pData, &Size))
		{
			free(pData);
			if(!Target.m_pStorage->FileExists(Target.m_Filename.c_str(), Target.m_Type))
			{
				pResult->m_Status = EReadStatus::NotFound;
				pResult->m_Error = "file not found";
			}
			return;
		}

		if(Size > 0 && !pData)
			return;

		if(pData)
		{
			pResult->m_Content.assign(static_cast<const char*>(pData), Size);
			free(pData);
		}
		pResult->m_Status = EReadStatus::Success;
		pResult->m_Error.clear();
	},
	[pResult, OnLoaded = std::move(OnLoaded)]() mutable
	{
		if(OnLoaded)
			OnLoaded(std::move(*pResult));
	});
}

void AsyncFile::Write(IStorageEngine* pStorage, const char* pFilename, int Type, std::string Content)
{
	if(!pStorage || !pFilename)
		return;

	const STarget Target = MakeTarget(pStorage, pFilename, Type, false);
	const std::string Key = SlotKey(Target);
	auto pSlot = QueueWrite(Key, std::move(Content));
	EnsureDrain(Target, Key, pSlot);
}

void AsyncFile::Remove(IStorageEngine* pStorage, const char* pFilename, int Type)
{
	if(!pStorage || !pFilename)
		return;

	const STarget Target = MakeTarget(pStorage, pFilename, Type, false);
	const std::string Key = SlotKey(Target);
	auto pSlot = QueueRemove(Key);
	EnsureDrain(Target, Key, pSlot);
}

void AsyncFile::Read(const char* pFilename, CReadCallback OnLoaded)
{
	if(!pFilename)
	{
		if(OnLoaded)
			OnLoaded(SReadResult {});
		return;
	}

	const STarget Target = MakeTarget(nullptr, pFilename, IStorageEngine::TYPE_ABSOLUTE, true);
	auto pResult = std::make_shared<SReadResult>();
	Async::Run([Target, pResult]
	{
		IOHANDLE File = io_open(Target.m_Filename.c_str(), IOFLAG_READ);
		if(!File)
		{
			if(!fs_is_file(Target.m_Filename.c_str()))
			{
				pResult->m_Status = EReadStatus::NotFound;
				pResult->m_Error = "file not found";
			}
			return;
		}

		char* pData = io_read_all_str(File);
		io_close(File);
		if(!pData)
			return;

		pResult->m_Content.assign(pData);
		free(pData);
		pResult->m_Status = EReadStatus::Success;
		pResult->m_Error.clear();
	},
	[pResult, OnLoaded = std::move(OnLoaded)]() mutable
	{
		if(OnLoaded)
			OnLoaded(std::move(*pResult));
	});
}

void AsyncFile::Write(const char* pFilename, std::string Content)
{
	if(!pFilename)
		return;

	const STarget Target = MakeTarget(nullptr, pFilename, IStorageEngine::TYPE_ABSOLUTE, true);
	const std::string Key = SlotKey(Target);
	auto pSlot = QueueWrite(Key, std::move(Content));
	EnsureDrain(Target, Key, pSlot);
}

void AsyncFile::Remove(const char* pFilename)
{
	if(!pFilename)
		return;

	const STarget Target = MakeTarget(nullptr, pFilename, IStorageEngine::TYPE_ABSOLUTE, true);
	const std::string Key = SlotKey(Target);
	auto pSlot = QueueRemove(Key);
	EnsureDrain(Target, Key, pSlot);
}
