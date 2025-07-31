/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H
#include <engine/server.h>
#include <game/collision.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "multipliers.h"
#include "playerbot.h"

#include "core/mmo_controller.h"

class CGS : public IGameServer
{
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

	class IServer* m_pServer;
	class IConsole* m_pConsole;
	class IStorageEngine* m_pStorage;
	class CCommandProcessor* m_pCommandProcessor;
	class CMmoController* m_pMmoController;
	class CEntityManager* m_pEntityManager;
	class CPathFinder* m_pPathFinder;
	class CScenarioPlayerManager* m_pScenarioPlayerManager;
	class CScenarioGroupManager* m_pScenarioGroupManager;

	CPlayer* m_apPlayers[MAX_CLIENTS];
	CBroadcastState m_aBroadcastStates[MAX_PLAYERS];
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;
	bool m_AllowedPVP;
	vec2 m_JailPosition;
	int m_WorldID;

public:
	IServer *Server() const { return m_pServer; }
	IConsole* Console() const { return m_pConsole; }
	CMmoController* Core() const { return m_pMmoController; }
	IStorageEngine* Storage() const { return m_pStorage; }
	CCommandProcessor* CommandProcessor() const { return m_pCommandProcessor; }
	CEntityManager* EntityManager() const { return m_pEntityManager; }
	CPathFinder* PathFinder() const { return m_pPathFinder; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	CScenarioGroupManager* ScenarioGroupManager() const { return m_pScenarioGroupManager; }
	CScenarioPlayerManager* ScenarioPlayerManager() const { return m_pScenarioPlayerManager; }

	CEventHandler m_Events;
	IGameController* m_pController;
	CGameWorld m_World;
	Multipliers m_Multipliers;

	CGS();
	~CGS() override;

	class CCharacter *GetPlayerChar(int ClientID) const;
	CPlayer *GetPlayer(int ClientID, bool CheckAuth = false, bool CheckCharacter = false) const;
	CPlayer *GetPlayerByUserID(int AccountID) const;
	class CItemDescription* GetItemInfo(ItemIdentifier ItemID) const;
	CQuestDescription* GetQuestInfo(QuestIdentifier QuestID) const;
	class CAttributeDescription* GetAttributeInfo(AttributeIdentifier ID) const;
	class CQuestsBoard* GetQuestBoard(int ID) const;
	class CWorldData* GetWorldData(int ID = -1) const;
	class CEidolonInfoData* GetEidolonByItemID(ItemIdentifier ItemID) const;

	void CreateFinishEffect(vec2 Pos, int64_t Mask = -1);
	void CreateBirthdayEffect(vec2 Pos, int64_t Mask = -1);
	void CreateDamage(vec2 Pos, int FromCID, int Amount, float Angle = 0.f, int64_t Mask = -1);
	void CreateHammerHit(vec2 Pos, int64_t Mask = -1);
	void CreateRandomRadiusExplosion(int ExplosionCount, float Radius, vec2 Pos, int Owner, int Weapon, int MaxDamage, int ForceFlag = 0);
	void CreateCircleExplosion(int ExplosionCount, float Radius, vec2 Pos, int Owner, int Weapon, int MaxDamage, int ForceFlag = 0);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage, int ForceFlag = 0);
	void CreatePlayerSpawn(vec2 Pos, int64_t Mask = -1);
	void CreateDeath(vec2 Pos, int ClientID, int64_t Mask = -1);
	void CreateSound(vec2 Pos, int Sound, int64_t Mask = -1);
	void CreatePlayerSound(int ClientID, int Sound);

	void SnapLaser(int SnappingClient, int ID, const vec2& To, const vec2& From, int StartTick,
		int LaserType = LASERTYPE_RIFLE, int Subtype = 0, int Owner = -1, int Flags = 0, int SwitchNumber = 0) const;
	void SnapPickup(int SnappingClient, int ID, const vec2& Pos, int Type = POWERUP_HEALTH, int SubType = 0, int Flags = 0) const;
	void SnapProjectile(int SnappingClient, int ID, const vec2& Pos, const vec2& Vel, int StartTick,
		int Type = WEAPON_HAMMER, int Owner = -1, int Flags = 0) const;

	void AddBroadcast(int ClientID, const char* pText, BroadcastPriority Priority, int LifeSpan);
	void BroadcastTick(int ClientID);
	void MarkUpdatedBroadcast(int ClientID);
	void SendChatTarget(int ClientID, const char *pText) const;
	void SendChat(int ChatterClientID, int Mode, const char *pText, int64_t Mask = -1);
	void SendChatRadius(int ChatterClientID, float Radius, const char *pText);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendTuningParams(int ClientID);

	void OnInit(int WorldID) override;
	void OnConsoleInit() override;
	void OnShutdown() override { delete this; }
	void OnDaytypeChange(int NewDaytype) override;
	void OnTick() override;
	void OnTickGlobal() override;
	void OnSnap(int ClientID) override;
	void OnPostSnap() override;
	void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) override;
	void OnClientConnected(int ClientID) override;
	void OnClientPrepareChangeWorld(int ClientID) override;
	void OnClientEnter(int ClientID, bool FirstEnter) override;
	void OnClientDrop(int ClientID, const char *pReason) override;
	void OnClientDirectInput(int ClientID, void *pInput) override;
	void OnClientPredictedInput(int ClientID, void *pInput) override;
	void OnUpdateClientServerInfo(nlohmann::json* pJson, int ClientID) override;
	bool IsClientReady(int ClientID) const override;
	bool IsClientPlayer(int ClientID) const override;
	bool IsClientCharacterExist(int ClientID) const override;
	bool IsClientMRPG(int ClientID) const;
	bool PlayerExists(int ClientID) const override { return m_apPlayers[ClientID]; }
	void* GetLastInput(int ClientID) const override;
	int GetClientVersion(int ClientID) const;
	const char *Version() const override;
	const char *NetVersion() const override;
	void OnClearClientData(int ClientID) override;

	void UpdateCollisionZones();
	bool SendMenuMotd(CPlayer* pPlayer, int Menulist) const;

	int GetWorldID() const { return m_WorldID; }
	bool IsWorldType(WorldType Type) const;
	bool HasWorldFlag(int64_t Flag) const;
	bool IsDutyStarted() const;
	bool IsPlayerInWorld(int ClientID, std::optional<int> WorldIdOpt = std::nullopt) const;
	bool IsAllowedPVP() const { return m_AllowedPVP; }
	vec2 GetJailPosition() const { return m_JailPosition; }
	bool ArePlayersNearby(vec2 Pos, float Distance) const;

	CPlayerBot* CreateBot(short BotType, int BotID, int SubID);
	bool TakeItemCharacter(int ClientID);
	void UpdateVotesIfForAll(int MenuList) const;
	bool OnClientVoteCommand(int ClientID, const char* pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char* pReason);
	bool OnClientMotdCommand(int ClientID, const char* pCmd);
	bool DestroyPlayer(int ClientID);

private:
	void InitWorld();
	void ProcessNicknameChange(CPlayer* pPlayer, const char* pNewNickname) const;
	void UpdateWorldMultipliers();
	void ResetWorldMultipliers();

public:
	template<typename... Ts> void Chat(int ClientID, const char* pText, const Ts&... args);
	template<typename... Ts> bool ChatAccount(int AccountID, const char* pText, const Ts&... args);
	template<typename... Ts> void ChatGuild(int GuildID, const char* pText, const Ts&... args);
	template<typename... Ts> void ChatWorld(int WorldID, const char* pSuffix, const char* pText, const Ts&... args);
	template<typename... Ts> void Motd(int ClientID, const char* pText, const Ts&... args);
	template<typename... Ts> void Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args);
	template<typename... Ts> void BroadcastWorld(int WorldID, BroadcastPriority Priority, int LifeSpan, const char* pText, const Ts&... args);
};

#include "gamecontext_msg_impl.hpp"

inline int64_t CmaskAll() { return -1; }
inline int64_t CmaskOne(int ClientID) { return (int64_t)1<<ClientID; }
inline int64_t CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64_t Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }

#endif
