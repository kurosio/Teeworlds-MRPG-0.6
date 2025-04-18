/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "inventory_manager.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/houses/house_manager.h>
#include <game/server/core/components/quests/quest_manager.h>

template <typename FilterT>
void ForEachMatchingItem(FilterT filter, const std::map<int, CPlayerItem>& items, const std::function<void(const CPlayerItem&)>& callback)
{
	for(const auto& [itemID, itemData] : items)
	{
		if constexpr(std::is_same_v<FilterT, ItemGroup>)
		{
			if(itemData.HasItem() && itemData.Info()->IsGroup(filter))
				callback(itemData);
		}
		else if constexpr(std::is_same_v<FilterT, ItemType>)
		{
			if(itemData.HasItem() && itemData.Info()->IsType(filter))
				callback(itemData);
		}
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
		const auto ScenarioData = pRes->getString("ScenarioData");
		const auto InitialPrice = pRes->getInt("InitialPrice");
		const auto Dysenthis = pRes->getInt("Desynthesis");
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

		CItemDescription(ItemID).Init(Name, Description, GroupSet, TypeSet, Dysenthis, InitialPrice, aContainerAttributes, Data, ScenarioData);
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

	// auto equip empty slots
	pPlayer->Account()->AutoEquipSlots(true);
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

		std::vector<std::pair<ItemGroup, std::string_view>> vMenuItems =
		{
			{ ItemGroup::Usable,           "\u270C" },
			{ ItemGroup::Resource,         "\u2692" },
			{ ItemGroup::Equipment,        "\u26B0" },
			{ ItemGroup::Potion,           "\u26B1" },
			{ ItemGroup::Quest,            "\u26C1" },
			{ ItemGroup::Other,            "\u26C3" }
		};

		// inventory tabs
		VoteWrapper VInventoryTabs(ClientID, VWF_SEPARATE|VWF_ALIGN_TITLE|VWF_STYLE_SIMPLE, "\u262A Inventory tabs");
		for(const auto& [Type, Icon] : vMenuItems)
			VInventoryTabs.AddMenu(MENU_INVENTORY, (int)Type, "{} {} ({})", Icon, GetItemGroupName(Type), GetCountItemsType(pPlayer, Type));
		VoteWrapper::AddEmptyline(ClientID);

		// show and sort inventory
		const auto itemTypeSelected = pPlayer->m_VotesData.GetExtraID();
		VoteWrapper VInfo(ClientID);
		if(itemTypeSelected.has_value())
		{
			if(!ListInventory(ClientID, (ItemGroup)itemTypeSelected.value()))
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
		auto addEquipmentFieldFunc = [&](VoteWrapper& Wrapper, ItemType EquipID) -> bool
		{
			// check if equipped
			const auto EquippedItemIdOpt = pPlayer->GetEquippedItemID(EquipID);
			if(!EquippedItemIdOpt.has_value() || !pPlayer->GetItem(EquippedItemIdOpt.value())->IsEquipped())
			{
				Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} not equipped", GetItemTypeName(EquipID));
				return false;
			}

			// handle potion information
			const auto pPlayerItem = pPlayer->GetItem(EquippedItemIdOpt.value());
			if(EquipID == ItemType::EquipPotionHeal || EquipID == ItemType::EquipPotionMana)
			{
				if(const auto PotionContextOpt = pPlayerItem->Info()->GetPotionContext())
				{
					const auto RecastTotal = PotionContextOpt->Lifetime + POTION_RECAST_APPEND_TIME;
					Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} (recast {} / +{}) x{}",
						pPlayerItem->Info()->GetName(), RecastTotal, PotionContextOpt->Value, pPlayerItem->GetValue());
				}
				else
				{
					Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} x{}", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
				}
				return true;
			}

			// handle other equipment
			const auto strAttributes = pPlayerItem->GetStringAttributesInfo(pPlayer);
			Wrapper.AddMenu(MENU_EQUIPMENT, (int)EquipID, "{} / {}", pPlayerItem->Info()->GetName(), strAttributes);
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

	if(Menulist == MENU_MODULES)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		const auto& PlayerItems = CPlayerItem::Data()[ClientID];

		// modules functional
		VoteWrapper VFunctional(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Modules: Functional");
		auto functionalModules = PlayerItems | std::views::filter([](const auto& pair)
		{
			return pair.second.Info()->IsEquipmentNonSlot() && !pair.second.Info()->HasAttributes() && pair.second.HasItem();
		});

		for(const auto& [ItemID, PlayerItem] : functionalModules)
		{
			const auto* pItemInfo = PlayerItem.Info();
			const auto EquippedFlagStr = PlayerItem.IsEquipped() ? "✔" : "";

			VFunctional.AddOption("EQUIP_ITEM", pItemInfo->GetID(), "{}{} * {}", EquippedFlagStr, pItemInfo->GetName(), pItemInfo->GetDescription());
		}
		if(VFunctional.IsEmpty())
			VFunctional.Add("No modules available");
		VoteWrapper::AddEmptyline(ClientID);

		// modules stats
		VoteWrapper VStats(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE, "\u2604 Modules: Stats");
		auto statModules = PlayerItems | std::views::filter([](const auto& pair)
		{
			return pair.second.Info()->IsEquipmentNonSlot() && pair.second.Info()->HasAttributes() && pair.second.HasItem();
		});

		for(const auto& [ItemID, PlayerItem] : statModules)
		{
			const auto* pItemInfo = PlayerItem.Info();
			const auto EquippedFlagStr = PlayerItem.IsEquipped() ? "✔" : "";

			VStats.AddOption("EQUIP_ITEM", pItemInfo->GetID(), "{}{} * {}", EquippedFlagStr, pItemInfo->GetName(), PlayerItem.GetStringAttributesInfo(pPlayer));
		}
		if(VStats.IsEmpty())
			VStats.Add("No modules available");

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CInventoryManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// Drop item
	if(PPSTR(pCmd, "DROP_ITEM") == 0)
	{
		// check available value
		auto AvailableValue = GetUnfrozenItemValue(pPlayer, Extra1);
		if(AvailableValue <= 0)
			return true;

		auto* pPlayerItem = pPlayer->GetItem(Extra1);
		ReasonNumber = minimum(AvailableValue, ReasonNumber);
		pPlayerItem->Drop(ReasonNumber);

		GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 100, "You drop {} x{}", pPlayerItem->Info()->GetName(), ReasonNumber);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_DROP);
		pPlayer->m_VotesData.UpdateCurrentVotes();
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

	// Dissasemble item
	if(PPSTR(pCmd, "DISSASEMBLE_ITEM") == 0)
	{
		// check available value
		auto AvailableValue = GetUnfrozenItemValue(pPlayer, Extra1);
		if(AvailableValue <= 0)
			return true;

		ReasonNumber = minimum(AvailableValue, ReasonNumber);
		auto* pPlayerItem = pPlayer->GetItem(Extra1);
		auto* pPlayerMaterial = pPlayer->GetItem(itMaterial);
		const int DesValue = pPlayerItem->GetDysenthis() * ReasonNumber;
		if(pPlayerItem->Remove(ReasonNumber) && pPlayerMaterial->Add(DesValue))
		{
			GS()->Chat(ClientID, "Disassemble '{} x{}'.", pPlayerItem->Info()->GetName(), ReasonNumber);
			GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_DISSASEMBLE);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
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
	if(PPSTR(pCmd, "EQUIP_ITEM") == 0)
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

	return false;
}

void CInventoryManager::RepairDurabilityItems(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	Database->Execute<DB::UPDATE>("tw_accounts_items", "Durability = '100' WHERE UserID = '{}'", pPlayer->Account()->GetID());
	for(auto& [ID, Item] : CPlayerItem::Data()[ClientID])
		Item.m_Durability = 100;

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

bool CInventoryManager::ListInventory(int ClientID, ItemGroup Group)
{
	bool hasItems = false;
	ForEachMatchingItem(Group, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem)
	{
		ItemSelected(GS()->GetPlayer(ClientID), &pItem);
		hasItems = true;
	});
	return hasItems;
}

bool CInventoryManager::ListInventory(int ClientID, ItemType Type)
{
	bool hasItems = false;
	ForEachMatchingItem(Type, CPlayerItem::Data()[ClientID], [&](const CPlayerItem& pItem)
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
		VItem.AddOption("USE_ITEM", ItemID, "Use");

	// is potion
	if(pInfo->m_Group == ItemGroup::Potion)
		VItem.AddOption("EQUIP_ITEM", ItemID, "Auto use - {}", (pItem->IsEquipped() ? "Enable" : "Disable"));

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
		else if(pPlayer->Account()->IsAvailableEquipmentSlot(pInfo->m_Type) || pInfo->IsEquipmentNonSlot())
		{
			VItem.AddOption("EQUIP_ITEM", ItemID, (pItem->IsEquipped() ? "Undress" : "Equip"));
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

	// not allowed drop equipped hammer or title
	if(ItemID != pPlayer->GetEquippedItemID(ItemType::EquipHammer) && !pInfo->IsType(ItemType::EquipTitle))
	{
		// can dysenthis
		if(pItem->GetDysenthis() > 0)
			VItem.AddOption("DISSASEMBLE_ITEM", ItemID, pItem->GetDysenthis(), "Disassemble (+{}m)", pItem->GetDysenthis());

		// can trade
		if(pInfo->m_InitialPrice > 0)
			VItem.AddOption("AUCTION_CREATE", ItemID, "Sell at auction");

		// drop
		VItem.AddOption("DROP_ITEM", ItemID, "Drop");
	}
}

int CInventoryManager::GetCountItemsType(CPlayer* pPlayer, ItemGroup Type) const
{
	const int ClientID = pPlayer->GetCID();
	return (int)std::count_if(CPlayerItem::Data()[ClientID].cbegin(), CPlayerItem::Data()[ClientID].cend(), [Type](auto pItem)
	{
		return pItem.second.HasItem() && pItem.second.Info()->IsGroup(Type);
	});
}