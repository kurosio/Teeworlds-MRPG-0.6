/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H

#include <engine/input_events.h>
#include <engine/shared/world_detail.h>

// When recording a demo on the server, the ClientID -1 is used
enum
{
	SERVER_DEMO_CLIENT = -1
};

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	virtual class IGameServer* GameServer(int WorldID = 0) const = 0;
	virtual class IGameServer* GameServerPlayer(int ClientID) const = 0;
	virtual class CLocalization* Localization() const = 0;
	virtual class IInputEvents* Input() const = 0;

	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
		bool m_GotDDNetVersion;
		int m_DDNetVersion;
		const char *m_pDDNetVersionStr;
		const CUuid *m_pConnectionID;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual const char *ClientName(int ClientID) const = 0;
	virtual const char *ClientClan(int ClientID) const = 0;
	virtual const char* ClientContinent(int ClientID) const = 0;
	virtual const char* ClientCountryIsoCode(int ClientID) const = 0;
	virtual int ClientCountry(int ClientID) const = 0;
	virtual bool ClientIngame(int ClientID) const = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) const = 0;
	virtual int GetClientLatency(int ClientID) const = 0;
	virtual void SetClientDDNetVersion(int ClientID, int DDNetVersion) = 0;
	virtual void GetClientAddr(int ClientID, char* pAddrStr, int Size) const = 0;
	virtual void SetStateClientMRPG(int ClientID, bool State) = 0;
	virtual bool GetStateClientMRPG(int ClientID) const = 0;
	virtual int GetClientVersion(int ClientID) const = 0;
	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID, int64_t Mask = -1, int WorldID = -1) = 0;
	virtual int SendMotd(int ClientID, const char* pText) = 0;
	virtual void SetSpectatorID(int ClientID, int SpectatorID) = 0;
	virtual int GetSpectatorID(int ClientID) const = 0;

	bool Translate(int& Target, int Client)
	{
		if(Target < MAX_PLAYERS || Client >= MAX_PLAYERS)
			return true;

		int* pMap = GetIdMap(Client);
		for(int i = MAX_PLAYERS; i < VANILLA_MAX_CLIENTS; i++)
		{
			if(Target == pMap[i])
			{
				Target = i;
				return true;
			}
		}

		return false;
	}

	bool ReverseTranslate(int& Target, int Client)
	{
		if(Target < MAX_PLAYERS || Client >= MAX_PLAYERS)
			return true;

		Target = clamp(Target, 0, VANILLA_MAX_CLIENTS - 1);
		int* pMap = GetIdMap(Client);
		if(pMap[Target] == -1)
			return false;
		Target = pMap[Target];
		return true;
	}

	template<class T>
	int SendPackMsg(const T* pMsg, int Flags, int ClientID, int64_t Mask = -1, int WorldID = -1)
	{
		int Result = 0;

		if(ClientID == -1)
		{
			for(int i = 0; i < MAX_PLAYERS; i++)
			{
				if(ClientIngame(i))
					Result = SendPackMsgTranslate(pMsg, Flags, i, Mask, WorldID);
			}
		}
		else
		{
			Result = SendPackMsgTranslate(pMsg, Flags, ClientID, Mask, WorldID);
		}

		return Result;
	}

	template<class T>
	int SendPackMsgOne(const T* pMsg, int Flags, int ClientID, int64_t Mask, int WorldID)
	{
		dbg_assert(ClientID != -1, "SendPackMsgOne called with -1");
		CMsgPacker Packer(pMsg->ms_MsgId, false);
		if(pMsg->Pack(&Packer))
			return -1;

		return SendMsg(&Packer, Flags, ClientID, Mask, WorldID);
	}

	template<class T>
	int SendPackMsgTranslate(const T* pMsg, int Flags, int ClientID, int64_t Mask, int WorldID)
	{
		return SendPackMsgOne(pMsg, Flags, ClientID, Mask, WorldID);
	}

	int SendPackMsgTranslate(const CNetMsg_Sv_Emoticon* pMsg, int Flags, int ClientID, int64_t Mask, int WorldID)
	{
		CNetMsg_Sv_Emoticon MsgCopy;
		mem_copy(&MsgCopy, pMsg, sizeof(MsgCopy));

		return Translate(MsgCopy.m_ClientId, ClientID) && SendPackMsgOne(&MsgCopy, Flags, ClientID, Mask, WorldID);
	}

	int SendPackMsgTranslate(const CNetMsg_Sv_Chat* pMsg, int Flags, int ClientID, int64_t Mask, int WorldID)
	{
		CNetMsg_Sv_Chat MsgCopy;
		mem_copy(&MsgCopy, pMsg, sizeof(MsgCopy));

		char aBuf[1000];
		if(MsgCopy.m_ClientId >= 0 && !Translate(MsgCopy.m_ClientId, ClientID))
		{
			str_format(aBuf, sizeof(aBuf), "%s: %s", ClientName(MsgCopy.m_ClientId), MsgCopy.m_pMessage);
			MsgCopy.m_pMessage = aBuf;
			MsgCopy.m_ClientId = VANILLA_MAX_CLIENTS - 1;
		}

		return SendPackMsgOne(&MsgCopy, Flags, ClientID, Mask, WorldID);
	}

	// translate KillMsg
	int SendPackMsgTranslate(const CNetMsg_Sv_KillMsg* pMsg, int Flags, int ClientID, int64_t Mask, int WorldID)
	{
		CNetMsg_Sv_KillMsg MsgCopy;
		mem_copy(&MsgCopy, pMsg, sizeof(MsgCopy));
		if(!Translate(MsgCopy.m_Victim, ClientID))
			return 0;
		if(!Translate(MsgCopy.m_Killer, ClientID))
			MsgCopy.m_Killer = MsgCopy.m_Victim;

		return SendPackMsgOne(&MsgCopy, Flags, ClientID, Mask, WorldID);
	}

	// World Time
	virtual int GetMinuteGameTime() const = 0;
	virtual int GetHourGameTime() const = 0;
	virtual int GetOffsetGameTime() const = 0;
	virtual void SetOffsetGameTime(int Hour) = 0;
	virtual const char* GetStringTypeday() const = 0;
	virtual int GetCurrentTypeday() const = 0;

	// main client functions
	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;

	virtual bool IsClientChangingWorld(int ClientID) = 0;
	virtual void ChangeWorld(int ClientID, int NewWorldID) = 0;
	virtual int GetClientWorldID(int ClientID) const = 0;
	virtual const char* GetWorldName(int WorldID) = 0;
	virtual CWorldDetail* GetWorldDetail(int WorldID) = 0;
	virtual bool IsWorldType(int WorldID, WorldType Type) const = 0;
	virtual int GetWorldsSize() const = 0;
	virtual int GetClientsCountByWorld(int WorldID) const = 0;

	virtual const char* Localize(int ClientID, const char* pText) = 0;
	virtual void SetClientLanguage(int ClientID, const char* pLanguage) = 0;
	virtual const char* GetClientLanguage(int ClientID) const = 0;

	// Bots
	virtual void InitClientBot(int ClientID) = 0;

	// snapshots
	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;
	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	template<typename T>
	T* SnapNewItem(int Id) { return static_cast<T*>(SnapNewItem(T::ms_MsgId, Id, sizeof(T))); }

	enum
	{
		RCON_CID_SERV=-1,
		RCON_CID_VOTE=-2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual int GetRconCID() const = 0;
	virtual int GetRconAuthLevel() const = 0;
	virtual int GetAuthedState(int ClientID) const = 0;
	virtual bool IsAuthed(int ClientID) const = 0;
	virtual bool IsBanned(int ClientID) = 0;
	virtual bool IsEmpty(int ClientID) const = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;

	virtual int* GetIdMap(int ClientID) = 0;

	virtual void UpdateAccountBase(int UID, std::string Nickname, int Rating) = 0;
	virtual const char* GetAccountNickname(int AccountID) = 0;
	virtual int GetAccountRank(int AccountID) = 0;

	virtual void ExpireServerInfo() = 0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	virtual void OnInit(int WorldID) = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown() = 0;

	virtual void OnTick() = 0;
	virtual void OnTickGlobal() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnDaytypeChange(int NewDaytype) = 0;
	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;
	virtual void OnClearClientData(int ClientID) = 0;
	virtual void OnClientPrepareChangeWorld(int ClientID) = 0;

	virtual void OnClientConnected(int ClientID) = 0;
	virtual void OnClientEnter(int ClientID, bool FirstEnter) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual void* GetLastInput(int ClientID) const = 0;
	virtual bool IsClientCharacterExist(int ClientID) const = 0;
	virtual bool IsClientReady(int ClientID) const = 0;
	virtual bool IsClientPlayer(int ClientID) const = 0;
	virtual bool PlayerExists(int ClientID) const = 0;

	virtual const char *Version() const = 0;
	virtual const char *NetVersion() const = 0;

	virtual void OnUpdateClientServerInfo(nlohmann::json* pJson, int ClientID) = 0;
};

namespace Instance
{
	struct Data { inline static class IServer* g_pServer; };
	static inline IServer* Server() { return Data::g_pServer; }
	static inline IGameServer* GameServerPlayer(int ClientID = -1) { return Data::g_pServer->GameServerPlayer(ClientID); }
	static inline IGameServer* GameServer(int WorldID = INITIALIZER_WORLD_ID) { return Data::g_pServer->GameServer(WorldID); }
	static inline const char* Localize(int ClientID, const char* pText) { return Data::g_pServer->Localize(ClientID, pText); }
};

extern IGameServer *CreateGameServer();
#endif