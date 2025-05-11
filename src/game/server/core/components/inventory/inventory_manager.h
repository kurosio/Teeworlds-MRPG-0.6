/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "item_data.h"

class CInventoryManager : public MmoComponent
{
	~CInventoryManager() override
	{
		// free data
		mystd::freeContainer(CAttributeDescription::Data(), CItemDescription::Data(), CPlayerItem::Data());
	}

	void OnPreInit() override;
	void OnPlayerLogin(class CPlayer* pPlayer) override;
	void OnClientReset(int ClientID) override;
	bool OnPlayerVoteCommand(class CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnSendMenuVotes(class CPlayer* pPlayer, int Menulist) override;

public:
	static std::vector<int> GetItemsCollection(std::optional<ItemGroup> optGroup, std::optional<ItemType> optType);
	static std::vector<int> GetItemIDsCollectionByType(ItemType Type);

	// primary
	bool ListInventory(int ClientID, std::optional<ItemGroup> GroupOpt, std::optional<ItemType> TypeOpt);
	void ItemSelected(CPlayer* pPlayer, const CPlayerItem* pItem);
	int GetUnfrozenItemValue(class CPlayer* pPlayer, ItemIdentifier ItemID) const;
	CPlayerItem* GetBestEquipmentSlotItem(CPlayer* pPlayer, ItemType Type);

	void RepairDurabilityItems(class CPlayer *pPlayer);
	int GetCountItemsType(CPlayer* pPlayer, std::optional<ItemGroup> GroupOpt, std::optional<ItemType> TypeOpt) const;

private:
	void ShowPlayerInventory(CPlayer* pPlayer);
	void ShowPlayerModules(CPlayer* pPlayer);
};

#endif