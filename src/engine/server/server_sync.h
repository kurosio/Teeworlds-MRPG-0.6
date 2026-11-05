#ifndef ENGINE_SERVER_SERVER_SYNC_H
#define ENGINE_SERVER_SERVER_SYNC_H

#include <functional>
#include <string>

#define TW_SERVER_SYNC_TABLE "tw_server_sync"

class CServerSync
{
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

	void Init(int ServerID) { m_ServerID = ServerID; }
	int GetServerID() const { return m_ServerID; }
	void SetEventCallback(EventCallback Fn) { m_EventCallback = std::move(Fn); }
	void Push(const char* pEvent, int WorldID, int Value);
	void OnTick();

private:
	CServerSync() = default;

	void MarkProcessed(int RequestID);

	int m_ServerID {};
	EventCallback m_EventCallback;
};

#define Sync CServerSync::GetInstance()

#endif
