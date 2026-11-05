#ifndef ENGINE_SERVER_SERVER_SYNC_H
#define ENGINE_SERVER_SERVER_SYNC_H

#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <unordered_set>

#define TW_SERVER_SYNC_TABLE "tw_server_sync"

class CServerSync
{
	static constexpr size_t MAX_PROCESSED_CACHE = 8192;

public:
	static CServerSync* GetInstance()
	{
		static CServerSync instance;
		return &instance;
	}

	CServerSync(const CServerSync&) = delete;
	CServerSync& operator=(const CServerSync&) = delete;

	struct SRequest
	{
		int ID {};
		int ServerID {};
		int WorldID {};
		std::string Event {};
		int Value {};
	};
	using EventCallback = std::function<void(const std::string& EventName, const SRequest&)>;

	void SetEventCallback(EventCallback Fn) { m_EventCallback = std::move(Fn); }
	void Push(const char* pEvent, int WorldID, int Value);
	void OnTick();

private:
	CServerSync() = default;

	void MarkProcessed(int RequestID);
	void RememberProcessed(int RequestID);
	bool IsProcessed(int RequestID) const;

	EventCallback m_EventCallback;
	std::unordered_set<int> m_ProcessedRequests;
	std::deque<int> m_ProcessedOrder;
};

#define Sync CServerSync::GetInstance()

#endif
