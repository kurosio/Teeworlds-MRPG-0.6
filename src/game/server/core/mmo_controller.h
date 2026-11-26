/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_CONTROLLER_H
#define GAME_SERVER_CORE_MMO_CONTROLLER_H

#include "mmo_component.h"

#include <any>

class CMmoController
{
	CGS* m_pGameServer;
	MmoComponent::CStack m_System;

	class CAchievementManager* m_pAchievementManager;
	class CAccountManager* m_pAccountManager;
	class CBotManager* m_pBotManager;
	class CInventoryManager *m_pInventoryManager;
	class CAccountMiningManager *m_pAccountMiningManager;
	class CAccountFarmingManager *m_pAccountFarmingManager;
	class CQuestManager *m_pQuestManager;
	class CWarehouseManager *m_pWarehouseManager;
	class CGuildManager* m_pGuildManager;
	class CGroupManager* m_pGroupManager;
	class CCraftManager* m_pCraftManager;
	class CDutiesManager* m_pDutiesManager;
	class CHouseManager* m_pHouseManager;
	class CMailboxManager* m_pMailboxManager;
	class CSkillManager* m_pSkillManager;
	class CWorldManager* m_pWorldManager;
	class CEidolonManager* m_pEidolonManager;
	class CMiniEventsManager* m_pMiniEventsManager;

public:
	explicit CMmoController(CGS *pGameServer);

	CGS *GS() const { return m_pGameServer; }

	CAchievementManager* AchievementManager() const { return m_pAchievementManager; }
	CAccountManager* AccountManager() const { return m_pAccountManager; }
	CBotManager* BotManager() const { return m_pBotManager; }
	CInventoryManager* InventoryManager() const { return m_pInventoryManager; }
	CAccountMiningManager* AccountMiningManager() const { return m_pAccountMiningManager; }
	CAccountFarmingManager* AccountFarmingManager() const { return m_pAccountFarmingManager; }
	CQuestManager* QuestManager() const { return m_pQuestManager; }
	CWarehouseManager* WarehouseManager() const { return m_pWarehouseManager; }
	CEidolonManager* EidolonManager() const { return m_pEidolonManager; }
	CCraftManager* CraftManager() const { return m_pCraftManager; }
	CDutiesManager* DutiesManager() const { return m_pDutiesManager; }
	CHouseManager* HouseManager() const { return m_pHouseManager; }
	CMailboxManager* MailboxManager() const { return m_pMailboxManager; }
	CGuildManager* GuildManager() const { return m_pGuildManager; }
	CGroupManager* GroupManager() const { return m_pGroupManager; }
	CSkillManager* SkillManager() const { return m_pSkillManager; }
	CWorldManager *WorldManager() const { return m_pWorldManager; }
	CMiniEventsManager* MiniEventsManager() const { return m_pMiniEventsManager; }

	// global systems
	void OnInit(IServer* pServer, IConsole* pConsole, IStorageEngine* pStorage);
	void OnConsoleInit(IConsole* pConsole) const;
	void OnTick() const;
	bool OnClientMessage(int MsgID, void* pRawMsg, int ClientID) const;
	void RegisterAccountLoadStages(CPlayer* pPlayer) const;
	void OnPlayerLogin(CPlayer* pPlayer) const;
	void OnCharacterTile(CCharacter* pChr) const;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) const;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) const;
	bool OnPlayerVoteCommand(CPlayer *pPlayer, const char *pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char *pReason) const;
	bool OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) const;
	void OnResetClientData(int ClientID) const;
	void OnHandleGlobalTimePeriod() const;
	void ApplyGlobalTimePeriod(const std::string& Data) const;
	void OnHandlePlayerTimePeriod(CPlayer* pPlayer) const;

	static void AsyncClientEnterMsgInfo(std::string_view ClientName, int ClientID);
	void SyncLocalizationsAsync() const;
	void SyncLocalizations() const;
	int SyncLocalizationFiles() const;
	bool ReloadLocalizations() const;
	void SaveAccount(CPlayer *pPlayer, int Table) const;

	struct TempTopData
	{
		std::string m_Name;
		BigInt m_Value {};
		BigInt m_Extra {};
	};

	struct STopRows
	{
		std::map<int, TempTopData> m_Rows {};
		int m_Stamp {};
		bool m_Loaded {};
	};

	// Stable query tags for shared top-list reads.
	static std::string TopUpdateTag(ToplistType Type);
	static std::string DungeonUpdateTag(int DungeonID);
	static std::string RhythmUpdateTag(int WorldID, const std::string& Difficulty);

	const std::map<int, TempTopData>& GetTopList(ToplistType Type, int Rows) const;
	const std::map<int, TempTopData>& GetDungeonTopList(int DungeonID, int Rows) const;
	const std::map<int, TempTopData>& GetRhythmTopList(int WorldID, const std::string& Difficulty, int Rows) const;
	void ShowTopList(class VoteWrapper* pWrapper, int ClientID, ToplistType Type, int Rows) const;

private:
	mutable std::map<std::string, STopRows> m_TopRows;
	mutable bool m_TimePeriodPending = false;

	const std::map<int, TempTopData>& BuildTrackingTopList(ToplistType Type) const;

	// true when the cached rows have expired and should be refreshed
	bool NeedTopRows(const std::string& Key) const;
	const std::map<int, TempTopData>& TopRows(const std::string& Key) const
	{
		return m_TopRows[Key].m_Rows;
	}
	void SetTopRows(const std::string& Key, std::map<int, TempTopData>&& Rows) const;
};

#endif
