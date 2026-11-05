#include "server_sync.h"

#include <base/format.h>
#include <engine/shared/config_variables.h>
#include <engine/server/sql_connect_pool.h>

void CServerSync::Push(const char* pEvent, int WorldID, int Value)
{
	Database->Execute<DB::INSERT>(TW_SERVER_SYNC_TABLE, "(ServerID, WorldID, Event, Value) VALUES ('{}', '{}', '{}', '{}')", g_Config.m_SvSyncServerID, WorldID, pEvent, Value);
}

void CServerSync::OnTick()
{
	if(g_Config.m_SvSyncServerID <= 0 || !m_EventCallback)
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_SERVER_SYNC_TABLE,
		"WHERE ServerID != '{}' AND (Processed = '' OR FIND_IN_SET('{}', Processed) = 0) ORDER BY ID ASC LIMIT 100", g_Config.m_SvSyncServerID, g_Config.m_SvSyncServerID);
	while(pRes->next())
	{
		SRequest Req;
		Req.ID = pRes->getInt("ID");
		Req.ServerID = pRes->getInt("ServerID");
		Req.WorldID = pRes->getInt("WorldID");
		Req.Event = pRes->getString("Event");
		Req.Value = pRes->getInt("Value");

		m_EventCallback(Req.Event, Req);
		MarkProcessed(Req.ID);
	}
}

void CServerSync::MarkProcessed(int RequestID)
{
	Database->Execute<DB::UPDATE>(TW_SERVER_SYNC_TABLE, "SET Processed = TRIM(BOTH ',' FROM CONCAT(Processed, ',{}')) WHERE ID = '{}'", g_Config.m_SvSyncServerID, RequestID);
}
