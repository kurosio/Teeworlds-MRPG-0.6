/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_INVENTORY_CORE_H
#define GAME_SERVER_COMPONENT_INVENTORY_CORE_H
#include <game/server/core/mmo_component.h>

#include "ItemData.h"

class CInventoryManager : public MmoComponent
{
	~CInventoryManager() override
	{
		// free data
		mrpgstd::cleaning_free_container_data(CAttributeDescription::Data(), CItemDescription::Data(), CPlayerItem::Data());
	}

	void OnInit() override;
	void OnPlayerLogin(class CPlayer* pPlayer) override;
	void OnClientReset(int ClientID) override;
	bool OnPlayerVoteCommand(class CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnPlayerMenulist(class CPlayer* pPlayer, int Menulist) override;

public:
	static std::vector<int> GetItemIDsCollection(ItemType Type);
	static std::vector<int> GetItemIDsCollectionByFunction(ItemFunctional Type);

	// primary
	void ListInventory(int ClientID, ItemType Type);
	void ListInventory(int ClientID, ItemFunctional Type);
	void ItemSelected(CPlayer* pPlayer, const CPlayerItem* pItem);
	int GetUnfrozenItemValue(class CPlayer* pPlayer, ItemIdentifier ItemID) const;

	void ShowSellingItemsByFunction(CPlayer* pPlayer, ItemFunctional Type) const;

	void RepairDurabilityItems(class CPlayer *pPlayer);
	int GetCountItemsType(class CPlayer* pPlayer, ItemType Type) const;

	void AddItemSleep(int AccountID, ItemIdentifier ItemID, int Value, int Milliseconds);
};

#endif