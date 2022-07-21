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

	class CAccountCore*m_pAccMain;
	class CBotCore *m_pBotsInfo;
	class CInventoryCore *m_pItemWork;
	class CAccountMinerCore *m_pAccMiner;
	class CAccountPlantCore *m_pAccPlant;
	class QuestCore *m_pQuest;
	class CShopCore *m_pShopmail;
	class CStorageCore *m_pStorageWork;
	class GuildCore* m_pGuildJob;
	class CCraftCore* m_pCraftJob;
	class DungeonCore* m_pDungeonJob;
	class CHouseCore* m_pHouseJob;
	class CMailBoxCore* m_pMailBoxJob;
	class CSkillsCore* m_pSkillJob;
	class CWorldSwapCore* m_pWorldSwapJob;

public:
	explicit MmoController(CGS *pGameServer);
	~MmoController();

	CGS *m_pGameServer;
	CGS *GS() const { return m_pGameServer; }

	CAccountCore *Account() const { return m_pAccMain; }
	CBotCore *BotsData() const { return m_pBotsInfo; }
	CInventoryCore *Item() const { return m_pItemWork; }
	CAccountMinerCore *MinerAcc() const { return m_pAccMiner; }
	CAccountPlantCore *PlantsAcc() const { return m_pAccPlant; }
	QuestCore *Quest() const { return m_pQuest; }
	CShopCore *Auction() const { return m_pShopmail; }
	CStorageCore *Storage() const { return m_pStorageWork; }

	CCraftCore* Craft() const { return m_pCraftJob; }
	DungeonCore* Dungeon() const { return m_pDungeonJob; }
	CHouseCore* House() const { return m_pHouseJob; }
	CMailBoxCore* Inbox() const { return m_pMailBoxJob; }
	GuildCore* Member() const { return m_pGuildJob; }
	CSkillsCore* Skills() const { return m_pSkillJob; }
	CWorldSwapCore *WorldSwap() const { return m_pWorldSwapJob; }

	// global systems
	void OnTick();
	bool OnPlayerHandleTile(CCharacter *pChr, int IndexCollision);
	bool OnPlayerHandleMainMenu(int ClientID, int Menulist, bool ReplaceMenu);
	void OnInitAccount(int ClientID);
	void OnMessage(int MsgID, void *pRawMsg, int ClientID);
	bool OnParsingVoteCommands(CPlayer *pPlayer, const char *CMD, int VoteID, int VoteID2, int Get, const char *GetText);
	void ResetClientData(int ClientID);
	void PrepareInformation(class IStorageEngine* pStorage);

	void ConSyncLinesForTranslate();
	//
	void LoadLogicWorld() const;
	static const char* PlayerName(int AccountID);
	void SaveAccount(CPlayer *pPlayer, int Table) const;
	void ShowLoadingProgress(const char* pLoading, int Size) const;
	void ShowTopList(CPlayer* pPlayer, int TypeID) const;
};

#endif