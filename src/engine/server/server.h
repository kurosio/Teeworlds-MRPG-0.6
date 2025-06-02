/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <engine/console.h>
#include <engine/server.h>

#include <engine/shared/econ.h>
#include <engine/shared/http.h>
#include <engine/shared/network.h>
#include <engine/shared/protocol.h>
#include <engine/shared/snapshot.h>
#include <engine/shared/uuid_manager.h>

#include "cache.h"
#include "snapshot_ids_pool.h"

class CServer : public IServer
{
	enum
	{
		UNINITIALIZED = 0,
		RUNNING = 1,
		STOPPING = 2
	};

	class IConsole* m_pConsole {};
	class IStorageEngine* m_pStorage {};
	class CMultiWorlds* m_pMultiWorlds;
	class CServerBan* m_pServerBan;
	class IRegister* m_pRegister{};

public:
	class IGameServer* GameServer(int WorldID = 0) const override;
	class IGameServer* GameServerPlayer(int ClientID) const override;
	class IConsole* Console() const { return m_pConsole; }
	class IStorageEngine* Storage() const { return m_pStorage; }
	class CMultiWorlds* MultiWorlds() const { return m_pMultiWorlds; }
	class CLocalization* Localization() const override { return m_pLocalization; }
	class IInputEvents* Input() const override;

	enum
	{
		MAX_RCONCMD_RATIO = 8,
		MAX_RCONCMD_SEND = 16,
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

		char m_aName[MAX_NAME_LENGTH];
		char m_aNameTransfersPrefix[MAX_NAME_LENGTH];

		char m_aClan[MAX_CLAN_LENGTH];
		char m_aLanguage[MAX_LANGUAGE_LENGTH];

		int m_Version;
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthTries;

		char m_aContinent[32];
		char m_aCountryIsoCode[8];

		int m_WorldID;
		int m_OldWorldID;
		bool m_ChangeWorld;
		int m_SpectatorID;

		int m_NextMapChunk;
		bool m_Quitting;
		const IConsole::CCommandInfo* m_pRconCmdToSend;

		int m_ClientVersion;
		bool m_IsClientMRPG;
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
	int m_aIdMap[MAX_CLIENTS * VANILLA_MAX_CLIENTS] {};

	class CInputEvents* m_pInputKeys;
	CLocalization* m_pLocalization;
	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
	CHttp m_Http;

	int64_t m_GameStartTime {};
	int m_RunServer;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex {};
	bool m_HeavyReload;

	// map
	enum
	{
		MAP_CHUNK_SIZE = NET_MAX_PAYLOAD - NET_MAX_CHUNKHEADERSIZE - 4, // msg type
	};

	int m_RconPasswordSet;
	int m_GeneratedRconPassword;

	std::shared_ptr<ILogger> m_pFileLogger = nullptr;
	std::shared_ptr<ILogger> m_pStdoutLogger = nullptr;

	CServer();
	~CServer() override;

	// world time
	int m_ShiftTime;
	int m_LastShiftTick;
	int m_GameMinuteTime {};
	int m_GameHourTime {};
	int m_GameTypeday {};

	int GetMinuteGameTime() const override;
	int GetHourGameTime() const override;
	int GetOffsetGameTime() const override;
	void SetOffsetGameTime(int Hour) override;
	const char* GetStringTypeday() const override;
	int GetCurrentTypeday() const override;

	// basic
	void SetClientName(int ClientID, const char* pName) override;
	void SetClientClan(int ClientID, char const* pClan) override;
	void SetClientCountry(int ClientID, int Country) override;
	void SetClientScore(int ClientID, int Score) override;

	bool IsClientChangingWorld(int ClientID) override;
	void ChangeWorld(int ClientID, int NewWorldID) override;
	int GetClientWorldID(int ClientID) const override;

	const char* ClientContinent(int ClientID) const override;
	const char* ClientCountryIsoCode(int ClientID) const override;

	const char* Localize(int ClientID, const char* pText) override;
	void SetClientLanguage(int ClientID, const char* pLanguage) override;
	const char* GetClientLanguage(int ClientID) const override;
	const char* GetWorldName(int WorldID) override;
	CWorldDetail* GetWorldDetail(int WorldID) override;
	bool IsWorldType(int WorldID, WorldType Type) const override;
	int GetWorldsSize() const override;
	void Kick(int ClientID, const char* pReason) override;

	int64_t TickStartTime(int Tick) const;
	int Init();

	void InitRconPasswordIfUnset();

	void SendLogLine(const CLogMessage* pMessage);
	void SetRconCID(int ClientID) override;
	int GetRconCID() const override;
	int GetRconAuthLevel() const override;
	int GetAuthedState(int ClientID) const override;
	bool IsAuthed(int ClientID) const override;
	bool IsBanned(int ClientID) override;
	bool IsEmpty(int ClientID) const override;
	void SetClientDDNetVersion(int ClientID, int DDNetVersion) override;
	int GetClientInfo(int ClientID, CClientInfo* pInfo) const override;
	void GetClientAddr(int ClientID, char* pAddrStr, int Size) const override;

	void SetSpectatorID(int ClientID, int SpectatorID) override;
	int GetSpectatorID(int ClientID) const override;

	void SetStateClientMRPG(int ClientID, bool State) override;
	bool GetStateClientMRPG(int ClientID) const override;

	const char* ClientName(int ClientID) const override;
	const char* ClientClan(int ClientID) const override;
	int ClientCountry(int ClientID) const override;
	bool ClientIngame(int ClientID) const override;
	int GetClientLatency(int ClientID) const override;
	int GetClientsCountByWorld(int WorldID) const override;

	int GetClientVersion(int ClientID) const override;
	int SendMsg(CMsgPacker* pMsg, int Flags, int ClientID, int64_t Mask = -1, int WorldID = -1) override;
	int SendMotd(int ClientID, const char *pText) override;

	void DoSnapshot(int WorldID);

	static int NewClientCallback(int ClientID, void* pUser);
	static int NewClientNoAuthCallback(int ClientID, void* pUser);
	static int DelClientCallback(int ClientID, const char* pReason, void* pUser);
	static int ClientRejoinCallback(int ClientID, void* pUser);

	void SendMapData(int ClientID, int Chunk);
	void SendCapabilities(int ClientID);
	void SendMap(int ClientID);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char* pLine);
	// Accepts -1 as ClientID to mean "all clients with at least auth level admin"
	void SendRconLogLine(int ClientID, const CLogMessage* pMessage);

	void SendRconCmdAdd(const IConsole::CCommandInfo* pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo* pCommandInfo, int ClientID);
	void UpdateClientRconCommands();

	void ProcessClientPacket(CNetChunk* pPacket);

	CBrowserCache m_aServerInfoCache[3 * 2];
	bool m_ServerInfoNeedsUpdate;
	int64_t m_ServerInfoFirstRequest;
	int m_ServerInfoNumRequests;

	void ExpireServerInfo() override;
	void CacheServerInfo(CBrowserCache* pCache, int Type, bool SendClients);
	void SendServerInfo(const NETADDR* pAddr, int Token, int Type, bool SendClients);
	bool RateLimitServerInfoConnless();
	void SendServerInfoConnless(const NETADDR* pAddr, int Token, int Type);
	void UpdateRegisterServerInfo();
	void UpdateServerInfo(bool Resend = false);

	void PumpNetwork(bool PacketWaiting);

	bool LoadMap(int ID);

	int Run(ILogger* pLogger);

	static void ConKick(IConsole::IResult* pResult, void* pUser);
	static void ConStatus(IConsole::IResult* pResult, void* pUser);
	static void ConShutdown(IConsole::IResult* pResult, void* pUser);
	static void ConReload(IConsole::IResult* pResult, void* pUser);
	static void ConLogout(IConsole::IResult* pResult, void* pUser);

	static void ConchainSpecialInfoupdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainModCommandUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainRconPasswordSet(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainLoglevel(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);
	static void ConchainStdoutOutputLevel(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData);

	static std::string CallbackLocalize(int ClientID, const char* pText, void* pUser);

	void RegisterCommands();

	// Bots
	void InitClientBot(int ClientID) override;

	int SnapNewID() override;
	void SnapFreeID(int ID) override;
	void* SnapNewItem(int Type, int ID, int Size) override;
	void SnapSetStaticsize(int ItemType, int Size) override;

	int* GetIdMap(int ClientID) override;

	void UpdateAccountBase(int UID, std::string Nickname, int Rating) override;
	const char* GetAccountNickname(int AccountID) override;
	int GetAccountRank(int AccountID) override;

	void SetLoggers(std::shared_ptr<ILogger>&& pFileLogger, std::shared_ptr<ILogger>&& pStdoutLogger);
private:
	struct BaseAccount
	{
		std::string Nickname {};
		int Rating {};

		bool operator<(const BaseAccount& other) const
		{
			return Rating > other.Rating;
		}
	};
	ska::unordered_map<int, BaseAccount> m_vBaseAccounts{};
	std::set<BaseAccount> m_vSortedRankAccounts{};
	void InitBaseAccounts();
};

extern CServer* CreateServer();

#endif