/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "InventoryManager.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/houses/house_manager.h>
#include <game/server/core/components/quests/quest_manager.h>

template < typename T >
void ExecuteTemplateItemsTypes(T Type, std::map < int, CPlayerItem >& paItems, const std::function<void(const CPlayerItem&)> pFunc)
{
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
void CInventoryManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_items_list");
	while(pRes->next())
	{
		const auto ItemID = pRes->getInt("ID");
		const auto Name = pRes->getString("Name");
		const auto Description = pRes->getString("Description");
		const auto Type = (ItemType)pRes->getInt("Type");
		const auto Function = (ItemFunctional)pRes->getInt("Function");
		const auto InitialPrice = pRes->getInt("InitialPrice");
		const auto Dysenthis = pRes->getInt("Desynthesis");
		auto Data = pRes->getString("Data");

		CItemDescription::ContainerAttributes aContainerAttributes;
		for(int i = 0; i < MAX_ATTRIBUTES_FOR_ITEM; i++)
		{
			char aAttributeID[32], aAttributeValue[32];
			str_format(aAttributeID, sizeof(aAttributeID), "Attribute%d", i);
			str_format(aAttributeValue, sizeof(aAttributeValue), "AttributeValue%d", i);

			const auto AttributeID = (AttributeIdentifier)pRes->getInt(aAttributeID);
			const auto AttributeValue = pRes->getInt(aAttributeValue);

			if(AttributeID >= AttributeIdentifier::DMG && AttributeValue > 0)
			{
				aContainerAttributes.emplace_back(AttributeID, AttributeValue);
			}
		}

		CItemDescription(ItemID).Init(Name, Description, Type, Dysenthis, InitialPrice, Function, aContainerAttributes, std::move(Data));
	}

	ResultPtr pResAtt = Database->Execute<DB::SELECT>("*", "tw_attributs");
	while(pResAtt->next())
	{
		const auto ID = (AttributeIdentifier)pResAtt->getInt("ID");
		const auto Name = pResAtt->getString("Name");
		const auto FieldName = pResAtt->getString("FieldName");
		const auto UpgradePrice = pResAtt->getInt("Price");
		const auto Group = (AttributeGroup)pResAtt->getInt("Group");

		CAttributeDescription::CreateElement(ID)->Init(Name, FieldName, UpgradePrice, Group);
	}
}

void CInventoryManager::OnPlayerLogin(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_items", "WHERE UserID = '{}'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		const auto ItemID = pRes->getInt("ItemID");
		const auto Value = pRes->getInt("Value");
		const auto Settings = pRes->getInt("Settings");
		const auto Enchant = pRes->getInt("Enchant");
		const auto Durability = pRes->getInt("Durability");

		CPlayerItem(ItemID, ClientID).Init(Value, Enchant, Durability, Settings);
	}
}

void CInventoryManager::OnClientReset(int ClientID)
{
	CPlayerItem::Data().erase(ClientID);
}

bool CInventoryManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_INVENTORY)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// inventory tabs
		VoteWrapper VInventoryTabs(ClientID, VWF_SEPARATE|VWF_ALIGN_TITLE|VWF_STYLE_SIMPLE, "\u262A Inventory tabs");
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_USED, "\u270C Used ({})", GetCountItemsType(pPlayer, ItemType::TYPE_USED));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_CRAFT, "\u2692 Craft ({})", GetCountItemsType(pPlayer, ItemType::TYPE_CRAFT));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_EQUIP, "\u26B0 Equipment ({})", GetCountItemsType(pPlayer, ItemType::TYPE_EQUIP));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_MODULE, "\u2693 Modules ({})", GetCountItemsType(pPlayer, ItemType::TYPE_MODULE));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_POTION, "\u26B1 Potion ({})", GetCountItemsType(pPlayer, ItemType::TYPE_POTION));
		VInventoryTabs.AddMenu(MENU_INVENTORY, (int)ItemType::TYPE_OTHER, "\u26C3 Other ({})", GetCountItemsType(pPlayer, ItemType::TYPE_OTHER));
		VoteWrapper::AddEmptyline(ClientID);

		// show and sort inventory
		const auto itemTypeSelected = pPlayer->m_VotesData.GetExtraID();
		VoteWrapper VInfo(ClientID);
		if(itemTypeSelected.has_value())
		{
			if(!ListInventory(ClientID, (ItemType)itemTypeSelected.value()))
				VInfo.Add("The selected list is empty");
		}
		else
		{
			VInfo.Add("Select the required tab");
		}

		// add backpage
		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_EQUIPMENT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// lambda equipment
		auto addEquipmentFieldFunc = [&](VoteWrapper& Wrapper, ItemFunctional EquipID) -> bool
		{
			// check if equipped
			const auto ItemID = pPlayer->GetEquippedItemID(EquipID);
			if(!ItemID.has_value() || !pPlayer->GetItem(ItemID.value())->IsEquipped())
			{
				Wrapper.AddMenu(MENU_EQUIPMENT, EquipID, "{} not equipped", ItemFunctionalString[EquipID]);
				return false;
			}

			// handle potion information
			const auto pPlayerItem = pPlayer->GetItem(ItemID.value());
			if(EquipID == EQUIP_POTION_HEAL || EquipID == EQUIP_POTION_MANA)
			{
				if(const auto optPotionContext = pPlayerItem->Info()->GetPotionContext())
				{
					const auto RecastTotal = optPotionContext->Lifetime + POTION_RECAST_APPEND_TIME;
					Wrapper.AddMenu(MENU_EQUIPMENT, EquipID, "{} (recast {} / +{}) x{}",
						pPlayerItem->Info()->GetName(), RecastTotal, optPotionContext->Value, pPlayerItem->GetValue());
				}
				else
				{
					Wrapper.AddMenu(MENU_EQUIPMENT, EquipID, "{} x{}", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
				}
				return true;
			}

			// handle other equipment
			const auto strAttributes = pPlayerItem->GetStringAttributesInfo(pPlayer);
			Wrapper.AddMenu(MENU_EQUIPMENT, EquipID, "{} / {}", pPlayerItem->Info()->GetName(), strAttributes);
			return true;
		};

		// weapons equipment
		VoteWrapper VWeapons(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: Weapons");
		addEquipmentFieldFunc(VWeapons, EQUIP_HAMMER);
		addEquipmentFieldFunc(VWeapons, EQUIP_GUN);
		addEquipmentFieldFunc(VWeapons, EQUIP_SHOTGUN);
		addEquipmentFieldFunc(VWeapons, EQUIP_GRENADE);
		addEquipmentFieldFunc(VWeapons, EQUIP_LASER);
		VoteWrapper::AddEmptyline(ClientID);

		// general equipment (tools and armor)
		VoteWrapper VEquipment(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: General");
		addEquipmentFieldFunc(VEquipment, EQUIP_EIDOLON);
		addEquipmentFieldFunc(VEquipment, EQUIP_ARMOR);
		addEquipmentFieldFunc(VEquipment, EQUIP_PICKAXE);
		addEquipmentFieldFunc(VEquipment, EQUIP_RAKE);
		VEquipment.AddMenu(MENU_MODULES, ">>> Manage Modules <<<");
		VoteWrapper::AddEmptyline(ClientID);

		// potions equipment
		VoteWrapper VPotions(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: Potions");
		addEquipmentFieldFunc(VPotions, EQUIP_POTION_HEAL);
		addEquipmentFieldFunc(VPotions, EQUIP_POTION_MANA);
		VoteWrapper::AddEmptyline(ClientID);

		// show and sort equipment
		const auto itemFunctionalSelected = pPlayer->m_VotesData.GetExtraID();
		VoteWrapper VInfo(ClientID);
		if(itemFunctionalSelected.has_value())
		{
			if(!ListInventory(ClientID, (ItemFunctional)itemFunctionalSelected.value()))
				VInfo.Add("The selected list is empty");
		}
		else
		{
			VInfo.Add("Select the required tab");
		}

		// add backpage
		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_MODULES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_EQUIPMENT);

		const auto& PlayerItems = CPlayerItem::Data()[ClientID];

		// weapons equipment
		VoteWrapper VFunctional(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Modules: Functional");
		auto functionalModules = PlayerItems | std::views::filter([](const auto& pair) 
		{
			return pair.second.Info()->IsType(ItemType::TYPE_MODULE) && !pair.second.Info()->HasAttributes() && pair.second.HasItem();
		});

		for(const auto& [ItemID, PlayerItem] : functionalModules)
		{
			const auto* pItemInfo = PlayerItem.Info();
			const auto EquippedFlagStr = PlayerItem.IsEquipped() ? "✔" : "";

			VFunctional.AddOption("EQUIP_ITEM", pItemInfo->GetID(), "{}{} * {}", EquippedFlagStr, pItemInfo->GetName(), pItemInfo->GetDescription());
		}

		if(VFunctional.IsEmpty())
		{
			VFunctional.Add("No modules available");
		}
		VoteWrapper::AddEmptyline(ClientID);

		// weapons equipment
		VoteWrapper VStats(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Modules: Stats");
		auto statModules = PlayerItems | std::views::filter([](const auto& pair) 
		{
			return pair.second.Info()->IsType(ItemType::TYPE_MODULE) && pair.second.Info()->HasAttributes() && pair.second.HasItem();
		});

		for(const auto& [ItemID, PlayerItem] : statModules)
		{
			const auto* pItemInfo = PlayerItem.Info();
			const auto EquippedFlagStr = PlayerItem.IsEquipped() ? "✔" : "";

			VStats.AddOption("EQUIP_ITEM", pItemInfo->GetID(), "{}{} * {}", EquippedFlagStr, pItemInfo->GetName(), PlayerItem.GetStringAttributesInfo(pPlayer));
		}

		if(VStats.IsEmpty())
		{
			VStats.Add("No modules available");
		}

		// add backpage
		VoteWrapper::AddEmptyline(ClientID);
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
	Database->Execute<DB::UPDATE>("tw_accounts_items", "Durability = '100' WHERE UserID = '{}'", pPlayer->Account()->GetID());
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

bool CInventoryManager::ListInventory(int ClientID, ItemType Type)
{
	bool hasItems = false;
	ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem)
	{
		ItemSelected(GS()->GetPlayer(ClientID), &pItem);
		hasItems = true;
	});
	return hasItems;
}

bool CInventoryManager::ListInventory(int ClientID, ItemFunctional Type)
{
	bool hasItems = false;
	ExecuteTemplateItemsTypes(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem)
	{
		ItemSelected(GS()->GetPlayer(ClientID), &pItem);
		hasItems = true;
	});
	return hasItems;
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
			VItems.AddOption("SELL_ITEM", ID, Price, "[{}] Sell {} ({$} gold's per unit)", pPlayer->GetItem(ID)->GetValue(), Item.GetName(), Price);
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
	{
		VItem.Add("{}", pInfo->GetDescription());
	}

	// is used item
	if(pInfo->m_Function == FUNCTION_ONE_USED || pInfo->m_Function == FUNCTION_USED)
	{
		VItem.AddOption("IUSE", ItemID, "Use");
	}

	// is potion
	if(pInfo->m_Type == ItemType::TYPE_POTION)
	{
		VItem.AddOption("EQUIP_ITEM", ItemID, "Auto use - {}", (pItem->m_Settings ? "Enable" : "Disable"));
	}

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

		ResultPtr pRes = Database->Execute<DB::SELECT>("Value", "tw_accounts_items", "WHERE ItemID = '{}' AND UserID = '{}'", ItemID, AccountID);
		if(pRes->next())
		{
			const int ReallyValue = pRes->getInt("Value") + Value;
			Database->Execute<DB::UPDATE>("tw_accounts_items", "Value = '{}' WHERE UserID = '{}' AND ItemID = '{}'", ReallyValue, AccountID, ItemID);
			lock_sleep.unlock();
			return;
		}
		Database->Execute<DB::INSERT>("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('{}', '{}', '{}', '0', '0')", ItemID, AccountID, Value);
		lock_sleep.unlock();
	});
	Thread.detach();
}