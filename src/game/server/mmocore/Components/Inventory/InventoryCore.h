/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_INVENTORY_CORE_H
#define GAME_SERVER_COMPONENT_INVENTORY_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "ItemData.h"

class CInventoryCore : public MmoComponent
{
	~CInventoryCore() override
	{
		CItemDataInfo::ms_aItemsInfo.clear();
		CItemData::ms_aItems.clear();
	};

	void OnPrepareInformation(class IStorageEngine* pStorage, class CDataFileWriter* pDataFile) override;
	void OnInit() override;
	void OnInitAccount(class CPlayer* pPlayer) override;
	void OnResetClient(int ClientID) override;
	bool OnHandleVoteCommands(class CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(class CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

	int SecureCheck(class CPlayer *pPlayer, int ItemID, int Value, int Settings, int Enchant);
	int DeSecureCheck(class CPlayer *pPlayer, int ItemID, int Value, int Settings);

public:
	// primary
	void ListInventory(class CPlayer *pPlayer, int TypeList, bool SortedFunction = false);
	void ItemSelected(class CPlayer* pPlayer, const CItemData& pItemPlayer, bool Dress = false);
	int GetUnfrozenItemValue(class CPlayer* pPlayer, int ItemID) const;

	void RepairDurabilityItems(class CPlayer *pPlayer);
	int GetValueItemsType(class CPlayer* pPlayer, int Type) const;

	void AddItemSleep(int AccountID, int ItemID, int Value, int Milliseconds);
	int GiveItem(class CPlayer* pPlayer, int ItemID, int Value, int Settings, int Enchant);
	int RemoveItem(class CPlayer* pPlayer, int ItemID, int Value, int Settings);
};

#endif