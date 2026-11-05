#include "server_sync.h"

#include <base/format.h>
#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/server/sql_connect_pool.h>

#include <exception>

void CServerSync::Push(const char* pEvent, int WorldID, int Value)
{
	if(g_Config.m_SvSyncServerID <= 0 || !pEvent || !*pEvent || WorldID < 0)
		return;

	Database->Execute<DB::INSERT>(TW_SERVER_SYNC_TABLE, "(ServerID, WorldID, Event, Value, Processed) VALUES ('{}', '{}', '{}', '{}', '')",	g_Config.m_SvSyncServerID, WorldID, pEvent, Value);
}

void CServerSync::OnTick()
{
	if(g_Config.m_SvSyncServerID <= 0 || !m_EventCallback)
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_SERVER_SYNC_TABLE,
		"WHERE ServerID != '{}' AND (Processed IS NULL OR Processed = '' OR FIND_IN_SET('{}', REPLACE(Processed, ' ', '')) = 0) ORDER BY ID ASC LIMIT 100",
		g_Config.m_SvSyncServerID, g_Config.m_SvSyncServerID);
	if(!pRes)
		return;

	while(pRes->next())
	{
		SRequest Req;
		Req.ID = pRes->getInt("ID");
		Req.ServerID = pRes->getInt("ServerID");
		Req.WorldID = pRes->getInt("WorldID");
		Req.Event = pRes->getString("Event");
		Req.Value = pRes->getInt("Value");

		if(Req.ID <= 0 || Req.Event.empty())
			continue;

		if(IsProcessed(Req.ID))
			continue;

		RememberProcessed(Req.ID);

		// A throwing handler must not break the queue or cause the event to be
		// delivered again.
		try
		{
			m_EventCallback(Req.Event, Req);
		}
		catch(const std::exception& e)
		{
			dbg_msg("server_sync", "Event '%s' (request %d) failed: %s", Req.Event.c_str(), Req.ID, e.what());
		}
		catch(...)
		{
			dbg_msg("server_sync", "Event '%s' (request %d) failed with an unknown exception.", Req.Event.c_str(), Req.ID);
		}

		MarkProcessed(Req.ID);
	}
}

void CServerSync::MarkProcessed(int RequestID)
{
	Database->Execute<DB::UPDATE>(TW_SERVER_SYNC_TABLE,
		"Processed = TRIM(BOTH ',' FROM CONCAT(COALESCE(Processed, ''), ',{}')) WHERE ID = '{}' AND (Processed IS NULL OR Processed = '' OR FIND_IN_SET('{}', REPLACE(Processed, ' ', '')) = 0)",
		g_Config.m_SvSyncServerID, RequestID, g_Config.m_SvSyncServerID);
}

void CServerSync::RememberProcessed(int RequestID)
{
	if(m_ProcessedRequests.size() >= MAX_PROCESSED_CACHE)
	{
		while(!m_ProcessedOrder.empty() && m_ProcessedRequests.size() >= MAX_PROCESSED_CACHE)
		{
			m_ProcessedRequests.erase(m_ProcessedOrder.front());
			m_ProcessedOrder.pop_front();
		}
	}

	if(m_ProcessedRequests.insert(RequestID).second)
		m_ProcessedOrder.push_back(RequestID);
}

bool CServerSync::IsProcessed(int RequestID) const
{
	return m_ProcessedRequests.contains(RequestID);
}
