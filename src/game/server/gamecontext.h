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

#include "mmocore/Utils/SafePtr.h"

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

	/* #########################################################################
		HELPER PLAYER FUNCTION
	######################################################################### */
	class CCharacter *GetPlayerChar(int ClientID);
	CPlayer *GetPlayer(int ClientID, bool CheckAuthed = false, bool CheckCharacter = false);
	CPlayer *GetPlayerFromUserID(int AccountID);
	std::unique_ptr<char[]> LevelString(int MaxValue, int CurrentValue, int Step, char toValue, char fromValue);
	class CItemDescription* GetItemInfo(ItemIdentifier ItemID) const;
	CQuestDataInfo &GetQuestInfo(int QuestID) const;
	class CAttributeDescription* GetAttributeInfo(AttributeIdentifier ID) const;
	class CWarehouse* GetWarehouse(int ID) const;
	class CWorldData* GetWorldData(int ID = -1) const;

	/* #########################################################################
		EVENTS
	######################################################################### */
	void CreateDamage(vec2 Pos, int FromCID, int Amount, bool CritDamage);
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
	void SendChat(int ChatterClientID, int Mode, const char *pText);
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
	bool PlayerExists(int ClientID) const override { return m_apPlayers[ClientID]; }

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
	static void ConGiveItem(IConsole::IResult *pResult, void *pUserData);
	static void ConRemItem(IConsole::IResult* pResult, void* pUserData);
	static void ConDisbandGuild(IConsole::IResult* pResult, void* pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConAddCharacter(IConsole::IResult *pResult, void *pUserData);
	static void ConSyncLinesForTranslate(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	/* #########################################################################
		VOTING MMO GAMECONTEXT TODO: rework fully
	######################################################################### */
	std::mutex m_mtxUniqueVotes;
	safe_ptr< std::deque<CVoteOptions> > m_aPlayerVotes[MAX_PLAYERS];
	static void CallbackUpdateVotes(CGS* pGS, int ClientID, int Menulist, bool PrepareCustom);

public:
	void AV(int ClientID , const char *pCmd, const char *pDesc = "\0", int TempInt = -1, int TempInt2 = -1);
	void AVL(int ClientID, const char *pCmd, const char *pText, ...);
	void AVH(int ClientID, int HiddenID, const char *pText, ...);
	void AVM(int ClientID, const char *pCmd, int TempInt, int HiddenID, const char* pText, ...);
	void AVD(int ClientID, const char *pCmd, int TempInt, int TempInt2, int HiddenID, const char *pText, ...);

private:
	void ClearVotes(int ClientID);
	void ShowVotesNewbieInformation(int ClientID);

public:
	void StartCustomVotes(int ClientID, int LastVoteMenu);
	void EndCustomVotes(int ClientID);
	void UpdateVotes(int ClientID, int MenuList);
	void StrongUpdateVotes(int ClientID, int MenuList);
	void StrongUpdateVotesForAll(int MenuList);
	void AddVotesBackpage(int ClientID);
	void ShowVotesPlayerStats(CPlayer *pPlayer);
	void ShowVotesItemValueInformation(CPlayer *pPlayer, ItemIdentifier ItemID = itGold);
	bool ParsingVoteCommands(int ClientID, const char *CMD, int VoteID, int VoteID2, int Get, const char *Text);

	/* #########################################################################
		MMO GAMECONTEXT
	######################################################################### */
	int CreateBot(short BotType, int BotID, int SubID);
	void CreateText(CEntity* pParent, bool Follow, vec2 Pos, vec2 Vel, int Lifespan, const char* pText);
	void CreateParticleExperience(vec2 Pos, int ClientID, int Experience, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropBonuses(vec2 Pos, int Type, int Value, int NumDrop = 1, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropItem(vec2 Pos, int ClientID, CItem DropItem, vec2 Force = vec2(0.0f, 0.0f));
	void CreateRandomDropItem(vec2 Pos, int ClientID, float Chance, CItem DropItem, vec2 Force = vec2(0.0f, 0.0f));
	bool TakeItemCharacter(int ClientID);
	void SendInbox(const char* pFrom, CPlayer *pPlayer, const char* Name, const char* Desc, ItemIdentifier ItemID = -1, int Value = -1, int Enchant = -1);
	void SendInbox(const char* pFrom, int AccountID, const char* Name, const char* Desc, ItemIdentifier ItemID = -1, int Value = -1, int Enchant = -1);

private:
	void SendDayInfo(int ClientID);

public:
	int GetWorldID() const { return m_WorldID; }
	int GetDungeonID() const { return m_DungeonID; }
	bool IsDungeon() const { return (m_DungeonID > 0); }
	int GetExperienceMultiplier(int Experience) const;
	bool IsPlayerEqualWorld(int ClientID, int WorldID = -1) const;
	bool IsAllowedPVP() const { return m_AllowedPVP; }

	bool IsPlayersNearby(vec2 Pos, float Distance) const;
	int GetRespawnWorld() const { return m_RespawnWorldID; }

private:
	void InitZones();

	bool m_AllowedPVP;
	int m_DayEnumType;
	static int m_MultiplierExp;
};

inline int64 CmaskAll() { return -1; }
inline int64 CmaskOne(int ClientID) { return (int64)1<<ClientID; }
inline int64 CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64 Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }

#endif
