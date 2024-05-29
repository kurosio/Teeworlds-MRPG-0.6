/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H
#include <engine/server.h>
#include <game/collision.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "playerbot.h"

#include "core/mmo_controller.h"

class CGS : public IGameServer
{
	/* #########################################################################
		VAR AND OBJECT GAMECONTEX DATA
	######################################################################### */
	class IServer* m_pServer;
	class IConsole* m_pConsole;
	class CPathFinder* m_pPathFinder;
	class IStorageEngine* m_pStorage;
	class CCommandProcessor* m_pCommandProcessor;
	class CMmoController* m_pMmoController;
	class CLayers* m_pLayers;
	class CEntityManager* m_pEntityManager;

	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	int m_WorldID;

public:
	IServer *Server() const { return m_pServer; }
	IConsole* Console() const { return m_pConsole; }
	CMmoController* Core() const { return m_pMmoController; }
	IStorageEngine* Storage() const { return m_pStorage; }
	CCommandProcessor* CommandProcessor() const { return m_pCommandProcessor; }
	CEntityManager* EntityManager() const { return m_pEntityManager; }

	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }

	CGS();
	~CGS() override;

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];
	IGameController *m_pController;
	CGameWorld m_World;
	CPathFinder* PathFinder() const { return m_pPathFinder; }

	/* #########################################################################
		SWAP GAMECONTEX DATA
	######################################################################### */
	static ska::unordered_map < std::string /* effect */, int /* seconds */ > ms_aEffects[MAX_PLAYERS];
	// - - - - - - - - - - - -

	/* #########################################################################
		HELPER PLAYER FUNCTION
	######################################################################### */
	class CCharacter *GetPlayerChar(int ClientID) const;
	CPlayer *GetPlayer(int ClientID, bool CheckAuthed = false, bool CheckCharacter = false) const;
	CPlayer *GetPlayerByUserID(int AccountID) const;
	class CItemDescription* GetItemInfo(ItemIdentifier ItemID) const;
	CQuestDescription* GetQuestInfo(QuestIdentifier QuestID) const;
	class CAttributeDescription* GetAttributeInfo(AttributeIdentifier ID) const;
	class CQuestsDailyBoard* GetQuestDailyBoard(int ID) const;
	class CWorldData* GetWorldData(int ID = -1) const;
	class CEidolonInfoData* GetEidolonByItemID(ItemIdentifier ItemID) const;
	void UpdateDiscordStatus();

	/* #########################################################################
		EVENTS
	######################################################################### */
	void CreateDamage(vec2 Pos, int FromCID, int Amount, bool CritDamage, float Angle = 0.f, int64_t Mask = -1);
	void CreateHammerHit(vec2 Pos, int64_t Mask = -1);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage);
	void CreatePlayerSpawn(vec2 Pos, int64_t Mask = -1);
	void CreateDeath(vec2 Pos, int ClientID, int64_t Mask = -1);
	void CreateSound(vec2 Pos, int Sound, int64_t Mask = -1);
	void CreatePlayerSound(int ClientID, int Sound);

	/* #########################################################################
		MESSAGES
	######################################################################### */
private:
	struct CBroadcastState
	{
		int m_NoChangeTick;
		char m_PrevMessage[1024];

		BroadcastPriority m_NextPriority;
		char m_NextMessage[1024];
		char m_aCompleteMsg[1024];
		bool m_Updated;

		int m_LifeSpanTick;
		BroadcastPriority m_TimedPriority;
		char m_TimedMessage[1024];
	};
	CBroadcastState m_aBroadcastStates[MAX_PLAYERS];

public:
	void AddBroadcast(int ClientID, const char* pText, BroadcastPriority Priority, int LifeSpan);
	void BroadcastTick(int ClientID);
	void MarkUpdatedBroadcast(int ClientID);
	void SendChatTarget(int ClientID, const char *pText) const;
	void SendChat(int ChatterClientID, int Mode, const char *pText);
	void SendMotd(int ClientID, const char* pText);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendTuningParams(int ClientID);

public:
	template< typename ... Ts>
	void Chat(int ClientID, const char* pText, const Ts&... args)
	{
		const int Start = (ClientID < 0 ? 0 : ClientID);
		const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

		for(int i = Start; i < End; i++)
		{
			if(m_apPlayers[i])
				SendChatTarget(i, fmt_handle(i, pText, args...).c_str());
		}
	}

	template <typename ... Ts>
	bool ChatAccount(int AccountID, const char* pText, const Ts&... args)
	{
		CPlayer* pPlayer = GetPlayerByUserID(AccountID);
		if(pPlayer)
			SendChatTarget(pPlayer->GetCID(), fmt_handle(pPlayer->GetCID(), pText, args...).c_str());
		return pPlayer != nullptr;
	}

	template <typename ... Ts>
	void ChatGuild(int GuildID, const char* pText, const Ts&... args)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && pPlayer->Account()->SameGuild(GuildID, i))
			{
				std::string Result = "Guild | ";
				Result += fmt_handle(i, pText, args...);
				SendChatTarget(i, Result.c_str());
			}
		}
	}

	template <typename ... Ts>
	void ChatWorld(int WorldID, const char* pSuffix, const char* pText, const Ts&... args)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && IsPlayerEqualWorld(i, WorldID))
			{
				std::string Result = pSuffix[0] != '\0' ? std::string(pSuffix) + " " : "";
				Result += fmt_handle(i, pText, args...);
				SendChatTarget(i, Result.c_str());
			}
		}
	}

	template <typename ... Ts>
	void ChatDiscord(int Color, const char *Title, const char* pText, const Ts&... args)
	{
#ifdef CONF_DISCORD
		Server()->SendDiscordMessage(g_Config.m_SvDiscordServerChatChannel, Color, Title, fmt(pText, args...).c_str());
#endif
	}

	template <typename ... Ts>
	void ChatDiscordChannel(const char* pChanel, int Color, const char* Title, const char* pText, const Ts&... args)
	{
#ifdef CONF_DISCORD
		Server()->SendDiscordMessage(pChanel, Color, Title, fmt(pText, args...).c_str());
#endif
	}

	template <typename ... Ts>
	void Motd(int ClientID, const char* pText, const Ts&... args)
	{
		const int Start = (ClientID < 0 ? 0 : ClientID);
		const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

		for(int i = Start; i < End; i++)
		{
			if(m_apPlayers[i])
				SendMotd(i, fmt_handle(i, pText, args...).c_str());
		}
	}

	template <typename ... Ts>
	void Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args)
	{
		const int Start = (ClientID < 0 ? 0 : ClientID);
		const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

		for(int i = Start; i < End; i++)
		{
			if(m_apPlayers[i])
				AddBroadcast(i, fmt_handle(i, pText, args...).c_str(), Priority, LifeSpan);
		}
	}

	template <typename ... Ts>
	void BroadcastWorld(int WorldID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(m_apPlayers[i] && IsPlayerEqualWorld(i, WorldID))
				AddBroadcast(i, fmt_handle(i, pText, args...).c_str(), Priority, LifeSpan);
		}
	}

	/* #########################################################################
		ENGINE GAMECONTEXT
	######################################################################### */
	void OnInit(int WorldID) override;
	void OnConsoleInit() override;
	void OnShutdown() override { delete this; }

	void OnTick() override;
	void OnTickGlobal() override;
	void OnPreSnap() override;
	void OnSnap(int ClientID) override;
	void OnPostSnap() override;

	void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) override;
	void OnClientConnected(int ClientID) override;
	void OnClientPrepareChangeWorld(int ClientID) override;

	void OnClientEnter(int ClientID) override;
	void OnClientDrop(int ClientID, const char *pReason) override;
	void OnClientDirectInput(int ClientID, void *pInput) override;
	void OnClientPredictedInput(int ClientID, void *pInput) override;
	void OnUpdateClientServerInfo(nlohmann::json* pJson, int ClientID) override;
	bool IsClientReady(int ClientID) const override;
	bool IsClientPlayer(int ClientID) const override;
	bool IsClientCharacterExist(int ClientID) const override;
	bool IsClientMRPG(int ClientID) const;
	bool PlayerExists(int ClientID) const override { return m_apPlayers[ClientID]; }

	int GetRank(int AccountID) const override;
	void* GetLastInput(int ClientID) const override;
	int GetClientVersion(int ClientID) const;
	const char *Version() const override;
	const char *NetVersion() const override;
	void OnClearClientData(int ClientID) override;

	/* #########################################################################
		CONSOLE GAMECONTEXT
	######################################################################### */
private:
	static void ConSetWorldTime(IConsole::IResult *pResult, void *pUserData);
	static void ConItemList(IConsole::IResult *pResult, void *pUserData);
	static void ConGiveItem(IConsole::IResult *pResult, void *pUserData);
	static void ConRemItem(IConsole::IResult* pResult, void* pUserData);
	static void ConDisbandGuild(IConsole::IResult* pResult, void* pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConAddCharacter(IConsole::IResult *pResult, void *pUserData);
	static void ConSyncLinesForTranslate(IConsole::IResult *pResult, void *pUserData);
	static void ConListAfk(IConsole::IResult *pResult, void *pUserData);
	static void ConCheckAfk(IConsole::IResult *pResult, void *pUserData);
	static void ConBanAcc(IConsole::IResult *pResult, void *pUserData);
	static void ConUnBanAcc(IConsole::IResult *pResult, void *pUserData);
	static void ConBansAcc(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	/* #########################################################################
		VOTING MMO GAMECONTEXT
	######################################################################### */
	void ShowVotesNewbieInformation(int ClientID);

public:
	void UpdateVotesIfForAll(int MenuList);
	bool OnClientVoteCommand(int ClientID, const char *pCmd, int Extra1, int Extra2, int ReasonNumber, const char *pReason);

	/* #########################################################################
		MMO GAMECONTEXT
	######################################################################### */
	int CreateBot(short BotType, int BotID, int SubID);
	bool TakeItemCharacter(int ClientID);
	void CreateLaserOrbite(class CEntity* pEntParent, int Amount, EntLaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1);
	void CreateLaserOrbite(int ClientID, int Amount, EntLaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1);
	class CLaserOrbite* CreateLaserOrbite(CEntity* pEntParent, int Amount, EntLaserOrbiteType Type, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1);

private:
	void SendDayInfo(int ClientID);

public:
	int GetWorldID() const { return m_WorldID; }
	bool IsWorldType(WorldType Type) const;
	int GetExperienceMultiplier(int Experience) const;
	bool IsPlayerEqualWorld(int ClientID, int WorldID = -1) const;
	bool IsAllowedPVP() const { return m_AllowedPVP; }
	vec2 GetJailPosition() const { return m_JailPosition; }

	bool IsPlayersNearby(vec2 Pos, float Distance) const;
private:
	void InitZones();

	bool m_AllowedPVP;
	int m_DayEnumType;
	vec2 m_JailPosition;
	static int m_MultiplierExp;
};

inline int64_t CmaskAll() { return -1; }
inline int64_t CmaskOne(int ClientID) { return (int64_t)1<<ClientID; }
inline int64_t CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64_t Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }

#endif
