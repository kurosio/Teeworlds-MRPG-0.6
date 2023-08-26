/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCONTROLLER_H
#define GAME_SERVER_MMOCONTROLLER_H

/*
	At the end, distribute the components in MmoController.cpp
	And distribute where they are required
	This will affect the size of the output file
*/
#include "MmoComponent.h"

class MmoController
{
	class CStack
	{
	public:
		void add(class MmoComponent *pComponent)
		{
 			m_paComponents.push_back(pComponent);
		}

		void free()
		{
			for(auto* pComponent : m_paComponents)
				delete pComponent;
			m_paComponents.clear();
		}

		std::list < class MmoComponent *> m_paComponents;
	};
	CStack m_Components;

	class CAccountManager*m_pAccMain;
	class CBotManager *m_pBotsInfo;
	class CInventoryManager *m_pItemWork;
	class CAccountMinerManager *m_pAccMiner;
	class CAccountPlantManager *m_pAccPlant;
	class CQuestManager *m_pQuest;
	class CWarehouseManager *m_pWarehouse;
	class CGuildManager* m_pGuild;
	class CCraftManager* m_pCraft;
	class CDungeonManager* m_pDungeon;
	class CHouseManager* m_pHouse;
	class CMailBoxManager* m_pMailBox;
	class CSkillManager* m_pSkill;
	class CTutorialManager* m_pTutorial;
	class CWorldManager* m_pWorldSwap;
	class CEidolonManager* m_pEidolon;

public:
	explicit MmoController(CGS *pGameServer);
	~MmoController();

	CGS *m_pGameServer;
	CGS *GS() const { return m_pGameServer; }

	CAccountManager *Account() const { return m_pAccMain; }
	CBotManager *BotsData() const { return m_pBotsInfo; }
	CInventoryManager *Item() const { return m_pItemWork; }
	CAccountMinerManager *MinerAcc() const { return m_pAccMiner; }
	CAccountPlantManager *PlantsAcc() const { return m_pAccPlant; }
	CQuestManager *Quest() const { return m_pQuest; }
	CWarehouseManager *Warehouse() const { return m_pWarehouse; }
	CEidolonManager* Eidolons() const { return m_pEidolon; }
	CTutorialManager* Tutorial() const { return m_pTutorial; }

	CCraftManager* Craft() const { return m_pCraft; }
	CDungeonManager* Dungeon() const { return m_pDungeon; }
	CHouseManager* House() const { return m_pHouse; }
	CMailBoxManager* Inbox() const { return m_pMailBox; }
	CGuildManager* Member() const { return m_pGuild; }
	CSkillManager* Skills() const { return m_pSkill; }
	CWorldManager *WorldSwap() const { return m_pWorldSwap; }

	// global systems
	void OnTick();
	bool OnMessage(int MsgID, void* pRawMsg, int ClientID);
	bool OnPlayerHandleTile(CCharacter *pChr, int IndexCollision);
	bool OnPlayerHandleMainMenu(int ClientID, int Menulist);
	void OnInitAccount(int ClientID);
	bool OnParsingVoteCommands(CPlayer *pPlayer, const char *CMD, int VoteID, int VoteID2, int Get, const char *GetText);
	void ResetClientData(int ClientID);

	static void AsyncClientEnterMsgInfo(std::string ClientName, int ClientID);
	void ConAsyncLinesForTranslate();
	//
	void LoadLogicWorld() const;
	static const char* PlayerName(int AccountID);
	void SaveAccount(CPlayer *pPlayer, int Table) const;
	void ShowLoadingProgress(const char* pLoading, int Size) const;
	void ShowTopList(int ClientID, ToplistType Type, bool ChatGlobalMode, int Limit) const;
};

#endif