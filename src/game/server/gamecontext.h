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

#include "mmocore/MmoController.h"

class CGS : public IGameServer
{
	/* #########################################################################
		VAR AND OBJECT GAMECONTEX DATA
	######################################################################### */
	class IServer* m_pServer;
	class IConsole* m_pConsole;
	class CPathfinder* m_pPathFinder;
	class IStorageEngine* m_pStorage;
	class CCommandProcessor* m_pCommandProcessor;
	class MmoController* m_pMmoController;
	class CLayers* m_pLayers;

	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;

	int m_WorldID;
	int m_DungeonID;
	int m_RespawnWorldID;
	int m_MusicID;

public:
	IServer *Server() const { return m_pServer; }
	IConsole* Console() const { return m_pConsole; }
	MmoController* Mmo() const { return m_pMmoController; }
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
	CPathfinder* PathFinder() const { return m_pPathFinder; }

	/* #########################################################################
		SWAP GAMECONTEX DATA
	######################################################################### */
	static std::unordered_map < std::string /* effect */, int /* seconds */ > ms_aEffects[MAX_PLAYERS];
	// - - - - - - - - - - - -
	struct StructAttribut
	{
		char m_aName[32];
		char m_aFieldName[32];
		int m_UpgradePrice;
		int m_Type;
		int m_Devide;
	};
	static std::map < int, StructAttribut > ms_aAttributsInfo;

	/* #########################################################################
		HELPER PLAYER FUNCTION
	######################################################################### */
	class CCharacter *GetPlayerChar(int ClientID);
	CPlayer *GetPlayer(int ClientID, bool CheckAuthed = false, bool CheckCharacter = false);
	CPlayer *GetPlayerFromUserID(int AccountID);
	std::unique_ptr<char[]> LevelString(int MaxValue, int CurrentValue, int Step, char toValue, char fromValue);
	CItemDataInfo &GetItemInfo(int ItemID) const;
	CQuestDataInfo &GetQuestInfo(int QuestID) const;
	const char* GetSymbolHandleMenu(int ClientID, bool HidenTabs, int ID) const;

	/* #########################################################################
		EVENTS
	######################################################################### */
	void CreateDamage(vec2 Pos, int ClientID, int Amount, bool CritDamage);
	void CreateHammerHit(vec2 Pos);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage);
	void CreatePlayerSpawn(vec2 Pos, int64 Mask = -1);
	void CreateDeath(vec2 Pos, int ClientID);
	void CreateSound(vec2 Pos, int Sound, int64 Mask = -1);
	void CreatePlayerSound(int ClientID, int Sound);

	/* #########################################################################
		CHAT FUNCTIONS
	######################################################################### */
private:
	void SendChat(int ChatterClientID, int Mode, int To, const char *pText);
	void UpdateDiscordStatus();

public:
	void FakeChat(const char *pName, const char *pText) override;
	void Chat(int ClientID, const char* pText, ...);
	bool ChatAccount(int AccountID, const char* pText, ...);
	void ChatDiscord(int Color, const char *Title, const char* pText, ...);
	void ChatDiscordChannel(const char* pChanel, int Color, const char* Title, const char* pText, ...);
	void ChatGuild(int GuildID, const char* pText, ...);
	void ChatWorldID(int WorldID, const char *Suffix, const char* pText, ...);
	void Motd(int ClientID, const char* Text, ...);

	/* #########################################################################
		BROADCAST FUNCTIONS
	######################################################################### */
private:
	struct CBroadcastState
	{
		int m_LifeSpanTick;
		int m_NoChangeTick;
		char m_aPrevMessage[1024];

		BroadcastPriority m_Priority;
		char m_aNextMessage[1024];

		BroadcastPriority m_TimedPriority;
		char m_aTimedMessage[1024];
	};
	CBroadcastState m_aBroadcastStates[MAX_PLAYERS];

public:
	void AddBroadcast(int ClientID, const char* pText, BroadcastPriority Priority, int LifeSpan);
	void Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char *pText, ...);
	void BroadcastWorldID(int WorldID, BroadcastPriority Priority, int LifeSpan, const char *pText, ...);
	void BroadcastTick(int ClientID);

	/* #########################################################################
		PACKET MESSAGE FUNCTIONS
	######################################################################### */
	void SendEmoticon(int ClientID, int Emoticon, bool SenderClient = false);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendMotd(int ClientID);
	void SendSettings(int ClientID);
	void SendSkinChange(int ClientID, int TargetID);
	void SendTeam(int ClientID, int Team, bool DoChatMsg, int TargetID);
	void SendGameMsg(int GameMsgID, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID);

	void SendTuningParams(int ClientID);

	/* #########################################################################
		ENGINE GAMECONTEXT
	######################################################################### */
	void OnInit(int WorldID) override;
	void OnConsoleInit() override;
	void OnShutdown() override { delete this; }

	void OnTick() override;
	void OnTickMainWorld() override;
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
	bool IsClientReady(int ClientID) const override;
	bool IsClientPlayer(int ClientID) const override;

	const char *Version() const override;
	const char *NetVersion() const override;

	void ClearClientData(int ClientID) override;
	int GetRank(int AccountID) override;

	/* #########################################################################
		CONSOLE GAMECONTEXT
	######################################################################### */
private:
	static void ConSetWorldTime(IConsole::IResult *pResult, void *pUserData);
	static void ConParseSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConGiveItem(IConsole::IResult *pResult, void *pUserData);
	static void ConRemItem(IConsole::IResult* pResult, void* pUserData);
	static void ConDisbandGuild(IConsole::IResult* pResult, void* pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConAddCharacter(IConsole::IResult *pResult, void *pUserData);
	static void ConSyncLinesForTranslate(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	/* #########################################################################
		VOTING MMO GAMECONTEXT
	######################################################################### */
	std::list<CVoteOptions> m_aPlayerVotes[MAX_PLAYERS];

public:
	void AV(int ClientID , const char *pCmd, const char *pDesc = "\0", int TempInt = -1, int TempInt2 = -1, const char *pIcon = "unused", VoteCallBack Callback = nullptr);
	void AVL(int ClientID, const char *pCmd, const char *pText, ...);
	void AVH(int ClientID, int HideID, const char *pText, ...);
	void AVHI(int ClientID, const char *pIcon, int HideID, vec3 Color, const char *pText, ...);
	void AVM(int ClientID, const char *pCmd, int TempInt, int HideID, const char* pText, ...);
	void AVMI(int ClientID, const char *pIcon, const char *pCmd, int TempInt, int HideID, const char *pText, ...);
	void AVD(int ClientID, const char *pCmd, int TempInt, int TempInt2, int HideID, const char *pText, ...);

	// TODO: fixme. improve the system using the ID method, as well as the ability to implement Backpage
	void AVCALLBACK(int To, const char* Type, const char* Icon, int ID, int ID2, int HideID, VoteCallBack Callback, const char* pText, ...);

	void ClearVotes(int ClientID);
	void ShowVotesNewbieInformation(int ClientID);
	void ResetVotes(int ClientID, int MenuList);
	void StrongUpdateVotes(int ClientID, int MenuList);
	void StrongUpdateVotesForAll(int MenuList);
	void AddVotesBackpage(int ClientID);
	void ShowVotesPlayerStats(CPlayer *pPlayer);
	void ShowVotesItemValueInformation(CPlayer *pPlayer, int ItemID = itGold);
	bool ParsingVoteCommands(int ClientID, const char *CMD, int VoteID, int VoteID2, int Get, const char *Text, VoteCallBack Callback = nullptr);

	/* #########################################################################
		MMO GAMECONTEXT
	######################################################################### */
	int CreateBot(short BotType, int BotID, int SubID);
	void CreateText(CEntity* pParent, bool Follow, vec2 Pos, vec2 Vel, int Lifespan, const char* pText);
	void CreateParticleExperience(vec2 Pos, int ClientID, int Experience, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropBonuses(vec2 Pos, int Type, int Value, int NumDrop = 1, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropItem(vec2 Pos, int ClientID, CItemData DropItem, vec2 Force = vec2(0.0f, 0.0f));
	void CreateRandomDropItem(vec2 Pos, int ClientID, float Chance, CItemData DropItem, vec2 Force = vec2(0.0f, 0.0f));
	bool TakeItemCharacter(int ClientID);
	void SendInbox(const char* pFrom, CPlayer *pPlayer, const char* Name, const char* Desc, int ItemID = -1, int Value = -1, int Enchant = -1);
	void SendInbox(const char* pFrom, int AccountID, const char* Name, const char* Desc, int ItemID = -1, int Value = -1, int Enchant = -1);

private:
	void SendDayInfo(int ClientID);

public:
	bool IsMmoClient(int ClientID) const;
	int GetWorldID() const { return m_WorldID; }
	int GetDungeonID() const { return m_DungeonID; }
	bool IsDungeon() const { return (m_DungeonID > 0); }
	int GetExperienceMultiplier(int Experience) const;
	bool IsPlayerEqualWorldID(int ClientID, int WorldID = -1) const;
	bool IsAllowedPVP() const { return m_AllowedPVP; }

	bool CheckingPlayersDistance(vec2 Pos, float Distance) const;
	void SetMapMusic(int SoundID) { m_MusicID = SoundID; }
	void SetRespawnWorld(int WorldID) { m_RespawnWorldID = WorldID; }
	int GetRespawnWorld() const { return m_RespawnWorldID; }

private:
	void UpdateZonePVP();
	void UpdateZoneDungeon();

	bool m_AllowedPVP;
	int m_DayEnumType;
	static int m_MultiplierExp;
};

inline int64 CmaskAll() { return -1; }
inline int64 CmaskOne(int ClientID) { return (int64)1<<ClientID; }
inline int64 CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64 Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }

#endif
