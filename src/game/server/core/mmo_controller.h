/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_CONTROLLER_H
#define GAME_SERVER_CORE_MMO_CONTROLLER_H

/*
	At the end, distribute the components in CMmoController.cpp
	And distribute where they are required
	This will affect the size of the output file
*/
#include "mmo_component.h"

class CMmoController
{
	class CStack
	{
	public:
		void add(class MmoComponent *pComponent)
		{
			m_vComponents.push_back(pComponent);
		}

		void free()
		{
			for(auto* pComponent : m_vComponents)
			{
				delete pComponent;
			}
			m_vComponents.clear();
			m_vComponents.shrink_to_fit();
		}

		std::vector< MmoComponent* > m_vComponents;
	};
	CStack m_System;

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
	class CDungeonManager* m_pDungeonManager;
	class CHouseManager* m_pHouseManager;
	class CMailboxManager* m_pMailboxManager;
	class CSkillManager* m_pSkillManager;
	class CTutorialManager* m_pTutorialManager;
	class CWorldManager* m_pWorldManager;
	class CEidolonManager* m_pEidolonManager;

public:
	explicit CMmoController(CGS *pGameServer);
	~CMmoController();

	CGS *m_pGameServer;
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
	CTutorialManager* TutorialManager() const { return m_pTutorialManager; }

	CCraftManager* CraftManager() const { return m_pCraftManager; }
	CDungeonManager* DungeonManager() const { return m_pDungeonManager; }
	CHouseManager* HouseManager() const { return m_pHouseManager; }
	CMailboxManager* MailboxManager() const { return m_pMailboxManager; }
	CGuildManager* GuildManager() const { return m_pGuildManager; }
	CGroupManager* GroupManager() const { return m_pGroupManager; }
	CSkillManager* SkillManager() const { return m_pSkillManager; }
	CWorldManager *WorldManager() const { return m_pWorldManager; }

	// global systems
	void OnTick();
	bool OnMessage(int MsgID, void* pRawMsg, int ClientID);
	bool OnPlayerHandleTile(CCharacter *pChr, int IndexCollision);
	bool OnPlayerHandleMainMenu(int ClientID, int Menulist);
	void OnInitAccount(int ClientID);
	bool OnParsingVoteCommands(CPlayer *pPlayer, const char *CMD, int VoteID, int VoteID2, int Get, const char *GetText);
	void ResetClientData(int ClientID);
	void HandleTimePeriod() const;
	void HandlePlayerTimePeriod(CPlayer* pPlayer);

	static void AsyncClientEnterMsgInfo(std::string ClientName, int ClientID);
	void ConAsyncLinesForTranslate();
	//
	void LoadLogicWorld() const;
	void SaveAccount(CPlayer *pPlayer, int Table) const;
	void ShowLoadingProgress(const char* pLoading, size_t Size) const;
	void ShowTopList(int ClientID, ToplistType Type, int Rows, class VoteWrapper* pWrapper = nullptr) const;
};

#endif