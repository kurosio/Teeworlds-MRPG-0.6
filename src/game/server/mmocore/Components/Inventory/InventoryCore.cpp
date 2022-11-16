/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "InventoryCore.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Houses/HouseCore.h>
#include <game/server/mmocore/Components/Quests/QuestCore.h>

template < typename T >
bool ExecuteTemplateItemsTypes(T Type, std::map < int, CPlayerItem >& paItems, const std::function<void(const CPlayerItem&)> pFunc)
{
	bool Found = false;
	for(const auto& [ItemID, ItemData] : paItems)
	{
		bool ActivateCallback = false;
		if constexpr(std::is_same_v<T, ItemType>)
			ActivateCallback = ItemData.HasItem() && ItemData.Info()->IsType(Type);
		else if constexpr(std::is_same_v<T, ItemFunctional>)
			ActivateCallback = ItemData.HasItem() && ItemData.Info()->IsFunctional(Type);

		if(ActivateCallback) 
		{
			pFunc(ItemData);
			Found = true;
		}
	}
	return Found;
}

using namespace sqlstr;
void CInventoryCore::OnInit()
{
	const auto InitItemsList = Database->Prepare<DB::SELECT>("*", "tw_items_list");
	InitItemsList->AtExecute([](ResultPtr pRes)
	{
		while (pRes->next())
		{
			const int ID = pRes->getInt("ID");
			std::string Name = pRes->getString("Name").c_str();
			std::string Description = pRes->getString("Description").c_str();
			std::string Data = pRes->getString("Data").c_str();
			ItemType Type = (ItemType)pRes->getInt("Type");
			ItemFunctional Function = (ItemFunctional)pRes->getInt("Function");
			int InitialPrice = pRes->getInt("InitialPrice");
			int Dysenthis = pRes->getInt("Desynthesis");

			CItemDescription::ContainerAttributes aContainerAttributes;
			for (int i = 0; i < STATS_MAX_FOR_ITEM; i++)
			{
				char aAttributeID[32], aAttributeValue[32];
				str_format(aAttributeID, sizeof(aAttributeID), "Attribute%d", i);
				str_format(aAttributeValue, sizeof(aAttributeValue), "AttributeValue%d", i);

				AttributeIdentifier AttributeID = (AttributeIdentifier)pRes->getInt(aAttributeID);
				int AttributeValue = pRes->getInt(aAttributeValue);
				aContainerAttributes.push_back({ AttributeID, AttributeValue });
			}

			CItemDescription(ID).Init(Name, Description, Type, Dysenthis, InitialPrice, Function, aContainerAttributes, Data);
		}
	});

	const auto InitAttributes = Database->Prepare<DB::SELECT>("*", "tw_attributs");
	InitAttributes->AtExecute([](ResultPtr pRes)
	{
		while (pRes->next())
		{
			const AttributeIdentifier ID = (AttributeIdentifier)pRes->getInt("ID");
			std::string Name = pRes->getString("Name").c_str();
			std::string FieldName = pRes->getString("FieldName").c_str();
			int UpgradePrice = pRes->getInt("Price");
			AttributeType Type = (AttributeType)pRes->getInt("Type");
			int Dividing = pRes->getInt("Divide");

			CAttributeDescription::CreateDataItem(ID)->Init(Name, FieldName, UpgradePrice, Type, Dividing);
		}
	});
}

void CInventoryCore::OnInitAccount(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		ItemIdentifier ItemID = pRes->getInt("ItemID");
		int Value = pRes->getInt("Value");
		int Settings = pRes->getInt("Settings");
		int Enchant = pRes->getInt("Enchant");
		int Durability = pRes->getInt("Durability");

		CPlayerItem(ItemID, ClientID).Init(Value, Enchant, Durability, Settings);
	}
}

void CInventoryCore::OnResetClient(int ClientID)
{
	CPlayerItem::Data().erase(ClientID);
}

bool CInventoryCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
		return false;

	if(Menulist == MenuList::MENU_INVENTORY)
	{
		pPlayer->m_LastVoteMenu = MenuList::MENU_MAIN;
		GS()->AVH(ClientID, TAB_INFO_INVENTORY, "Inventory Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_INVENTORY, "Choose the type of items you want to show");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_INVENTORY, "After, need select item to interact");
		GS()->AV(ClientID, "null");

		GS()->AVH(ClientID, TAB_INVENTORY_SELECT, "Inventory tabs");
		int SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_USED);
		GS()->AVM(ClientID, "SORTEDINVENTORY", (int)ItemType::TYPE_USED, TAB_INVENTORY_SELECT, "Used ({INT})", SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_CRAFT);
		GS()->AVM(ClientID, "SORTEDINVENTORY", (int)ItemType::TYPE_CRAFT, TAB_INVENTORY_SELECT, "Craft ({INT})", SizeItems);
		
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_EQUIP);
		GS()->AVM(ClientID, "SORTEDINVENTORY", (int)ItemType::TYPE_EQUIP, TAB_INVENTORY_SELECT, "Equipment ({INT})", SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_MODULE);
		GS()->AVM(ClientID, "SORTEDINVENTORY", (int)ItemType::TYPE_MODULE, TAB_INVENTORY_SELECT, "Modules ({INT})", SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_POTION);
		GS()->AVM(ClientID, "SORTEDINVENTORY", (int)ItemType::TYPE_POTION, TAB_INVENTORY_SELECT, "Potion ({INT})", SizeItems);

		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_OTHER);
		GS()->AVM(ClientID, "SORTEDINVENTORY", (int)ItemType::TYPE_OTHER, TAB_INVENTORY_SELECT, "Other ({INT})", SizeItems);

		if(pPlayer->m_aSortTabs[SORT_INVENTORY] >= 0)
			ListInventory(ClientID, (ItemType)pPlayer->m_aSortTabs[SORT_INVENTORY]);

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MenuList::MENU_EQUIPMENT)
	{
		pPlayer->m_LastVoteMenu = MenuList::MENU_MAIN;
		GS()->AVH(ClientID, TAB_INFO_EQUIP, "Equip / Armor Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_EQUIP, "SELECT tab and select armor.");
		GS()->AV(ClientID, "null");

		GS()->AVH(ClientID, TAB_EQUIP_SELECT, "Equip SELECT Slot");
		const char* paTypeNames[NUM_EQUIPPED] = { "Hammer", "Gun", "Shotgun", "Grenade", "Rifle", "Pickaxe", "Rake", "Armor", "Eidolon" };
		for(int i = 0; i < NUM_EQUIPPED; i++)
		{
			ItemIdentifier ItemID = pPlayer->GetEquippedItemID((ItemFunctional)i);
			if(ItemID <= 0 || !pPlayer->GetItem(ItemID)->IsEquipped())
			{
				GS()->AVM(ClientID, "SORTEDEQUIP", i, TAB_EQUIP_SELECT, "{STR} not equipped", paTypeNames[i]);
				continue;
			}

			char aAttributes[128];
			pPlayer->GetItem(ItemID)->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
			GS()->AVM(ClientID, "SORTEDEQUIP", i, TAB_EQUIP_SELECT, "{STR} * {STR}", pPlayer->GetItem(ItemID)->Info()->GetName(), aAttributes);
		}

		// show and sort equipment
		if(pPlayer->m_aSortTabs[SORT_EQUIPING] >= 0)
			ListInventory(ClientID, (ItemFunctional)pPlayer->m_aSortTabs[SORT_EQUIPING]);

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

bool CInventoryCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(CMD, "SORTEDINVENTORY") == 0)
	{
		pPlayer->m_aSortTabs[SORT_INVENTORY] = VoteID;
		GS()->StrongUpdateVotes(ClientID, MenuList::MENU_INVENTORY);
		return true;
	}

	if(PPSTR(CMD, "IDROP") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = min(AvailableValue, Get);
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		pPlayerItem->Drop(Get);

		GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "You drop {STR}x{VAL}", pPlayerItem->Info()->GetName(), Get);
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "IUSE") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = min(AvailableValue, Get);
		pPlayer->GetItem(VoteID)->Use(Get);
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "IDESYNTHESIS") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = min(AvailableValue, Get);
		CPlayerItem* pPlayerSelectedItem = pPlayer->GetItem(VoteID);
		CPlayerItem* pPlayerMaterialItem = pPlayer->GetItem(itMaterial);
		const int DesValue = pPlayerSelectedItem->Info()->m_Dysenthis * Get;
		if(pPlayerSelectedItem->Remove(Get) && pPlayerMaterialItem->Add(DesValue))
		{
			GS()->Chat(ClientID, "Disassemble {STR}x{VAL}.", pPlayerSelectedItem->Info()->GetName(), Get);
			GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	if(PPSTR(CMD, "ISETTINGS") == 0)
	{
		pPlayer->GetItem(VoteID)->Equip();
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "IENCHANT") == 0)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		if(pPlayerItem->IsEnchantMaxLevel())
		{
			GS()->Chat(ClientID, "Enchantment level is maximum");
			return true;
		}

		const int Price = pPlayerItem->GetEnchantPrice();
		if(!pPlayer->SpendCurrency(Price, itMaterial))
			return true;

		const int EnchantLevel = pPlayerItem->GetEnchant() + 1;
		pPlayerItem->SetEnchant(EnchantLevel);

		char aEnchantBuf[16];
		pPlayerItem->StrFormatEnchantLevel(aEnchantBuf, sizeof(aEnchantBuf));

		char aAttributes[128];
		pPlayerItem->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
		GS()->Chat(-1, "{STR} enchant {STR} {STR} {STR}", Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(), aEnchantBuf, aAttributes);
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "SORTEDEQUIP") == 0)
	{
		pPlayer->m_aSortTabs[SORT_EQUIPING] = VoteID;
		GS()->StrongUpdateVotes(ClientID, MenuList::MENU_EQUIPMENT);
		return true;
	}

	return false;
}

void CInventoryCore::RepairDurabilityItems(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	Database->Execute<DB::UPDATE>("tw_accounts_items", "Durability = '100' WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	for(auto& [ID, Item] : CPlayerItem::Data()[ClientID])
		Item.m_Durability = 100;
}

void CInventoryCore::ListInventory(int ClientID, ItemType Type)
{
	if(Type >= ItemType::TYPE_USED && Type < ItemType::NUM_TYPES)
	{
		GS()->AV(ClientID, "null");
		if(!ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem){ ItemSelected(GS()->m_apPlayers[ClientID], pItem); }))
			GS()->AVL(ClientID, "null", "There are no items in this tab");
	}
}

void CInventoryCore::ListInventory(int ClientID, ItemFunctional Type)
{
	GS()->AV(ClientID, "null");
	if(!ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem){ ItemSelected(GS()->m_apPlayers[ClientID], pItem); }))
		GS()->AVL(ClientID, "null", "There are no items in this tab");
}

int CInventoryCore::GiveItem(CPlayer *pPlayer, ItemIdentifier ItemID, int Value, int Settings, int Enchant)
{
	const int ClientID = pPlayer->GetCID();
	const int SecureID = SecureCheck(pPlayer, ItemID, Value, Settings, Enchant);
	if(SecureID == 1)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_items", "Value = '%d', Settings = '%d', Enchant = '%d' WHERE ItemID = '%d' AND UserID = '%d'",
		       CPlayerItem::Data()[ClientID][ItemID].m_Value, CPlayerItem::Data()[ClientID][ItemID].m_Settings, CPlayerItem::Data()[ClientID][ItemID].m_Enchant, ItemID, pPlayer->Acc().m_UserID);
	}
	return SecureID;
}

int CInventoryCore::SecureCheck(CPlayer *pPlayer, ItemIdentifier ItemID, int Value, int Settings, int Enchant)
{
	// check initialize and add the item
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("Value, Settings", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
	if(pRes->next())
	{
		CPlayerItem::Data()[ClientID][ItemID].m_Value = pRes->getInt("Value")+Value;
		CPlayerItem::Data()[ClientID][ItemID].m_Settings = pRes->getInt("Settings")+Settings;
		CPlayerItem::Data()[ClientID][ItemID].m_Enchant = Enchant;
		return 1;
	}

	// create an object if not found
	CPlayerItem::Data()[ClientID][ItemID].m_Value = Value;
	CPlayerItem::Data()[ClientID][ItemID].m_Settings = Settings;
	CPlayerItem::Data()[ClientID][ItemID].m_Enchant = Enchant;
	CPlayerItem::Data()[ClientID][ItemID].m_Durability = 100;
	Database->Execute<DB::INSERT>("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('%d', '%d', '%d', '%d', '%d')",
		ItemID, pPlayer->Acc().m_UserID, Value, Settings, Enchant);
	return 2;
}

int CInventoryCore::RemoveItem(CPlayer *pPlayer, ItemIdentifier ItemID, int Value, int Settings)
{
	const int SecureID = DeSecureCheck(pPlayer, ItemID, Value, Settings);
	if(SecureID == 1)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_items", "Value = Value - '%d', Settings = Settings - '%d' WHERE ItemID = '%d' AND UserID = '%d'",
			Value, Settings, ItemID, pPlayer->Acc().m_UserID);
	}
	return SecureID;
}

int CInventoryCore::DeSecureCheck(CPlayer *pPlayer, ItemIdentifier ItemID, int Value, int Settings)
{
	// we check the database
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("Value, Settings", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
	if(pRes->next())
	{
		// update if there is more
		if(pRes->getInt("Value") > Value)
		{
			CPlayerItem::Data()[ClientID][ItemID].m_Value = pRes->getInt("Value")-Value;
			CPlayerItem::Data()[ClientID][ItemID].m_Settings = pRes->getInt("Settings")-Settings;
			return 1;
		}

		// remove the object if it is less than the required amount
		CPlayerItem::Data()[ClientID][ItemID].m_Value = 0;
		CPlayerItem::Data()[ClientID][ItemID].m_Settings = 0;
		CPlayerItem::Data()[ClientID][ItemID].m_Enchant = 0;
		Database->Execute<DB::REMOVE>("tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
		return 2;
	}

	CPlayerItem::Data()[ClientID][ItemID].m_Value = 0;
	CPlayerItem::Data()[ClientID][ItemID].m_Settings = 0;
	CPlayerItem::Data()[ClientID][ItemID].m_Enchant = 0;
	return 0;
}

int CInventoryCore::GetUnfrozenItemValue(CPlayer *pPlayer, ItemIdentifier ItemID) const
{
	const int AvailableValue = Job()->Quest()->GetUnfrozenItemValue(pPlayer, ItemID);
	if(AvailableValue <= 0 && pPlayer->GetItem(ItemID)->HasItem())
	{
		GS()->Chat(pPlayer->GetCID(), "'{STR}' frozen for some quest.", pPlayer->GetItem(ItemID)->Info()->GetName());
		GS()->Chat(pPlayer->GetCID(), "In the 'Adventure Journal', you can see in which quest an item is used", pPlayer->GetItem(ItemID)->Info()->GetName());
	}
	return AvailableValue;
}

void CInventoryCore::ItemSelected(CPlayer* pPlayer, const CPlayerItem& pItemPlayer, bool Dress)
{
	const int ClientID = pPlayer->GetCID();
	const ItemIdentifier ItemID = pItemPlayer.GetID();
	const int HideID = NUM_TAB_MENU + ItemID;
	const char* pNameItem = pItemPlayer.Info()->GetName();

	// overwritten or not
	if (pItemPlayer.Info()->IsEnchantable())
	{
		char aEnchantBuf[16];
		pItemPlayer.StrFormatEnchantLevel(aEnchantBuf, sizeof(aEnchantBuf));
		GS()->AVH(ClientID, HideID, "{STR}{STR} {STR}", (pItemPlayer.m_Settings ? "✔ " : "\0"), pNameItem, (pItemPlayer.m_Enchant > 0 ? aEnchantBuf : "\0"));

		if(Dress && pPlayer->GetItem(itShowEquipmentDescription)->IsEquipped())
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pItemPlayer.Info()->GetDescription());

		char aAttributes[64];
		pItemPlayer.StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
	}
	else
	{
		GS()->AVH(ClientID, HideID, "{STR}{STR} x{VAL}", (pItemPlayer.m_Settings ? "✔ " : "\0"), pNameItem, pItemPlayer.m_Value);
		if(pItemPlayer.Info()->m_Type != ItemType::TYPE_CRAFT && pItemPlayer.Info()->m_Type != ItemType::TYPE_OTHER)
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pItemPlayer.Info()->GetDescription());
	}

	// functional by function
	if (pItemPlayer.Info()->m_Function == FUNCTION_ONE_USED || pItemPlayer.Info()->m_Function == FUNCTION_USED)
	{
		GS()->AVM(ClientID, "null", NOPE, HideID, "For bind command '/useitem {INT}'", ItemID);
		GS()->AVM(ClientID, "IUSE", ItemID, HideID, "Use {STR}", pNameItem);
	}
	else if(pItemPlayer.Info()->m_Function == FUNCTION_PLANTS)
	{
		const int HouseID = Job()->House()->OwnerHouseID(pPlayer->Acc().m_UserID);
		const ItemIdentifier PlantItemID = Job()->House()->GetPlantsID(HouseID);
		if(HouseID > 0 && PlantItemID != ItemID)
		{
			const int random_change = random_int() % 900;
			GS()->AVD(ClientID, "HOMEPLANTSET", ItemID, random_change, HideID, "To plant {STR}, to house (0.06%)", pNameItem);
		}
	}

	// functional by type
	if (pItemPlayer.Info()->m_Type == ItemType::TYPE_POTION)
	{
		GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, "Auto use {STR} - {STR}", pNameItem, (pItemPlayer.m_Settings ? "Enable" : "Disable"));

	}
	else if (pItemPlayer.Info()->m_Type == ItemType::TYPE_DECORATION)
	{
		GS()->AVM(ClientID, "DECOSTART", ItemID, HideID, "Add {STR} to your house", pNameItem);
		GS()->AVM(ClientID, "DECOGUILDSTART", ItemID, HideID, "Add {STR} to your guild house", pNameItem);
	}
	else if(pItemPlayer.Info()->m_Type == ItemType::TYPE_EQUIP || pItemPlayer.Info()->m_Function == FUNCTION_SETTINGS)
	{
		if((pItemPlayer.Info()->m_Function == EQUIP_HAMMER && pItemPlayer.IsEquipped()))
			GS()->AVM(ClientID, "null", NOPE, HideID, "You can not undress equipping hammer", pNameItem);
		else
			GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, "{STR} {STR}", (pItemPlayer.m_Settings ? "Undress" : "Equip"), pNameItem);
	}

	// house plant


	// enchant
	if (pItemPlayer.Info()->IsEnchantable() && !pItemPlayer.IsEnchantMaxLevel())
	{
		const int Price = pItemPlayer.GetEnchantPrice();
		GS()->AVM(ClientID, "IENCHANT", ItemID, HideID, "Enchant {STR} ({VAL} materials)", pNameItem, Price);
	}

	// not allowed drop equipped hammer
	if (ItemID != pPlayer->GetEquippedItemID(EQUIP_HAMMER))
	{
		// dysenthis
		if (pItemPlayer.Info()->GetDysenthis() > 0)
		{
			GS()->AVM(ClientID, "IDESYNTHESIS", ItemID, HideID, "Disassemble {STR} (+{VAL} materials)", pNameItem, pItemPlayer.Info()->GetDysenthis());
		}

		// drop
		GS()->AVM(ClientID, "IDROP", ItemID, HideID, "Drop {STR}", pNameItem);

		// auction
		if (pItemPlayer.Info()->m_InitialPrice > 0)
		{
			GS()->AVM(ClientID, "AUCTION_SLOT", ItemID, HideID, "Create Slot Auction {STR}", pNameItem);
		}
	}
}

int CInventoryCore::GetCountItemsType(CPlayer *pPlayer, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();
	return (int)std::count_if(CPlayerItem::Data()[ClientID].begin(), CPlayerItem::Data()[ClientID].end(), [Type](auto& pItem)
	{
		return pItem.second.HasItem() && pItem.second.Info()->IsType(Type);
	});
}

// TODO: FIX IT (lock .. unlock)
std::mutex lock_sleep;
void CInventoryCore::AddItemSleep(int AccountID, ItemIdentifier ItemID, int Value, int Milliseconds)
{
	std::thread Thread([this, AccountID, ItemID, Value, Milliseconds]()
	{
		if(Milliseconds > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

		lock_sleep.lock();
		CPlayer* pPlayer = GS()->GetPlayerFromUserID(AccountID);
		if(pPlayer)
		{
			pPlayer->GetItem(ItemID)->Add(Value);
			lock_sleep.unlock();
			return;
		}

		ResultPtr pRes = Database->Execute<DB::SELECT>("Value", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, AccountID);
		if(pRes->next())
		{
			const int ReallyValue = pRes->getInt("Value") + Value;
			Database->Execute<DB::UPDATE>("tw_accounts_items", "Value = '%d' WHERE UserID = '%d' AND ItemID = '%d'", ReallyValue, AccountID, ItemID);
			lock_sleep.unlock();
			return;
		}
		Database->Execute<DB::INSERT>("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('%d', '%d', '%d', '0', '0')", ItemID, AccountID, Value);
		lock_sleep.unlock();
	});
	Thread.detach();
}