#include <engine/console.h>
#include "server_ban.h"

#include <engine/server.h>
#include <engine/shared/network.h>

void CServerBan::InitServerBan(IConsole* pConsole, IStorageEngine* pStorage, IServer* pServer, CNetServer* pNetServer)
{
	CNetBan::Init(pConsole, pStorage);

	m_pServer = pServer;
	m_pNetServer = pNetServer;

	// overwrites base command, todo: improve this
	Console()->Register("ban", "s[id|ip|range] ?i[minutes] r[reason]", CFGFLAG_SERVER | CFGFLAG_STORE, ConBanExt, this, "Ban player with IP/IP range/client id for x minutes for any reason");
}

template<class T>
int CServerBan::BanExt(T* pBanPool, const typename T::CDataType* pData, int Seconds, const char* pReason)
{
	// validate address
	int RconCID = Server()->GetRconCID();
	if(!Server()->IsEmpty(RconCID))
	{
		if(NetMatch(pData, m_pNetServer->ClientAddr(RconCID)))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (you can't ban yourself)");
			return -1;
		}

		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			if(i == RconCID || Server()->IsEmpty(i))
				continue;

			if(Server()->GetAuthedState(i) >= Server()->GetRconAuthLevel() && NetMatch(pData, m_pNetServer->ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}
	else if(RconCID == IServer::RCON_CID_VOTE)
	{
		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			if(Server()->IsEmpty(i))
				continue;

			if(Server()->GetAuthedState(i) != AUTHED_NO && NetMatch(pData, m_pNetServer->ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}

	int Result = Ban(pBanPool, pData, Seconds, pReason);
	if(Result != 0)
		return Result;

	// drop banned clients
	typename T::CDataType Data = *pData;
	for(int i = 0; i < MAX_PLAYERS; ++i)
	{
		if(Server()->IsEmpty(i))
			continue;

		if(NetMatch(&Data, m_pNetServer->ClientAddr(i)))
		{
			CNetHash NetHash(&Data);
			char aBuf[256];
			MakeBanInfo(pBanPool->Find(&Data, &NetHash), aBuf, sizeof(aBuf), MSGTYPE_PLAYER);
			m_pNetServer->Drop(i, aBuf);
		}
	}

	return Result;
}

int CServerBan::BanAddr(const NETADDR* pAddr, int Seconds, const char* pReason)
{
	return BanExt(&m_BanAddrPool, pAddr, Seconds, pReason);
}

int CServerBan::BanRange(const CNetRange* pRange, int Seconds, const char* pReason)
{
	if(pRange->IsValid())
		return BanExt(&m_BanRangePool, pRange, Seconds, pReason);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban failed (invalid range)");
	return -1;
}

void CServerBan::ConBanExt(IConsole::IResult* pResult, void* pUser)
{
	CServerBan* pThis = static_cast<CServerBan*>(pUser);

	const char* pStr = pResult->GetString(0);
	const int Minutes = pResult->NumArguments() > 1 ? clamp(pResult->GetInteger(1), 0, 44640) : 30;
	const char* pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "No reason given";

	if(str_isallnum(pStr))
	{
		const int ClientID = str_toint(pStr);
		if(pThis->Server()->IsEmpty(ClientID))
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid client id)");
		else
			pThis->BanAddr(pThis->m_pNetServer->ClientAddr(ClientID), Minutes * 60, pReason);
	}
	else
		ConBan(pResult, pUser);
}

