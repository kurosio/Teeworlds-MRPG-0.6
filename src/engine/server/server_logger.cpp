#include "server_logger.h"

#include "server.h"

constexpr size_t MAX_PENDING_MSG_SHIRK = 300;
std::atomic_bool grequest_update {false};

CServerLogger::CServerLogger(CServer *pServer) :
	m_pServer(pServer),
	m_MainThread(std::this_thread::get_id())
{
	dbg_assert(pServer != nullptr, "server pointer must not be null");
	m_vPending.reserve(MAX_PENDING_MSG_SHIRK);
}

void CServerLogger::Update()
{
	if(grequest_update.load())
	{
		log_info("logger", "relocation request from stores future log");

		m_PendingLock.lock();
		m_vPending.clear();
		m_vPending.shrink_to_fit();
		m_vPending.reserve(MAX_PENDING_MSG_SHIRK);
		m_PendingLock.unlock();

		grequest_update.store(false);
	}
}

void CServerLogger::Log(const CLogMessage *pMessage)
{
	if(m_Filter.Filters(pMessage))
	{
		return;
	}

	m_PendingLock.lock();
	if(m_MainThread == std::this_thread::get_id())
	{
		if(!m_vPending.empty())
		{
			if(m_pServer)
			{
				for(const auto &Message : m_vPending)
				{
					m_pServer->SendLogLine(&Message);
				}
			}
			m_vPending.clear();
		}
		m_PendingLock.unlock();

		if(m_pServer)
		{
			m_pServer->SendLogLine(pMessage);
		}
	}
	else
	{
		if(m_vPending.capacity() > MAX_PENDING_MSG_SHIRK)
			grequest_update.store(true);

		m_vPending.push_back(*pMessage);
		m_PendingLock.unlock();
	}
}

void CServerLogger::OnServerDeletion()
{
	dbg_assert(m_MainThread == std::this_thread::get_id(), "CServerLogger::OnServerDeletion not called from the main thread");
	m_pServer = nullptr;
}
