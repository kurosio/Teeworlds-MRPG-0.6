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
	class CWarehouseCore *m_pWarehouse;
	class GuildCore* m_pGuildJob;
	class CCraftCore* m_pCraftJob;
	class DungeonCore* m_pDungeonJob;
	class CHouseCore* m_pHouseJob;
	class CMailBoxCore* m_pMailBoxJob;
	class CSkillsCore* m_pSkillJob;
	class CWorldDataCore* m_pWorldSwapJob;

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
	CWarehouseCore *Warehouse() const { return m_pWarehouse; }

	CCraftCore* Craft() const { return m_pCraftJob; }
	DungeonCore* Dungeon() const { return m_pDungeonJob; }
	CHouseCore* House() const { return m_pHouseJob; }
	CMailBoxCore* Inbox() const { return m_pMailBoxJob; }
	GuildCore* Member() const { return m_pGuildJob; }
	CSkillsCore* Skills() const { return m_pSkillJob; }
	CWorldDataCore *WorldSwap() const { return m_pWorldSwapJob; }

	// global systems
	void OnTick();
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

private:
	void ShowTopList(CPlayer* pPlayer, int TypeID) const;
};

#endif