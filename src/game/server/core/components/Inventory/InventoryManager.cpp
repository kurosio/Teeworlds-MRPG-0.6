/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "InventoryManager.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/houses/house_manager.h>
#include <game/server/core/components/Quests/QuestManager.h>

template < typename T >
void ExecuteTemplateItemsTypes(T Type, std::map < int, CPlayerItem >& paItems, const std::function<void(const CPlayerItem&)> pFunc)
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
			pFunc(ItemData);
	}
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

		CItemDescription(ID).Init(Name, Description, Type, Dysenthis, InitialPrice, Function, aContainerAttributes, std::move(Data));
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

void CInventoryManager::OnPlayerLogin(CPlayer* pPlayer)
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

void CInventoryManager::OnClientReset(int ClientID)
{
	CPlayerItem::Data().erase(ClientID);
}

bool CInventoryManager::OnPlayerMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MenuList::MENU_INVENTORY)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		VoteWrapper VInventoryInfo(ClientID, VWF_SEPARATE_OPEN, "Inventory Information");
		VInventoryInfo.Add("Choose the type of items you want to show");
		VInventoryInfo.Add("After, need select item to interact");
		VInventoryInfo.AddLine();

		VoteWrapper VInventoryTabs(ClientID, VWF_SEPARATE_OPEN, "\u262A Inventory tabs");
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_USED, "\u270C Used ({})", GetCountItemsType(pPlayer, ItemType::TYPE_USED));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_CRAFT, "\u2692 Craft ({})", GetCountItemsType(pPlayer, ItemType::TYPE_CRAFT));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_EQUIP, "\u26B0 Equipment ({})", GetCountItemsType(pPlayer, ItemType::TYPE_EQUIP));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_MODULE, "\u2693 Modules ({})", GetCountItemsType(pPlayer, ItemType::TYPE_MODULE));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_POTION, "\u26B1 Potion ({})", GetCountItemsType(pPlayer, ItemType::TYPE_POTION));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_OTHER, "\u26C3 Other ({})", GetCountItemsType(pPlayer, ItemType::TYPE_OTHER));
		VInventoryTabs.AddLine();

		if(pPlayer->m_VotesData.GetExtraID() >= 0)
			ListInventory(ClientID, (ItemType)pPlayer->m_VotesData.GetExtraID());

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MenuList::MENU_EQUIPMENT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		VoteWrapper VEquipInfo(ClientID, VWF_SEPARATE_OPEN, "\u2604 Equipment Information");
		VEquipInfo.Add("Select the type of equipment you want to show");
		VEquipInfo.Add("After, need select item to interact");
		VEquipInfo.AddLine();

		VoteWrapper VEquipTabs(ClientID, VWF_SEPARATE_OPEN, "\u2604 Equipment tabs");
		const char* paTypeNames[NUM_EQUIPPED] = { "Hammer", "Gun", "Shotgun", "Grenade", "Rifle", "Pickaxe", "Rake", "Armor", "Eidolon" };
		for(int i = 0; i < NUM_EQUIPPED; i++)
		{
			const auto ItemID = pPlayer->GetEquippedItemID((ItemFunctional)i);
			if(!ItemID.has_value() || !pPlayer->GetItem(ItemID.value())->IsEquipped())
			{
				VEquipTabs.AddMenu(MENU_EQUIPMENT, i, "{} not equipped", paTypeNames[i]);
				continue;
			}

			CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID.value());
			std::string strAttributes = pPlayerItem->Info()->HasAttributes() ? pPlayer->GetItem(ItemID.value())->GetStringAttributesInfo(pPlayer) : "unattributed";
			VEquipTabs.AddMenu(MENU_EQUIPMENT, i, "{} * {}", paTypeNames[i], strAttributes.c_str());
		}

		// show and sort equipment
		if(pPlayer->m_VotesData.GetExtraID() > 0)
			ListInventory(ClientID, (ItemFunctional)pPlayer->m_VotesData.GetExtraID());

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CInventoryManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
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

		GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "You drop {}x{}", pPlayerItem->Info()->GetName(), Get);
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
			GS()->Chat(ClientID, "Disassemble {}x{}.", pPlayerSelectedItem->Info()->GetName(), Get);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}

	if(PPSTR(CMD, "EQUIP_ITEM") == 0)
	{
		pPlayer->GetItem(VoteID)->Equip();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(CMD, "ENCHANT_ITEM") == 0)
	{
		// check enchant max level
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		if(pPlayerItem->IsEnchantMaxLevel())
		{
			GS()->Chat(ClientID, "Enchantment level is maximum");
			return true;
		}

		// try to spend material
		const int Price = pPlayerItem->GetEnchantPrice();
		if(!pPlayer->Account()->SpendCurrency(Price, itMaterial))
			return true;

		// enchant new level
		const int EnchantLevel = pPlayerItem->GetEnchant() + 1;
		pPlayerItem->SetEnchant(EnchantLevel);

		// information
		std::string strNewAttributes = pPlayerItem->Info()->HasAttributes() ? pPlayerItem->GetStringAttributesInfo(pPlayer) : "unattributed";
		GS()->Chat(-1, "{} enchant {} {} {}", Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(), pPlayerItem->GetStringEnchantLevel(), strNewAttributes);
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

std::vector<int> CInventoryManager::GetItemIDsCollection(ItemType Type)
{
	std::vector<int> ItemIDs {};

	for(const auto& [ID, pInfo] : CItemDescription::Data())
	{
		if(pInfo.IsType(Type))
			ItemIDs.push_back(ID);
	}

	return ItemIDs;
}

std::vector<int> CInventoryManager::GetItemIDsCollectionByFunction(ItemFunctional Type)
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
	ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem)
	{
		ItemSelected(GS()->GetPlayer(ClientID), &pItem);
	});
}

void CInventoryManager::ListInventory(int ClientID, ItemFunctional Type)
{
	ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem)
	{
		ItemSelected(GS()->GetPlayer(ClientID), &pItem);
	});
}

int CInventoryManager::GetUnfrozenItemValue(CPlayer* pPlayer, ItemIdentifier ItemID) const
{
	const int AvailableValue = Core()->QuestManager()->GetUnfrozenItemValue(pPlayer, ItemID);
	if(AvailableValue <= 0 && pPlayer->GetItem(ItemID)->HasItem())
	{
		GS()->Chat(pPlayer->GetCID(), "'{}' frozen for some quest.", pPlayer->GetItem(ItemID)->Info()->GetName());
		GS()->Chat(pPlayer->GetCID(), "In the 'Adventure Journal', you can see in which quest an item is used", pPlayer->GetItem(ItemID)->Info()->GetName());
	}
	return AvailableValue;
}

void CInventoryManager::ShowSellingItemsByFunction(CPlayer* pPlayer, ItemFunctional Type) const
{
	const int ClientID = pPlayer->GetCID();

	// show base shop functions
	VoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Selling item's");
	VInfo.Add("You can sell items from the list");
	VInfo.AddLine();

	VoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Sale of items from the list is available!");
	VItems.Add("Choose the item you want to sell");
	{
		VItems.BeginDepth();
		for(auto& [ID, Item] : CItemDescription::Data())
		{
			if(Item.GetFunctional() != Type)
				continue;

			int Price = maximum(1, Item.GetInitialPrice());
			VItems.AddOption("SELL_ITEM", ID, Price, "[{}] Sell {} ({} gold's per unit)", pPlayer->GetItem(ID)->GetValue(), Item.GetName(), Price);
		}
		VItems.EndDepth();
	}
	VItems.AddLine();
}

void CInventoryManager::ItemSelected(CPlayer* pPlayer, const CPlayerItem* pItem)
{
	const int ClientID = pPlayer->GetCID();
	const ItemIdentifier ItemID = pItem->GetID();
	CItemDescription* pInfo = pItem->Info();

	VoteWrapper VItem(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE);

	// name description
	if(pInfo->IsEnchantable())
	{
		VItem.SetTitle("{}{} {}", (pItem->m_Settings ? "✔ " : "\0"), pInfo->GetName(), pItem->GetStringEnchantLevel().c_str());
		if(pInfo->HasAttributes())
			VItem.Add("{}", pItem->GetStringAttributesInfo(pPlayer));
	}
	else
	{
		VItem.SetTitle("{}{} x{}", (pItem->m_Settings ? "✔ " : "\0"), pInfo->GetName(), pItem->m_Value);
	}

	// show description
	if(pPlayer->GetItem(itShowEquipmentDescription)->IsEquipped())
		VItem.Add("{}", pInfo->GetDescription());

	// is used item
	if(pInfo->m_Function == FUNCTION_ONE_USED || pInfo->m_Function == FUNCTION_USED)
		VItem.AddOption("IUSE", ItemID, "Use");

	// is potion
	if(pInfo->m_Type == ItemType::TYPE_POTION)
		VItem.AddOption("EQUIP_ITEM", ItemID, "Auto use - {}", (pItem->m_Settings ? "Enable" : "Disable"));

	// is decoration
	if(pInfo->m_Type == ItemType::TYPE_DECORATION)
	{
		VItem.AddOption("DECORATION_HOUSE_ADD", ItemID, "Start drawing near house");
		VItem.AddOption("GUILD_HOUSE_DECORATION", ItemID, "Start drawing near guild house");
	}

	// is equipped
	if(pInfo->m_Type == ItemType::TYPE_EQUIP || pInfo->m_Function == FUNCTION_SETTINGS)
	{
		if(pInfo->m_Function == EQUIP_HAMMER && pItem->IsEquipped())
			VItem.Add("You can not undress equipping hammer");
		else
			VItem.AddOption("EQUIP_ITEM", ItemID, (pItem->m_Settings ? "Undress" : "Equip"));
	}

	// is enchantable
	if(pInfo->IsEnchantable() && !pItem->IsEnchantMaxLevel())
	{
		const int Price = pItem->GetEnchantPrice();
		VItem.AddOption("ENCHANT_ITEM", ItemID, "Enchant ({}m)", Price);
	}

	// not allowed drop equipped hammer
	if(ItemID != pPlayer->GetEquippedItemID(EQUIP_HAMMER))
	{
		// can dysenthis
		if(pItem->GetDysenthis() > 0)
			VItem.AddOption("IDESYNTHESIS", ItemID, pItem->GetDysenthis(), "Disassemble (+{}m)", pItem->GetDysenthis());

		// can trade
		if(pInfo->m_InitialPrice > 0)
			VItem.AddOption("AUCTION_CREATE", ItemID, "Sell at auction");

		// drop
		VItem.AddOption("IDROP", ItemID, "Drop");
	}
}

int CInventoryManager::GetCountItemsType(CPlayer* pPlayer, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();
	return (int)std::count_if(CPlayerItem::Data()[ClientID].cbegin(), CPlayerItem::Data()[ClientID].cend(), [Type](auto pItem)
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