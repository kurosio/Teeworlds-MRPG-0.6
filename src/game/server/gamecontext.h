/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/console.h>
#include <engine/server.h>

#include <game/collision.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"
#include "playerbot.h"

#include "core/entities/tools/flying_point.h"
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
	/* [[nodiscard]] */class CFlyingPoint* CreateFlyingPoint(vec2 Pos, vec2 InitialVel, int ClientID, int FromID = -1);

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
		CHAT FUNCTIONS
	######################################################################### */
private:
	void SendChat(int ChatterClientID, int Mode, const char *pText);
	void UpdateDiscordStatus();

public:
	/*
	 * Message Chat (default)
	 */
	template< typename ... Ts> void Chat(int ClientID, const char* pText, Ts&&... args)
	{
		const int Start = (ClientID < 0 ? 0 : ClientID);
		const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

		CNetMsg_Sv_Chat Msg { -1, -1 };
		for(int i = Start; i < End; i++)
		{
			if(m_apPlayers[i])
			{
				std::string endText = Server()->Localization()->Format(m_apPlayers[i]->GetLanguage(), pText, std::forward<Ts>(args) ...);
				Msg.m_pMessage = endText.c_str();
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			}
		}
	}

	/*
	 * Message ChatAccount (by AccountID)
	 */
	template <typename ... Ts> bool ChatAccount(int AccountID, const char* pText, Ts&&... args)
	{
		CPlayer* pPlayer = GetPlayerByUserID(AccountID);
		if(!pPlayer)
			return false;

		CNetMsg_Sv_Chat Msg { -1, -1 };
		std::string endText = Server()->Localization()->Format(pPlayer->GetLanguage(), pText, std::forward<Ts>(args) ...);
		Msg.m_pMessage = endText.c_str();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, pPlayer->GetCID());
		return true;
	}

	/*
	 * Message ChatDiscord (default)
	 */
	template <typename ... Ts> void ChatDiscord(int Color, const char *Title, const char* pText, Ts&&... args)
	{
#ifdef CONF_DISCORD
		std::string endText = Server()->Localization()->Format("en", pText, std::forward<Ts>(args) ...);
		Server()->SendDiscordMessage(g_Config.m_SvDiscordServerChatChannel, Color, Title, endText.c_str());
#endif
	}

	/*
	 * Message ChatDiscordChannel (Channel)
	 */
	template <typename ... Ts> void ChatDiscordChannel(const char* pChanel, int Color, const char* Title, const char* pText, Ts&&... args)
	{
#ifdef CONF_DISCORD
		std::string endText = Server()->Localization()->Format("en", pText, std::forward<Ts>(args) ...);
		Server()->SendDiscordMessage(pChanel, Color, Title, endText.c_str());
#endif
	}

	/*
	 * Message Chat (by GuildID)
	 */
	template <typename ... Ts> void ChatGuild(int GuildID, const char* pText, Ts&&... args)
	{
		if(GuildID <= 0)
			return;

		dynamic_string Buffer {};
		CNetMsg_Sv_Chat Msg {-1, -1};
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && pPlayer->Account()->SameGuild(GuildID, i))
			{
				Buffer.append("Guild | ");
				Server()->Localization()->Format(Buffer, m_apPlayers[i]->GetLanguage(), pText, std::forward<Ts>(args) ...);
				Msg.m_pMessage = Buffer.buffer();
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
				Buffer.clear();
			}
		}
	}

	/*
	 * Message Chat (by WorldID)
	 */
	template <typename ... Ts> void ChatWorldID(int WorldID, const char* Suffix, const char* pText, Ts&&... args)
	{
		dynamic_string Buffer;
		CNetMsg_Sv_Chat Msg {-1, -1};
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPlayer = GetPlayer(i, true);
			if(!pPlayer || !IsPlayerEqualWorld(i, WorldID))
				continue;

			if(Suffix && Suffix[0] != '\0')
			{
				Buffer.append(Suffix);
				Buffer.append(" ");
			}
			Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), pText, std::forward<Ts>(args) ...);
			Msg.m_pMessage = Buffer.buffer();
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i, WorldID);
			Buffer.clear();
		}
	}

	/*
	 * Message Motd
	 */
	template <typename ... Ts> void Motd(int ClientID, const char* pText, Ts&&... args)
	{
		CPlayer* pPlayer = GetPlayer(ClientID, true);
		if(pPlayer)
		{
			std::string endText = Server()->Localization()->Format(pPlayer->GetLanguage(), pText, std::forward<Ts>(args) ...);
			CNetMsg_Sv_Motd Msg;
			Msg.m_pMessage = endText.c_str();
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
		}
	}

	/* #########################################################################
		BROADCAST FUNCTIONS
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
	/*
	 * Message broadcast
	 */
	template <typename ... Args> void Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char *pText, Args&&... args)
	{
		const int Start = (ClientID < 0 ? 0 : ClientID);
		const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

		for(int i = Start; i < End; i++)
		{
			if(m_apPlayers[i])
			{
				std::string endText = Server()->Localization()->Format(m_apPlayers[i]->GetLanguage(), pText, std::forward<Args>(args) ...);
				AddBroadcast(i, endText.c_str(), Priority, LifeSpan);
			}
		}
	}

	// Message Broadcast (by WorldID)
	template <typename ... Args> void BroadcastWorldID(int WorldID, BroadcastPriority Priority, int LifeSpan, const char *pText, Args&&... args)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(m_apPlayers[i] && IsPlayerEqualWorld(i, WorldID))
			{
				std::string endText = Server()->Localization()->Format(m_apPlayers[i]->GetLanguage(), pText, std::forward<Args>(args) ...);
				AddBroadcast(i, endText.c_str(), Priority, LifeSpan);
			}
		}
	}

	void AddBroadcast(int ClientID, const char* pText, BroadcastPriority Priority, int LifeSpan);
	void BroadcastTick(int ClientID);
	void MarkUpdatedBroadcast(int ClientID);

	/* #########################################################################
		PACKET MESSAGE FUNCTIONS
	######################################################################### */
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendMotd(int ClientID);
	void SendTuningParams(int ClientID);

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
	void PrepareClientChangeWorld(int ClientID) override;

	void OnClientEnter(int ClientID) override;
	void OnClientDrop(int ClientID, const char *pReason) override;
	void OnClientDirectInput(int ClientID, void *pInput) override;
	void OnClientPredictedInput(int ClientID, void *pInput) override;
	void OnUpdatePlayerServerInfo(nlohmann::json* pJson, int ClientID) override;
	bool IsClientReady(int ClientID) const override;
	bool IsClientPlayer(int ClientID) const override;
	bool IsClientCharacterExist(int ClientID) const override;
	bool IsClientMRPG(int ClientID) const;
	bool PlayerExists(int ClientID) const override { return m_apPlayers[ClientID]; }

	void* GetLastInput(int ClientID) const override;
	int GetClientVersion(int ClientID) const;
	const char *Version() const override;
	const char *NetVersion() const override;

	void ClearClientData(int ClientID) override;
	int GetRank(int AccountID) override;

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

public:
	void AV(int ClientID , const char *pCmd, const char *pDesc = "\0", int TempInt = -1, int TempInt2 = -1);

private:
	void ShowVotesNewbieInformation(int ClientID);

public:
	void UpdateVotesIfForAll(int MenuList);
	bool ParsingVoteCommands(int ClientID, const char *CMD, int VoteID, int VoteID2, int Get, const char *Text);

	/* #########################################################################
		MMO GAMECONTEXT
	######################################################################### */
	int CreateBot(short BotType, int BotID, int SubID);
	bool CreateText(CEntity* pParent, bool Follow, vec2 Pos, vec2 Vel, int Lifespan, const char* pText);
	void CreateParticleExperience(vec2 Pos, int ClientID, int Experience, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropBonuses(vec2 Pos, int Type, int Value, int NumDrop = 1, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropItem(vec2 Pos, int ClientID, CItem DropItem, vec2 Force = vec2(0.0f, 0.0f));
	void CreateRandomDropItem(vec2 Pos, int ClientID, float Chance, CItem DropItem, vec2 Force = vec2(0.0f, 0.0f));
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
