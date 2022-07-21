/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_BAN_H
#define ENGINE_SERVER_SERVER_BAN_H
#include <engine/shared/netban.h>

class CServerBan : public CNetBan
{
	class IServer* m_pServer;
	class CNetServer* m_pNetServer;

	template<class T> int BanExt(T* pBanPool, const typename T::CDataType* pData, int Seconds, const char* pReason);

public:
	class IServer* Server() const { return m_pServer; }

	void InitServerBan(class IConsole* pConsole, class IStorageEngine* pStorage, class IServer* pServer, class CNetServer* pNetServer);

	virtual int BanAddr(const NETADDR* pAddr, int Seconds, const char* pReason);
	virtual int BanRange(const CNetRange* pRange, int Seconds, const char* pReason);

	static void ConBanExt(class IConsole::IResult* pResult, void* pUser);
};


#endif
