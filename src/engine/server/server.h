/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H
#include <engine/server.h>

#include <engine/shared/uuid_manager.h>

class CServer : public IServer
{
	class IConsole *m_pConsole;
	class IStorageEngine *m_pStorage;
	class CMultiWorlds* m_pMultiWorlds;
	class CServerBan* m_pServerBan;
	class DiscordJob* m_pDiscord;

public:
	class IGameServer* GameServer(int WorldID = 0) override;
	class IGameServer* GameServerPlayer(int ClientID) override;
	class IConsole *Console() const { return m_pConsole; }
	class IStorageEngine*Storage() const { return m_pStorage; }
	class CMultiWorlds* MultiWorlds() const { return m_pMultiWorlds; }

	enum
	{
		MAX_RCONCMD_RATIO = 8,
		MAX_RCONCMD_SEND=16,
	};

	class CClient
	{
	public:
		enum
		{
			STATE_EMPTY = 0,
			STATE_PREAUTH,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_READY,
			STATE_INGAME,

			SNAPRATE_INIT = 0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER,
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		// names update
		char m_aName[MAX_NAME_LENGTH];
		char m_aNameChangeRequest[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		char m_aLanguage[MAX_LANGUAGE_LENGTH];

		int m_Version;
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthTries;

		char m_aNameTransfersPrefix[MAX_NAME_LENGTH];
		int m_WorldID;
		int m_OldWorldID;
		bool m_IsChangesWorld;

		int m_NextMapChunk;
		bool m_Quitting;
		const IConsole::CCommandInfo *m_pRconCmdToSend;

		int m_ClientVersion;
		void Reset();

		// DDRace
		NETADDR m_Addr;
		bool m_GotDDNetVersionPacket;
		bool m_DDNetVersionSettled;
		int m_DDNetVersion;
		char m_aDDNetVersionStr[64];
		CUuid m_ConnectionID;
	};

	CClient m_aClients[MAX_CLIENTS];
	int m_aIdMap[MAX_CLIENTS * VANILLA_MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;

	int64 m_GameStartTime;
	int m_RunServer;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;
	bool m_HeavyReload;

	// map
	enum
	{
		MAP_CHUNK_SIZE=NET_MAX_PAYLOAD-NET_MAX_CHUNKHEADERSIZE-4, // msg type
	};
	int m_MapChunksPerRequest;
	int m_DataChunksPerRequest;

	int m_RconPasswordSet;
	int m_GeneratedRconPassword;

	CRegister m_Register;

	CServer();
	~CServer() override;

	// world time
	int m_ShiftTime;
	int m_LastShiftTick;
	int m_WorldMinute;
	int m_WorldHour;
	bool m_IsNewMinute;

	int GetMinuteWorldTime() const override;
	int GetHourWorldTime() const override;
	int GetOffsetWorldTime() const override;
	void SetOffsetWorldTime(int Hour) override;
	bool CheckWorldTime(int Hour, int Minute) override;
	const char* GetStringTypeDay() const override;
	int GetEnumTypeDay() const override;

	// basic
	void SetClientName(int ClientID, const char *pName) override;
	void SetClientClan(int ClientID, char const *pClan) override;
	void SetClientCountry(int ClientID, int Country) override;
	void SetClientScore(int ClientID, int Score) override;

	void SetClientNameChangeRequest(int ClientID, const char* pName) override;
	const char* GetClientNameChangeRequest(int ClientID) override;

	bool IsClientChangesWorld(int ClientID) override;
	void ChangeWorld(int ClientID, int NewWorldID) override;
	int GetClientWorldID(int ClientID) override;

	void SetClientLanguage(int ClientID, const char* pLanguage) override;
	const char* GetClientLanguage(int ClientID) const override;
	const char* GetWorldName(int WorldID) override;
	int GetWorldsSize() const override;

	void SendDiscordMessage(const char *pChannel, int Color, const char* pTitle, const char* pText) override;
	void SendDiscordGenerateMessage(const char *pTitle, int AccountID, int Color = 0) override;
	void UpdateDiscordStatus(const char *pStatus) override;

	void Kick(int ClientID, const char *pReason) override;

	int64 TickStartTime(int Tick) const;
	int Init();

	void InitRconPasswordIfUnset();

	void SetRconCID(int ClientID) override;
	int GetRconCID() const override;
	int GetRconAuthLevel() const override;
	int GetAuthedState(int ClientID) const override;
	bool IsAuthed(int ClientID) const override;
	bool IsBanned(int ClientID) override;
	bool IsEmpty(int ClientID) const override;
	void SetClientDDNetVersion(int ClientID, int DDNetVersion) override;
	int GetClientInfo(int ClientID, CClientInfo *pInfo) const override;
	void GetClientAddr(int ClientID, char *pAddrStr, int Size) const override;
	const char *ClientName(int ClientID) const override;
	const char *ClientClan(int ClientID) const override;
	int ClientCountry(int ClientID) const override;
	bool ClientIngame(int ClientID) const override;
	
	int GetClientVersion(int ClientID) const override;
	int SendMsg(CMsgPacker* pMsg, int Flags, int ClientID, int64 Mask = -1, int WorldID = -1) override;

	void DoSnapshot(int WorldID);

	static int NewClientCallback(int ClientID, void* pUser, bool Sixup);
	static int NewClientNoAuthCallback(int ClientID, void* pUser);
	static int DelClientCallback(int ClientID, const char* pReason, void* pUser);
	static int ClientRejoinCallback(int ClientID, void* pUser);

	void SendMapData(int ClientID, int Chunk);
	void SendCapabilities(int ClientID);
	void SendMap(int ClientID);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();

	void ProcessClientPacket(CNetChunk *pPacket);

	class CCache
	{
	public:
		class CCacheChunk
		{
		public:
			CCacheChunk(const void* pData, int Size);
			CCacheChunk(const CCacheChunk&) = delete;

			int m_DataSize;
			unsigned char m_aData[NET_MAX_PAYLOAD];
		};

		std::list<CCacheChunk> m_Cache;

		CCache();
		~CCache();

		void AddChunk(const void* pData, int Size);
		void Clear();
	};
	CCache m_aServerInfoCache[3 * 2];
	bool m_ServerInfoNeedsUpdate;
	int64_t m_ServerInfoFirstRequest;
	int m_ServerInfoNumRequests;

	void ExpireServerInfo() override;
	void CacheServerInfo(CCache* pCache, int Type, bool SendClients);
	void SendServerInfo(const NETADDR* pAddr, int Token, int Type, bool SendClients);
	void GetServerInfoSixup(CPacker* pPacker, int Token, bool SendClients);
	bool RateLimitServerInfoConnless();
	void SendServerInfoConnless(const NETADDR* pAddr, int Token, int Type);
	void UpdateServerInfo(bool Resend = false);

	void PumpNetwork();

	bool LoadMap(int ID);

	void InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, IConsole *pConsole);
	int Run();

	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConReload(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainModCommandUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconPasswordSet(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void RegisterCommands();

	// Bots
	void InitClientBot(int ClientID) override;

	int SnapNewID() override;
	void SnapFreeID(int ID) override;
	void *SnapNewItem(int Type, int ID, int Size) override;
	void SnapSetStaticsize(int ItemType, int Size) override;

	int* GetIdMap(int ClientID) override;
};

#endif