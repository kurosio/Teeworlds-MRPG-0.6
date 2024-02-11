/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "InventoryManager.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Houses/HouseManager.h>
#include <game/server/core/components/Quests/QuestManager.h>

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
void CInventoryManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_items_list");
	while(pRes->next())
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
		for(int i = 0; i < MAX_ATTRIBUTES_FOR_ITEM; i++)
		{
			char aAttributeID[32], aAttributeValue[32];
			str_format(aAttributeID, sizeof(aAttributeID), "Attribute%d", i);
			str_format(aAttributeValue, sizeof(aAttributeValue), "AttributeValue%d", i);

			AttributeIdentifier AttributeID = (AttributeIdentifier)pRes->getInt(aAttributeID);
			int AttributeValue = pRes->getInt(aAttributeValue);
			if(AttributeID >= AttributeIdentifier::SpreadShotgun && AttributeValue > 0)
			{
				aContainerAttributes.push_back({ AttributeID, AttributeValue });
			}
		}

		CItemDescription(ID).Init(Name, Description, Type, Dysenthis, InitialPrice, Function, aContainerAttributes, Data);
	}

	ResultPtr pResAtt = Database->Execute<DB::SELECT>("*", "tw_attributs");
	while(pResAtt->next())
	{
		const AttributeIdentifier ID = (AttributeIdentifier)pResAtt->getInt("ID");
		std::string Name = pResAtt->getString("Name").c_str();
		std::string FieldName = pResAtt->getString("FieldName").c_str();
		int UpgradePrice = pResAtt->getInt("Price");
		AttributeGroup Group = (AttributeGroup)pResAtt->getInt("Group");

		CAttributeDescription::CreateElement(ID)->Init(Name, FieldName, UpgradePrice, Group);
	}
}

void CInventoryManager::OnInitAccount(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
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

void CInventoryManager::OnResetClient(int ClientID)
{
	CPlayerItem::Data().erase(ClientID);
}

bool CInventoryManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MenuList::MENU_INVENTORY)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		CVoteWrapper VInventoryInfo(ClientID, HIDE_DEFAULT_OPEN, "\u205C Inventory Information");
		VInventoryInfo.Add("Choose the type of items you want to show");
		VInventoryInfo.Add("After, need select item to interact");
		VInventoryInfo.AddLine();

		CVoteWrapper VInventoryTabs(ClientID, HIDE_DEFAULT_OPEN, "\u205C Inventory tabs");
		int SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_USED);
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_USED, "Used ({INT})", SizeItems);
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_CRAFT);
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_CRAFT, "Craft ({INT})", SizeItems);
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_EQUIP);
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_EQUIP, "Equipment ({INT})", SizeItems);
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_MODULE);
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_MODULE, "Modules ({INT})", SizeItems);
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_POTION);
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_POTION, "Potion ({INT})", SizeItems);
		SizeItems = GetCountItemsType(pPlayer, ItemType::TYPE_OTHER);
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_OTHER, "Other ({INT})", SizeItems);
		VInventoryTabs.AddLine();

		if(pPlayer->m_VotesData.GetMenuTemporaryInteger() >= 0)
			ListInventory(ClientID, (ItemType)pPlayer->m_VotesData.GetMenuTemporaryInteger());

		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MenuList::MENU_EQUIPMENT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		CVoteWrapper VEquipInfo(ClientID, HIDE_DEFAULT_OPEN, "\u2604 Equipment Information");
		VEquipInfo.Add("Select the type of equipment you want to show");
		VEquipInfo.Add("After, need select item to interact");
		VEquipInfo.AddLine();

		CVoteWrapper VEquipTabs(ClientID, HIDE_DEFAULT_OPEN, "\u2604 Equipment tabs");
		const char* paTypeNames[NUM_EQUIPPED] = { "Hammer", "Gun", "Shotgun", "Grenade", "Rifle", "Pickaxe", "Rake", "Armor", "Eidolon" };
		for(int i = 0; i < NUM_EQUIPPED; i++)
		{
			ItemIdentifier ItemID = pPlayer->GetEquippedItemID((ItemFunctional)i);
			if(ItemID <= 0 || !pPlayer->GetItem(ItemID)->IsEquipped())
			{
				VEquipTabs.AddMenu(MENU_EQUIPMENT, i, "{STR} not equipped", paTypeNames[i]);
				continue;
			}

			char aAttributes[128];
			pPlayer->GetItem(ItemID)->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
			VEquipTabs.AddMenu(MENU_EQUIPMENT, i, "{STR} * {STR}", paTypeNames[i], aAttributes);
		}

		// show and sort equipment
		if(pPlayer->m_VotesData.GetMenuTemporaryInteger() > 0)
			ListInventory(ClientID, (ItemFunctional)pPlayer->m_VotesData.GetMenuTemporaryInteger());

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

bool CInventoryManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(CMD, "IDROP") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = minimum(AvailableValue, Get);
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		pPlayerItem->Drop(Get);

		GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "You drop {STR}x{VAL}", pPlayerItem->Info()->GetName(), Get);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(CMD, "IUSE") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = minimum(AvailableValue, Get);
		pPlayer->GetItem(VoteID)->Use(Get);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(CMD, "IDESYNTHESIS") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = minimum(AvailableValue, Get);
		CPlayerItem* pPlayerSelectedItem = pPlayer->GetItem(VoteID);
		CPlayerItem* pPlayerMaterialItem = pPlayer->GetItem(itMaterial);
		const int DesValue = pPlayerSelectedItem->GetDysenthis() * Get;
		if(pPlayerSelectedItem->Remove(Get) && pPlayerMaterialItem->Add(DesValue))
		{
			GS()->Chat(ClientID, "Disassemble {STR}x{VAL}.", pPlayerSelectedItem->Info()->GetName(), Get);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}

	if(PPSTR(CMD, "ISETTINGS") == 0)
	{
		pPlayer->GetItem(VoteID)->Equip();
		pPlayer->m_VotesData.UpdateCurrentVotes();
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
		if(!pPlayer->Account()->SpendCurrency(Price, itMaterial))
			return true;

		const int EnchantLevel = pPlayerItem->GetEnchant() + 1;
		pPlayerItem->SetEnchant(EnchantLevel);

		char aAttributes[128];
		pPlayerItem->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
		GS()->Chat(-1, "{STR} enchant {STR} {STR} {STR}", Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(), pPlayerItem->StringEnchantLevel().c_str(), aAttributes);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	return false;
}

void CInventoryManager::RepairDurabilityItems(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	Database->Execute<DB::UPDATE>("tw_accounts_items", "Durability = '100' WHERE UserID = '%d'", pPlayer->Account()->GetID());
	for(auto& [ID, Item] : CPlayerItem::Data()[ClientID])
		Item.m_Durability = 100;
}

std::vector<int> CInventoryManager::GetItemIDsCollection(ItemType Type) const
{
	std::vector<int> ItemIDs {};

	for(const auto& [ID, pInfo] : CItemDescription::Data())
	{
		if(pInfo.IsType(Type))
			ItemIDs.push_back(ID);
	}

	return ItemIDs;
}

std::vector<int> CInventoryManager::GetItemIDsCollectionByFunction(ItemFunctional Type) const
{
	std::vector<int> ItemIDs {};

	for(const auto& [ID, pInfo] : CItemDescription::Data())
	{
		if(pInfo.IsFunctional(Type))
			ItemIDs.push_back(ID);
	}

	return ItemIDs;
}

void CInventoryManager::ListInventory(int ClientID, ItemType Type)
{
	if(Type >= ItemType::TYPE_USED && Type < ItemType::NUM_TYPES)
	{
		GS()->AV(ClientID, "null");
		if(!ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem){ ItemSelected(GS()->m_apPlayers[ClientID], pItem); }))
			GS()->AVL(ClientID, "null", "There are no items in this tab");
	}
}

void CInventoryManager::ListInventory(int ClientID, ItemFunctional Type)
{
	GS()->AV(ClientID, "null");
	if(!ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem){ ItemSelected(GS()->m_apPlayers[ClientID], pItem); }))
		GS()->AVL(ClientID, "null", "There are no items in this tab");
}

int CInventoryManager::GetUnfrozenItemValue(CPlayer* pPlayer, ItemIdentifier ItemID) const
{
	const int AvailableValue = Core()->QuestManager()->GetUnfrozenItemValue(pPlayer, ItemID);
	if(AvailableValue <= 0 && pPlayer->GetItem(ItemID)->HasItem())
	{
		GS()->Chat(pPlayer->GetCID(), "'{STR}' frozen for some quest.", pPlayer->GetItem(ItemID)->Info()->GetName());
		GS()->Chat(pPlayer->GetCID(), "In the 'Adventure Journal', you can see in which quest an item is used", pPlayer->GetItem(ItemID)->Info()->GetName());
	}
	return AvailableValue;
}

void CInventoryManager::ShowSellingItemsByFunction(CPlayer* pPlayer, ItemFunctional Type) const
{
	const int ClientID = pPlayer->GetCID();

	// show base shop functions
	GS()->AVH(ClientID, TAB_STORAGE, "Sale of items from the list is available!");
	GS()->AVM(ClientID, "REPAIR_ITEMS", NOPE, TAB_STORAGE, "Repair all items - FREE");

	// show currency
	GS()->AV(ClientID, "null");
	GS()->AddVoteItemValue(ClientID);
	GS()->AV(ClientID, "null");

	// show selling list
	GS()->AVL(ClientID, "null", "\u2747 Selling targeted items");
	for(auto& [ID, Item] : CItemDescription::Data())
	{
		if(Item.GetFunctional() != Type)
			continue;

		int Price = maximum(1, Item.GetInitialPrice());
		GS()->AVD(ClientID, "SELL_ITEM", ID, Price, NOPE, "[{VAL}] Sell {STR} ({VAL} gold's per unit)", pPlayer->GetItem(ID)->GetValue(), Item.GetName(), Price);
	}

	GS()->AV(ClientID, "null");
}

void CInventoryManager::ItemSelected(CPlayer* pPlayer, const CPlayerItem& pItemPlayer, bool Dress)
{
	const int ClientID = pPlayer->GetCID();
	const ItemIdentifier ItemID = pItemPlayer.GetID();
	const char* pNameItem = pItemPlayer.Info()->GetName();

	CVoteWrapper VItem(ClientID, HIDE_UNIQUE);

	// name description
	if(pItemPlayer.Info()->IsEnchantable())
	{
		VItem.SetTitle("{STR}{STR} {STR}", (pItemPlayer.m_Settings ? "✔ " : "\0"), pNameItem, pItemPlayer.StringEnchantLevel().c_str());

		char aAttributes[64];
		pItemPlayer.StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
		VItem.AddIf(aAttributes[0] != '\0', "{STR}", aAttributes);
	}
	else
	{
		VItem.SetTitle("{STR}{STR} x{VAL}", (pItemPlayer.m_Settings ? "✔ " : "\0"), pNameItem, pItemPlayer.m_Value);
	}
	VItem.AddIf(Dress && pPlayer->GetItem(itShowEquipmentDescription)->IsEquipped(), "{STR}", pItemPlayer.Info()->GetDescription());

	// is used item
	bool IsUsed = pItemPlayer.Info()->m_Function == FUNCTION_ONE_USED || pItemPlayer.Info()->m_Function == FUNCTION_USED;
	VItem.AddIfOption(IsUsed, "IUSE", ItemID, "Use");

	// is planting item
	bool IsPlantItem = pItemPlayer.Info()->m_Function == FUNCTION_PLANT;
	VItem.AddIfOption(IsPlantItem, "PLANTING_HOUSE_SET", ItemID, "To plant at home (0.06%)");

	// is potion
	bool IsPotion = pItemPlayer.Info()->m_Type == ItemType::TYPE_POTION;
	VItem.AddIfOption(IsPotion, "ISETTINGS", ItemID, "Auto use - {STR}", (pItemPlayer.m_Settings ? "Enable" : "Disable"));

	// is decoration
	bool IsDeco = pItemPlayer.Info()->m_Type == ItemType::TYPE_DECORATION;
	VItem.AddIfOption(IsDeco, "DECORATION_HOUSE_ADD", ItemID, "Start drawing near house");
	VItem.AddIfOption(IsDeco, "GUILD_HOUSE_DECORATION", ItemID, "Start drawing near guild house");

	// is equipped
	bool IsEquipped = pItemPlayer.Info()->m_Type == ItemType::TYPE_EQUIP || pItemPlayer.Info()->m_Function == FUNCTION_SETTINGS;
	if(IsEquipped)
	{
		if(pItemPlayer.Info()->m_Function == EQUIP_HAMMER && pItemPlayer.IsEquipped())
			VItem.Add("You can not undress equipping hammer");
		else
			VItem.AddOption("ISETTINGS", ItemID, (pItemPlayer.m_Settings ? "Undress" : "Equip"));
	}

	// is enchantable
	if(pItemPlayer.Info()->IsEnchantable() && !pItemPlayer.IsEnchantMaxLevel())
	{
		const int Price = pItemPlayer.GetEnchantPrice();
		VItem.AddOption("IENCHANT", ItemID, "Enchant ({VAL}m)", Price);
	}

	// not allowed drop equipped hammer
	if(ItemID != pPlayer->GetEquippedItemID(EQUIP_HAMMER))
	{
		VItem.AddIfOption(pItemPlayer.GetDysenthis() > 0, "IDESYNTHESIS", ItemID, pItemPlayer.GetDysenthis(), "Disassemble (+{VAL}m)");
		VItem.AddIfOption(pItemPlayer.Info()->m_InitialPrice > 0, "AUCTION_SLOT", ItemID, "Sell at auction");
		VItem.AddOption("IDROP", ItemID, "Drop");
	}
}

int CInventoryManager::GetCountItemsType(CPlayer* pPlayer, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();
	return (int)std::count_if(CPlayerItem::Data()[ClientID].begin(), CPlayerItem::Data()[ClientID].end(), [Type](auto& pItem)
	{
		return pItem.second.HasItem() && pItem.second.Info()->IsType(Type);
	});
}

// TODO: FIX IT (lock .. unlock)
std::mutex lock_sleep;
void CInventoryManager::AddItemSleep(int AccountID, ItemIdentifier ItemID, int Value, int Milliseconds)
{
	std::thread Thread([this, AccountID, ItemID, Value, Milliseconds]()
	{
		if(Milliseconds > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

		lock_sleep.lock();
		CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID);
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