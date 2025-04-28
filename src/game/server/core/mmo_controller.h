/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_CONTROLLER_H
#define GAME_SERVER_CORE_MMO_CONTROLLER_H

#include "mmo_component.h"

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

	// global systems
	void OnInit(IServer* pServer, IConsole* pConsole, IStorageEngine* pStorage);
	void OnConsoleInit(IConsole* pConsole) const;
	void OnTick() const;
	bool OnClientMessage(int MsgID, void* pRawMsg, int ClientID) const;
	void OnPlayerLogin(CPlayer* pPlayer) const;
	void OnCharacterTile(CCharacter* pChr) const;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) const;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) const;
	bool OnPlayerVoteCommand(CPlayer *pPlayer, const char *pCmd, int ExtraValue1, int ExtraValue2, int ReasonNumber, const char *pReason) const;
	bool OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) const;
	void OnResetClientData(int ClientID) const;
	void OnHandleGlobalTimePeriod() const;
	void OnHandlePlayerTimePeriod(CPlayer* pPlayer) const;

	static void AsyncClientEnterMsgInfo(std::string_view ClientName, int ClientID);
	void SyncLocalizations() const;
	void SaveAccount(CPlayer *pPlayer, int Table) const;

	struct TempTopData
	{
		std::string Name;
		std::map<std::string, BigInt> Data;
	};
	std::map<int, TempTopData> GetTopList(ToplistType Type, int Rows) const;
	std::map<int, TempTopData> GetDungeonTopList(int DungeonID, int Rows) const;
	void ShowTopList(class VoteWrapper* pWrapper, int ClientID, ToplistType Type, int Rows) const;
};

#endif