/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "inventory_manager.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/houses/house_manager.h>
#include <game/server/core/components/quests/quest_manager.h>

void ForEachMatchingItem(std::optional<ItemGroup> filterGroup, std::optional<ItemType> filterType,
	const std::map<int, CPlayerItem>& items, const std::function<void(const CPlayerItem&)>& callback)
{
	for(const auto& [itemID, itemData] : items)
	{
		if(!itemData.HasItem())
			continue;

		if((filterGroup && !itemData.Info()->IsGroup(*filterGroup)) ||
			(filterType && !itemData.Info()->IsType(*filterType)))
			continue;

		callback(itemData);
	}
}


void CInventoryManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_items_list");
	while(pRes->next())
	{
		const auto ItemID = pRes->getInt("ID");
		const auto Name = pRes->getString("Name");
		const auto Description = pRes->getString("Description");
		const auto GroupSet = DBSet(pRes->getString("Group"));
		const auto TypeSet = DBSet(pRes->getString("Type"));
		const auto FlagsSet = DBSet(pRes->getString("Flags"));
		const auto ScenarioData = pRes->getString("ScenarioData");
		const auto InitialPrice = pRes->getInt("InitialPrice");
		const auto Data = pRes->getString("Data");

		CItemDescription::ContainerAttributes aContainerAttributes;
		for(int i = 0; i < MAX_ATTRIBUTES_FOR_ITEM; i++)
		{
			char aAttributeID[32], aAttributeValue[32];
			str_format(aAttributeID, sizeof(aAttributeID), "AT%d", (i + 1));
			str_format(aAttributeValue, sizeof(aAttributeValue), "ATValue%d", (i + 1));

			const auto AttributeID = (AttributeIdentifier)pRes->getInt(aAttributeID);
			const auto AttributeValue = pRes->getInt(aAttributeValue);

			if(AttributeID >= AttributeIdentifier::DMG && AttributeValue > 0)
			{
				CAttribute Attribute(AttributeID, AttributeValue);
				aContainerAttributes.emplace_back(Attribute);
			}
		}

		CItemDescription(ItemID).Init(Name, Description, GroupSet, TypeSet, FlagsSet, InitialPrice, aContainerAttributes, Data, ScenarioData);
	}

	ResultPtr pResAtt = Database->Execute<DB::SELECT>("*", "tw_attributes");
	while(pResAtt->next())
	{
		const auto ID = (AttributeIdentifier)pResAtt->getInt("ID");
		const auto Name = pResAtt->getString("Name");
		const auto UpgradePrice = pResAtt->getInt("Price");
		const auto Group = (AttributeGroup)pResAtt->getInt("Group");

		CAttributeDescription::CreateElement(ID)->Init(Name, UpgradePrice, Group);
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
		ShowPlayerInventory(pPlayer);
		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_INVENTORY_RANDOMBOX_OPEN)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_INVENTORY);

		if(auto ItemIdOpt = pPlayer->m_VotesData.GetExtraID())
		{
			// random box information
			const auto* pPlayerItem = pPlayer->GetItem(*ItemIdOpt);
			VoteWrapper VBoxDetail(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "Detail information");
			if(pPlayerItem->Info()->GetRandomBox())
			{
				const auto& vItemList = pPlayerItem->Info()->GetRandomBox()->GetItems();
				for(auto& [ItemDetail, Chance] : vItemList)
				{
					auto* pDropItemInfo = GS()->GetItemInfo(ItemDetail.ItemID);
					const auto Value = ItemDetail.Value;
					const auto pSelector = GetSelectorStringByCondition(pPlayer->m_CurrentRandomItem == ItemDetail);
					VBoxDetail.Add("{~.2}% - {} x{$}{SELECTOR}", Chance, pDropItemInfo->GetName(), Value, pSelector);
				}

				VBoxDetail.AddLine();
				VBoxDetail.AddItemValue(pPlayerItem->GetID());
				if(pPlayerItem->HasItem())
					VBoxDetail.AddOption("USE_ITEM", pPlayerItem->GetID(), "Use (ID: {})", pPlayerItem->GetID());
			}
		}

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_EQUIPMENT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// lambda equipment
		const auto TypeSelectedOpt = pPlayer->m_VotesData.GetExtraID();
		auto addEquipmentFieldFunc = [&](VoteWrapper& Wrapper, ItemType EquipID) -> bool
		{
			// check if equipped
			const char* pSelector = GetSelectorStringByCondition(TypeSelectedOpt && (ItemType)(*TypeSelectedOpt) == EquipID);
			const auto EquippedItemIdOpt = pPlayer->GetEquippedSlotItemID(EquipID);
			if(!EquippedItemIdOpt.has_value() || !pPlayer->GetItem(EquippedItemIdOpt.value())->IsEquipped())
			{
				Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} not equipped{SELECTOR}", GetItemTypeName(EquipID), pSelector);
				return false;
			}

			// handle potion information
			const auto pPlayerItem = pPlayer->GetItem(EquippedItemIdOpt.value());
			if(EquipID == ItemType::EquipPotionHeal || EquipID == ItemType::EquipPotionMana)
			{
				if(const auto PotionContextOpt = pPlayerItem->Info()->GetPotionContext())
				{
					const auto RecastTotal = PotionContextOpt->Lifetime + POTION_RECAST_DEFAULT_TIME;
					Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} (recast {} / +{}) x{}{SELECTOR}",
						pPlayerItem->Info()->GetName(), RecastTotal, PotionContextOpt->Value, pPlayerItem->GetValue(), pSelector);
				}
				else
				{
					Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} x{}{SELECTOR}", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue(), pSelector);
				}
				return true;
			}

			// handle other equipment
			const auto strAttributes = pPlayerItem->GetStringAttributesInfo(pPlayer);
			Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} / {}{SELECTOR}", pPlayerItem->Info()->GetName(), strAttributes, pSelector);
			return true;
		};

		// basic equipments tools
		VoteWrapper VMain(ClientID, VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: Basic");
		VMain.AddOption("AUTO_EQUIP_SLOTS", "Auto equip slots by best equipment");
		VoteWrapper::AddEmptyline(ClientID);

		// profession slots
		auto* pProfession = pPlayer->Account()->GetActiveProfession();
		if(pProfession)
		{
			VoteWrapper VProfession(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: Profession");
			for(auto& [Type, ItemIdOpt] : pProfession->GetEquippedSlots().getSlots())
				addEquipmentFieldFunc(VProfession, Type);
			VoteWrapper::AddEmptyline(ClientID);
		}

		// shared slots
		VoteWrapper VShared(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: Shared");
		for(auto& [Type, ItemIdOpt] : pPlayer->Account()->GetEquippedSlots().getSlots())
			addEquipmentFieldFunc(VShared, Type);
		VoteWrapper::AddEmptyline(ClientID);

		// profession job slots
		VoteWrapper VJob(ClientID, VWF_SEPARATE_OPEN | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Equipment: Job");
		for(auto& Prof : pPlayer->Account()->GetProfessions())
		{
			if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER))
			{
				for(auto& [Type, ItemIdOpt] : Prof.GetEquippedSlots().getSlots())
					addEquipmentFieldFunc(VJob, Type);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// show and sort equipment
		VoteWrapper VInfo(ClientID);
		if(TypeSelectedOpt.has_value())
		{
			if(!ListInventory(ClientID, std::nullopt, (ItemType)TypeSelectedOpt.value()))
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
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		ShowPlayerModules(pPlayer);

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CInventoryManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// filter itmes
	if(PPSTR(pCmd, "FILTER_ITEMS") == 0)
	{
		pPlayer->m_InventoryItemGroupFilter = Extra1 != NOPE ? std::make_optional<ItemGroup>((ItemGroup)Extra1) : std::nullopt;
		pPlayer->m_InventoryItemTypeFilter = Extra2 != NOPE ? std::make_optional<ItemType>((ItemType)Extra2) : std::nullopt;
		pPlayer->m_VotesData.ResetHidden();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// Drop item
	if(PPSTR(pCmd, "DROP_ITEM") == 0)
	{
		// check available value
		auto AvailableValue = GetUnfrozenItemValue(pPlayer, Extra1);
		if(AvailableValue <= 0)
			return true;

		auto* pPlayerItem = pPlayer->GetItem(Extra1);
		ReasonNumber = minimum(AvailableValue, ReasonNumber);
		if(pPlayerItem->Drop(ReasonNumber))
		{
			GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "You drop {} x{}", pPlayerItem->Info()->GetName(), ReasonNumber);
			GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_DROP);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}

	// Use item
	if(PPSTR(pCmd, "USE_ITEM") == 0)
	{
		// check available value
		int AvailableValue = GetUnfrozenItemValue(pPlayer, Extra1);
		if(AvailableValue <= 0)
			return true;

		auto* pPlayerItem = pPlayer->GetItem(Extra1);
		ReasonNumber = minimum(AvailableValue, ReasonNumber);
		pPlayerItem->Use(ReasonNumber);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// Enchant item
	if(PPSTR(pCmd, "ENCHANT_ITEM") == 0)
	{
		// check enchant max level
		auto* pPlayerItem = pPlayer->GetItem(Extra1);
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
		const auto strNewAttributes = pPlayerItem->Info()->HasAttributes() ? pPlayerItem->GetStringAttributesInfo(pPlayer) : "unattributed";
		GS()->Chat(-1, "'{}' enchant '{} {} {}'.", Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(),
			pPlayerItem->GetStringEnchantLevel(), strNewAttributes);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// Auto equip slots by best equippement items
	if(PPSTR(pCmd, "AUTO_EQUIP_SLOTS") == 0)
	{
		pPlayer->Account()->AutoEquipSlots(false);
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	// Equip item
	if(PPSTR(pCmd, "TOGGLE_EQUIP") == 0)
	{
		bool Succesful = false;
		auto* pPlayerItem = pPlayer->GetItem(Extra1);
		if(pPlayerItem->IsEquipped())
			Succesful = pPlayerItem->UnEquip();
		else
			Succesful = pPlayerItem->Equip();

		if(Succesful)
		{
			pPlayerItem->Save();
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}

	// Settings
	if(PPSTR(pCmd, "TOGGLE_SETTING") == 0)
	{
		auto* pPlayerItem = pPlayer->GetItem(Extra1);
		if(pPlayerItem->IsEquipped())
			pPlayerItem->UnEquip();
		else
			pPlayerItem->Equip();

		pPlayerItem->Save();
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
	{
		if(Item.GetDurability() < 100)
			Item.SetDurability(100);
	}
	pPlayer->TryCreateEidolon();
}

std::vector<int> CInventoryManager::GetItemsCollection(std::optional<ItemGroup> optGroup, std::optional<ItemType> optType)
{
	std::vector<int> ItemIDs {};

	for(const auto& [ID, pInfo] : CItemDescription::Data())
	{
		if(!optGroup.has_value() && !optType.has_value())
			break;

		if(optGroup.has_value() && !pInfo.IsGroup(optGroup.value()))
			continue;

		if(optType.has_value() && !pInfo.IsType(optType.value()))
			continue;

		ItemIDs.push_back(ID);
	}

	return ItemIDs;
}

std::vector<int> CInventoryManager::GetItemIDsCollectionByType(ItemType Type)
{
	std::vector<int> ItemIDs {};

	for(const auto& [ID, pInfo] : CItemDescription::Data())
	{
		if(pInfo.IsType(Type))
			ItemIDs.push_back(ID);
	}

	return ItemIDs;
}

bool CInventoryManager::ListInventory(int ClientID, std::optional<ItemGroup> GroupOpt, std::optional<ItemType> TypeOpt)
{
	bool hasItems = false;
	ForEachMatchingItem(GroupOpt, TypeOpt, CPlayerItem::Data()[ClientID], [this, &ClientID, &hasItems](const CPlayerItem& pItem)
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

CPlayerItem* CInventoryManager::GetBestEquipmentSlotItem(CPlayer* pPlayer, ItemType Type)
{
	if(!pPlayer)
		return nullptr;

	const auto ClientID = pPlayer->GetCID();
	auto& PlayerItemsContainer = CPlayerItem::Data()[ClientID];

	CPlayerItem* pBestItem = nullptr;

	for(auto& [ItemId, Item] : PlayerItemsContainer)
	{
		if(!Item.HasItem() || !Item.Info()->IsEquipmentSlot() || !Item.Info()->IsType(Type))
			continue;

		if(pBestItem == nullptr || Item.GetTotalAttributesLevel() > pBestItem->GetTotalAttributesLevel())
			pBestItem = &Item;
	}

	return pBestItem;
}

void CInventoryManager::ItemSelected(CPlayer* pPlayer, const CPlayerItem* pItem)
{
	const int ClientID = pPlayer->GetCID();
	const ItemIdentifier ItemID = pItem->GetID();
	CItemDescription* pInfo = pItem->Info();

	// name description
	VoteWrapper VItem(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE);
	if(!pInfo->IsStackable())
		VItem.SetTitle("{}{} {}", (pItem->IsEquipped() ? "✔ " : "\0"), pInfo->GetName(), pItem->GetStringEnchantLevel().c_str());
	else
		VItem.SetTitle("{}{} x{}", (pItem->IsEquipped() ? "✔ " : "\0"), pInfo->GetName(), pItem->m_Value);

	// is group quest non action
	if(pInfo->m_Group == ItemGroup::Quest)
	{
		VItem.Add("{}", pInfo->GetDescription());
		return;
	}

	// add attributes
	if(pInfo->HasAttributes())
		VItem.Add("{}", pItem->GetStringAttributesInfo(pPlayer));

	// show description
	if(pPlayer->GetItem(itShowEquipmentDescription)->IsEquipped())
		VItem.Add("{}", pInfo->GetDescription());

	// is used item
	if(pInfo->m_Group == ItemGroup::Usable && (pInfo->m_Type == ItemType::UseSingle || pInfo->m_Type == ItemType::UseMultiple))
	{
		// random box
		if(pInfo->GetRandomBox())
			VItem.AddMenu(MENU_INVENTORY_RANDOMBOX_OPEN, ItemID, "Open");
		else
			VItem.AddOption("USE_ITEM", ItemID, "Use (ID: {})", ItemID);
	}

	// is potion
	if(pInfo->m_Group == ItemGroup::Potion)
	{
		VItem.AddOption("USE_ITEM", ItemID, "Use (ID: {})", ItemID);
		VItem.AddOption("TOGGLE_EQUIP", ItemID, "Auto use - {}", (pItem->IsEquipped() ? "Enable" : "Disable"));
	}

	// is decoration
	if(pInfo->m_Group == ItemGroup::Decoration)
	{
		VItem.AddOption("DECORATION_HOUSE_ADD", ItemID, "Start drawing near house");
		VItem.AddOption("GUILD_HOUSE_DECORATION", ItemID, "Start drawing near guild house");
	}

	// is equipped
	if(pInfo->m_Group == ItemGroup::Equipment)
	{
		if(pInfo->m_Type == ItemType::EquipHammer && pItem->IsEquipped())
		{
			VItem.Add("You can not undress equipping hammer");
		}
		else if(pInfo->IsEquipmentModules())
		{
			VItem.Add("Cannot be equipped from inventory");
		}
		else if(pPlayer->Account()->IsAvailableEquipmentSlot(pInfo->m_Type))
		{
			VItem.AddOption("TOGGLE_EQUIP", ItemID, (pItem->IsEquipped() ? "Undress" : "Equip"));
		}
		else
		{
			VItem.Add("You can't equip on this profession");
		}
	}

	// is enchantable
	if(pInfo->IsEnchantable() && !pItem->IsEnchantMaxLevel())
	{
		const int Price = pItem->GetEnchantPrice();
		VItem.AddOption("ENCHANT_ITEM", ItemID, "Enchant ({}m)", Price);
	}

	// not allowed drop or equipped hammer or title
	bool IsEquippedHammer = (ItemID == pPlayer->GetEquippedSlotItemID(ItemType::EquipHammer));
	if(!IsEquippedHammer && !pInfo->IsType(ItemType::EquipTitle))
	{
		// can trade
		if(!pInfo->HasFlag(ITEMFLAG_CANT_TRADE) && pInfo->GetInitialPrice() > 0)
			VItem.AddOption("AUCTION_CREATE", ItemID, "Sell at auction");

		// drop
		if(!pInfo->HasFlag(ITEMFLAG_CANT_DROP))
			VItem.AddOption("DROP_ITEM", ItemID, "Drop");
	}
}

int CInventoryManager::GetCountItemsType(CPlayer* pPlayer, std::optional<ItemGroup> GroupOpt, std::optional<ItemType> TypeOpt) const
{
	const int ClientID = pPlayer->GetCID();
	return (int)std::count_if(CPlayerItem::Data()[ClientID].cbegin(), CPlayerItem::Data()[ClientID].cend(), [GroupOpt, TypeOpt](auto pItem)
	{
		return pItem.second.HasItem() && (!GroupOpt || pItem.second.Info()->IsGroup(*GroupOpt)) &&
											(!TypeOpt || pItem.second.Info()->IsType(*TypeOpt));
	});
}

void CInventoryManager::ShowPlayerInventory(CPlayer* pPlayer)
{
	if(!pPlayer)
		return;

	const auto ClientID = pPlayer->GetCID();
	std::map<ItemGroup, int> vGroupCounts {};
	std::vector<std::pair<ItemGroup, std::string_view>> vMenuItems =
	{
		{ ItemGroup::Usable,      "\u270C" },
		{ ItemGroup::Resource,    "\u2692" },
		{ ItemGroup::Equipment,   "\u26B0" },
		{ ItemGroup::Potion,      "\u26B1" },
		{ ItemGroup::Quest,       "\u26C1" },
		{ ItemGroup::Other,       "\u26C3" }
	};

	auto getDefaultTypeName = [](ItemGroup Group) -> const char*
	{
		switch(Group)
		{
			case ItemGroup::Resource:    return "Resource (Misc)";
			case ItemGroup::Potion:      return "Potion (Misc)";
			case ItemGroup::Equipment:   return "Equipment (Modules)";
			case ItemGroup::Other:       return "Miscellaneous";
			default:                     return "Other";
		}
	};

	// inventory group filter
	VoteWrapper VFilterGroup(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u262A Inventory tabs");
	for(const auto& [Group, Icon] : vMenuItems)
	{
		const char* pSelected = GetSelectorStringByCondition(pPlayer->m_InventoryItemGroupFilter && (*pPlayer->m_InventoryItemGroupFilter) == Group);
		const int ItemGroupCount = GetCountItemsType(pPlayer, Group, std::nullopt);
		vGroupCounts[Group] = ItemGroupCount;
		VFilterGroup.AddOption("FILTER_ITEMS", (int)Group, "{} {} ({}){}", Icon, GetItemGroupName(Group), ItemGroupCount, pSelected);
	}
	VoteWrapper::AddEmptyline(ClientID);

	// active filter group
	if(pPlayer->m_InventoryItemGroupFilter)
	{
		const auto& FilterGroupOpt = pPlayer->m_InventoryItemGroupFilter;
		const auto FilterGroupValue = *FilterGroupOpt;

		std::set<ItemType> vTypes {};
		for(const auto& [itemId, data] : CItemDescription::Data())
		{
			if(pPlayer->GetItem(itemId)->HasItem() && data.IsGroup(FilterGroupValue))
				vTypes.insert(data.GetType());
		}

		// show type selector or one full one group
		if(vTypes.size() > 1)
		{
			std::map<ItemType, int> vTypeCounts{};
			for(const auto& Type : vTypes)
				vTypeCounts[Type] = GetCountItemsType(pPlayer, FilterGroupOpt, Type);

			VFilterGroup.AddLine();
			const char* pSelected = GetSelectorStringByCondition(!pPlayer->m_InventoryItemTypeFilter);
			VFilterGroup.AddOption("FILTER_ITEMS", (int)FilterGroupValue, "All items ({}){SELECTOR}", vGroupCounts[FilterGroupValue], pSelected);
			for(const auto& Type : vTypes)
			{
				pSelected = GetSelectorStringByCondition(pPlayer->m_InventoryItemTypeFilter && *pPlayer->m_InventoryItemTypeFilter == Type);
				const char* pTypeName = Type == ItemType::Default ? getDefaultTypeName(FilterGroupValue) : GetItemTypeName(Type);
				VFilterGroup.AddOption("FILTER_ITEMS", (int)FilterGroupValue, (int)Type, "{} ({}){SELECTOR}", pTypeName, vTypeCounts[Type], pSelected);
			}
		}

		VoteWrapper VInfo(ClientID);
		if(!ListInventory(ClientID, pPlayer->m_InventoryItemGroupFilter, pPlayer->m_InventoryItemTypeFilter))
			VInfo.Add("The selected list is empty");
	}
}

void CInventoryManager::ShowPlayerModules(CPlayer* pPlayer)
{
	if(!pPlayer)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	const int MaxAttrSlots = g_Config.m_SvAttributedModulesSlots;
	const int MaxFuncSlots = g_Config.m_SvNonAttributedModulesSlots;
	const bool SimpleView = pPlayer->GetItem(itShowOnlyFunctionModules)->GetSettings() > 0;

	// settings
	VoteWrapper VSettings(ClientID, VWF_OPEN, "\u2699 Modules settings");
	VSettings.AddOption("TOGGLE_SETTING", itShowOnlyFunctionModules, "[{}] {}",
		SimpleView ? "Enabled" : "Disabled", pPlayer->GetItem(itShowOnlyFunctionModules)->Info()->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// add menus
	int equippedFunc = 0;
	int equippedStats = 0;
	VoteWrapper VCollected(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_STRICT_BOLD, "\u2604 Active Effects Summary");
	VoteWrapper::AddEmptyline(ClientID);
	VoteWrapper VFunctional(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE, "\u2699 Modules: Functional ({} of {})",
		pPlayer->Account()->GetUsedSlotsFunctionalModules(), MaxFuncSlots);
	VoteWrapper::AddEmptyline(ClientID);
	VoteWrapper VStats(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE, "\u2696 Modules: Stats ({} of {})", pPlayer->Account()->GetUsedSlotsAttributedModules(), MaxAttrSlots);
	for(const auto& [ItemID, Item] : CPlayerItem::Data()[ClientID])
	{
		const auto* pInfo = Item.Info();
		if(!pInfo->IsEquipmentModules() || !Item.HasItem())
			continue;

		auto SortPriority = Item.IsEquipped() ? 1 : 0;
		const bool HasAttrs = pInfo->HasAttributes();
		VoteWrapper& TargetList = HasAttrs ? VStats : VFunctional;
		const auto Description = HasAttrs ? Item.GetStringAttributesInfo(pPlayer) : pInfo->GetDescription();
		const char* EquippedFlag = Item.IsEquipped() ? "✔ " : "";
		if(SimpleView)
			TargetList.AddOption("TOGGLE_EQUIP", ItemID, "{}{}", EquippedFlag, Description).SetSortPriority(SortPriority);
		else
			TargetList.AddOption("TOGGLE_EQUIP", ItemID, "{}{}", EquippedFlag, pInfo->GetName()).SetSortPriority(SortPriority);

		if(Item.IsEquipped())
		{
			SortPriority = !HasAttrs ? 1 : 0;
			VCollected.Add("\u2022 {}", Description).SetSortPriority(SortPriority);
			HasAttrs ? equippedStats++ : equippedFunc++;
		}
	}

	// append spaces line
	for(int i = equippedFunc; i < MaxFuncSlots; ++i) VCollected.Add("\u2022");
	for(int i = equippedStats; i < MaxAttrSlots; ++i) VCollected.Add("\u2022");

	// sorting all modules groups
	auto Sorter = [](const CVoteOption& o1, const CVoteOption& o2)
	{
		if(o1.m_SortPriority != o2.m_SortPriority)
			return o1.m_SortPriority > o2.m_SortPriority;
		return std::string_view(o1.m_aDescription) < std::string_view(o2.m_aDescription);
	};
	VCollected.Sort(Sorter);
	VFunctional.Sort(Sorter);
	VStats.Sort(Sorter);
}