/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "InventoryCore.h"

#include <engine/shared/datafile.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Houses/HouseCore.h>
#include <game/server/mmocore/Components/Quests/QuestCore.h>

using namespace sqlstr;
void CInventoryCore::OnPrepareInformation(IStorageEngine* pStorage, CDataFileWriter* pDataFile)
{
	nlohmann::json JsonQuestData;
	for(auto& p : CItemDataInfo::ms_aItemsInfo)
	{
		JsonQuestData["items"].push_back(
		{
			{ "id", p.first },
			{ "name", p.second.m_aName },
			{ "desc", p.second.m_aDesc },
			{ "icon", p.second.m_aIcon },
		});
	}

	std::string Data = JsonQuestData.dump();
	pDataFile->AddItem(MMO_DATA_INVENTORY_INFORMATION, 0, (int)Data.length() + 1, Data.c_str());
}

void CInventoryCore::OnInit()
{
	SJK.SDT("*", "tw_items_list", [&](ResultPtr pRes)
	{
		while(pRes->next())
		{
			const int ItemID = (int)pRes->getInt("ItemID");
			str_copy(CItemDataInfo::ms_aItemsInfo[ItemID].m_aName, pRes->getString("Name").c_str(), sizeof(CItemDataInfo::ms_aItemsInfo[ItemID].m_aName));
			str_copy(CItemDataInfo::ms_aItemsInfo[ItemID].m_aDesc, pRes->getString("Description").c_str(), sizeof(CItemDataInfo::ms_aItemsInfo[ItemID].m_aDesc));
			str_copy(CItemDataInfo::ms_aItemsInfo[ItemID].m_aIcon, pRes->getString("Icon").c_str(), sizeof(CItemDataInfo::ms_aItemsInfo[ItemID].m_aIcon));
			CItemDataInfo::ms_aItemsInfo[ItemID].m_Type = (int)pRes->getInt("Type");
			CItemDataInfo::ms_aItemsInfo[ItemID].m_Function = (int)pRes->getInt("Function");
			CItemDataInfo::ms_aItemsInfo[ItemID].m_Dysenthis = (int)pRes->getInt("Desynthesis");
			CItemDataInfo::ms_aItemsInfo[ItemID].m_MinimalPrice = (int)pRes->getInt("Selling");
			for(int i = 0; i < STATS_MAX_FOR_ITEM; i++)
			{
				char aBuf[32];
				str_format(aBuf, sizeof(aBuf), "Attribute%d", i);
				CItemDataInfo::ms_aItemsInfo[ItemID].m_aAttribute[i] = (int)pRes->getInt(aBuf);
				str_format(aBuf, sizeof(aBuf), "AttributeValue%d", i);
				CItemDataInfo::ms_aItemsInfo[ItemID].m_aAttributeValue[i] = (int)pRes->getInt(aBuf);
			}
			CItemDataInfo::ms_aItemsInfo[ItemID].m_ProjID = (int)pRes->getInt("ProjectileID");
		}
	});

	SJK.SDT("*", "tw_attributs", [&](ResultPtr pRes)
	{
		while(pRes->next())
		{
			const int AttID = pRes->getInt("ID");
			str_copy(CGS::ms_aAttributsInfo[AttID].m_aName, pRes->getString("Name").c_str(), sizeof(CGS::ms_aAttributsInfo[AttID].m_aName));
			str_copy(CGS::ms_aAttributsInfo[AttID].m_aFieldName, pRes->getString("FieldName").c_str(), sizeof(CGS::ms_aAttributsInfo[AttID].m_aFieldName));
			CGS::ms_aAttributsInfo[AttID].m_UpgradePrice = pRes->getInt("Price");
			CGS::ms_aAttributsInfo[AttID].m_Type = pRes->getInt("Type");
			CGS::ms_aAttributsInfo[AttID].m_Devide = pRes->getInt("Divide");
		}
	});
}

void CInventoryCore::OnInitAccount(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = SJK.SD("*", "tw_accounts_items", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		int ItemID = (int)pRes->getInt("ItemID");
		CItemData::ms_aItems[ClientID][ItemID].SetItemOwner(pPlayer);
		CItemData::ms_aItems[ClientID][ItemID].m_ItemID = ItemID;
		CItemData::ms_aItems[ClientID][ItemID].m_Value = (int)pRes->getInt("Value");
		CItemData::ms_aItems[ClientID][ItemID].m_Settings = (int)pRes->getInt("Settings");
		CItemData::ms_aItems[ClientID][ItemID].m_Enchant = (int)pRes->getInt("Enchant");
		CItemData::ms_aItems[ClientID][ItemID].m_Durability = (int)pRes->getInt("Durability");
	}
}

void CInventoryCore::OnResetClient(int ClientID)
{
	CItemData::ms_aItems.erase(ClientID);
}

bool CInventoryCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
		return false;

	if(Menulist == MenuList::MENU_INVENTORY)
	{
		pPlayer->m_LastVoteMenu = MenuList::MAIN_MENU;
		GS()->AVH(ClientID, TAB_INFO_INVENTORY, GREEN_COLOR, "Inventory Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_INVENTORY, "Choose the type of items you want to show");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_INVENTORY, "After, need select item to interact");
		GS()->AV(ClientID, "null");

		GS()->AVH(ClientID, TAB_INVENTORY_SELECT, RED_COLOR, "Inventory Select List");
		int SizeItems = GetValueItemsType(pPlayer, ItemType::TYPE_USED);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_USED, TAB_INVENTORY_SELECT, "Used ({INT})", SizeItems);

		SizeItems = GetValueItemsType(pPlayer, ItemType::TYPE_CRAFT);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_CRAFT, TAB_INVENTORY_SELECT, "Craft ({INT})", SizeItems);

		SizeItems = GetValueItemsType(pPlayer, ItemType::TYPE_MODULE);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_MODULE, TAB_INVENTORY_SELECT, "Modules ({INT})", SizeItems);

		SizeItems = GetValueItemsType(pPlayer, ItemType::TYPE_POTION);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_POTION, TAB_INVENTORY_SELECT, "Potion ({INT})", SizeItems);

		SizeItems = GetValueItemsType(pPlayer, ItemType::TYPE_OTHER);
		GS()->AVM(ClientID, "SORTEDINVENTORY", ItemType::TYPE_OTHER, TAB_INVENTORY_SELECT, "Other ({INT})", SizeItems);
		if(pPlayer->m_aSortTabs[SORT_INVENTORY])
			ListInventory(pPlayer, pPlayer->m_aSortTabs[SORT_INVENTORY]);

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MenuList::MENU_EQUIPMENT)
	{
		pPlayer->m_LastVoteMenu = MenuList::MAIN_MENU;
		GS()->AVH(ClientID, TAB_INFO_EQUIP, GREEN_COLOR, "Equip / Armor Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_EQUIP, "Select tab and select armor.");
		GS()->AV(ClientID, "null");

		GS()->AVH(ClientID, TAB_EQUIP_SELECT, RED_COLOR, "Equip Select Slot");
		const char* pType[NUM_EQUIPS] = { "Hammer", "Gun", "Shotgun", "Grenade", "Rifle", "Pickaxe", "Wings", "Discord" };
		for(int i = 0; i < NUM_EQUIPS; i++)
		{
			const int ItemID = pPlayer->GetEquippedItemID(i);
			CItemData& pItemPlayer = pPlayer->GetItem(ItemID);
			if(ItemID <= 0 || !pItemPlayer.IsEquipped())
			{
				GS()->AVM(ClientID, "SORTEDEQUIP", i, TAB_EQUIP_SELECT, "{STR} Not equipped", pType[i]);
				continue;
			}

			char aAttributes[128];
			pItemPlayer.FormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
			GS()->AVMI(ClientID, pItemPlayer.Info().GetIcon(), "SORTEDEQUIP", i, TAB_EQUIP_SELECT, "{STR} | {STR}", pItemPlayer.Info().GetName(), aAttributes);
		}
		GS()->AV(ClientID, "null");

		bool FindItem = false;
		for(const auto& it : CItemData::ms_aItems[ClientID])
		{
			if(!it.second.m_Value || it.second.Info().m_Function != pPlayer->m_aSortTabs[SORT_EQUIPING])
				continue;

			ItemSelected(pPlayer, it.second, true);
			FindItem = true;
		}

		if(!FindItem)
			GS()->AVL(ClientID, "null", "There are no items in this tab");

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
		CItemData& pItemPlayer = pPlayer->GetItem(VoteID);
		pItemPlayer.Drop(Get);

		GS()->Broadcast(ClientID, BroadcastPriority::GAME_INFORMATION, 100, "You drop {STR}x{INT}", pItemPlayer.Info().GetName(), Get);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "IUSE") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = min(AvailableValue, Get);
		pPlayer->GetItem(VoteID).Use(Get);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "IDESYNTHESIS") == 0)
	{
		int AvailableValue = GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		Get = min(AvailableValue, Get);
		CItemData& pPlayerSelectedItem = pPlayer->GetItem(VoteID);
		CItemData& pPlayerMaterialItem = pPlayer->GetItem(itMaterial);
		const int DesValue = pPlayerSelectedItem.Info().m_Dysenthis * Get;
		if(pPlayerSelectedItem.Remove(Get) && pPlayerMaterialItem.Add(DesValue))
		{
			GS()->Chat(ClientID, "Disassemble {STR}x{INT}, you receive {INT} materials",
				pPlayerSelectedItem.Info().GetName(), Get, DesValue);
			GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}
		return true;
	}

	if(PPSTR(CMD, "ISETTINGS") == 0)
	{
		pPlayer->GetItem(VoteID).Equip();
		GS()->CreatePlayerSound(ClientID, SOUND_ITEM_EQUIP);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "IENCHANT") == 0)
	{
		CItemData& pItemPlayer = pPlayer->GetItem(VoteID);
		if(pItemPlayer.IsEnchantMaxLevel())
		{
			GS()->Chat(ClientID, "Enchantment level is maximum");
			return true;
		}

		const int Price = pItemPlayer.GetEnchantPrice();
		if(!pPlayer->SpendCurrency(Price, itMaterial))
			return true;

		const int EnchantLevel = pItemPlayer.m_Enchant + 1;
		pItemPlayer.SetEnchant(EnchantLevel);
		if(pItemPlayer.IsEnchantMaxLevel())
			GS()->SendEquipments(ClientID, -1);

		char aEnchantBuf[16];
		pItemPlayer.FormatEnchantLevel(aEnchantBuf, sizeof(aEnchantBuf));

		char aAttributes[128];
		pItemPlayer.FormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
		GS()->Chat(-1, "{STR} enchant {STR} {STR} {STR}", Server()->ClientName(ClientID), pItemPlayer.Info().GetName(), aEnchantBuf, aAttributes);
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
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
	SJK.UD("tw_accounts_items", "Durability = '100' WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	for(auto& it : CItemData::ms_aItems[ClientID])
		it.second.m_Durability = 100;
}

void CInventoryCore::ListInventory(CPlayer *pPlayer, int TypeList, bool SortedFunction)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AV(ClientID, "null");

	// show a list of items to the player
	bool Found = false;
	for(const auto& it : CItemData::ms_aItems[ClientID])
	{
		if(!it.second.m_Value || ((SortedFunction && it.second.Info().m_Function != TypeList)
			|| (!SortedFunction && it.second.Info().m_Type != TypeList)))
			continue;

		ItemSelected(pPlayer, it.second);
		Found = true;
	}
	if(!Found)
		GS()->AVL(ClientID, "null", "There are no items in this tab");
}

int CInventoryCore::GiveItem(CPlayer *pPlayer, int ItemID, int Value, int Settings, int Enchant)
{
	const int ClientID = pPlayer->GetCID();
	const int SecureID = SecureCheck(pPlayer, ItemID, Value, Settings, Enchant);
	if(SecureID == 1)
	{
		SJK.UD("tw_accounts_items", "Value = '%d', Settings = '%d', Enchant = '%d' WHERE ItemID = '%d' AND UserID = '%d'",
		       CItemData::ms_aItems[ClientID][ItemID].m_Value, CItemData::ms_aItems[ClientID][ItemID].m_Settings, CItemData::ms_aItems[ClientID][ItemID].m_Enchant, ItemID, pPlayer->Acc().m_UserID);
	}
	return SecureID;
}

int CInventoryCore::SecureCheck(CPlayer *pPlayer, int ItemID, int Value, int Settings, int Enchant)
{
	// check initialize and add the item
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = SJK.SD("Value, Settings", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
	if(pRes->next())
	{
		CItemData::ms_aItems[ClientID][ItemID].m_Value = pRes->getInt("Value")+Value;
		CItemData::ms_aItems[ClientID][ItemID].m_Settings = pRes->getInt("Settings")+Settings;
		CItemData::ms_aItems[ClientID][ItemID].m_Enchant = Enchant;
		return 1;
	}

	// create an object if not found
	CItemData::ms_aItems[ClientID][ItemID].m_Value = Value;
	CItemData::ms_aItems[ClientID][ItemID].m_Settings = Settings;
	CItemData::ms_aItems[ClientID][ItemID].m_Enchant = Enchant;
	CItemData::ms_aItems[ClientID][ItemID].m_Durability = 100;
	SJK.ID("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('%d', '%d', '%d', '%d', '%d')",
		ItemID, pPlayer->Acc().m_UserID, Value, Settings, Enchant);
	return 2;
}

int CInventoryCore::RemoveItem(CPlayer *pPlayer, int ItemID, int Value, int Settings)
{
	const int SecureID = DeSecureCheck(pPlayer, ItemID, Value, Settings);
	if(SecureID == 1)
	{
		SJK.UD("tw_accounts_items", "Value = Value - '%d', Settings = Settings - '%d' WHERE ItemID = '%d' AND UserID = '%d'",
			Value, Settings, ItemID, pPlayer->Acc().m_UserID);
	}
	return SecureID;
}

int CInventoryCore::DeSecureCheck(CPlayer *pPlayer, int ItemID, int Value, int Settings)
{
	// we check the database
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = SJK.SD("Value, Settings", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
	if(pRes->next())
	{
		// update if there is more
		if(pRes->getInt("Value") > Value)
		{
			CItemData::ms_aItems[ClientID][ItemID].m_Value = pRes->getInt("Value")-Value;
			CItemData::ms_aItems[ClientID][ItemID].m_Settings = pRes->getInt("Settings")-Settings;
			return 1;
		}

		// remove the object if it is less than the required amount
		CItemData::ms_aItems[ClientID][ItemID].m_Value = 0;
		CItemData::ms_aItems[ClientID][ItemID].m_Settings = 0;
		CItemData::ms_aItems[ClientID][ItemID].m_Enchant = 0;
		SJK.DD("tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
		return 2;
	}

	CItemData::ms_aItems[ClientID][ItemID].m_Value = 0;
	CItemData::ms_aItems[ClientID][ItemID].m_Settings = 0;
	CItemData::ms_aItems[ClientID][ItemID].m_Enchant = 0;
	return 0;
}

int CInventoryCore::GetUnfrozenItemValue(CPlayer *pPlayer, int ItemID) const
{
	const int AvailableValue = Job()->Quest()->GetUnfrozenItemValue(pPlayer, ItemID);
	if(AvailableValue <= 0 && pPlayer->GetItem(ItemID).m_Value >= 1)
	{
		GS()->Chat(pPlayer->GetCID(), "'{STR}' frozen for some quest.", pPlayer->GetItem(ItemID).Info().GetName());
		GS()->Chat(pPlayer->GetCID(), "In the 'Adventure Journal', you can see in which quest an item is used", pPlayer->GetItem(ItemID).Info().GetName());
	}
	return AvailableValue;
}

void CInventoryCore::ItemSelected(CPlayer* pPlayer, const CItemData& pItemPlayer, bool Dress)
{
	const int ClientID = pPlayer->GetCID();
	const int ItemID = pItemPlayer.GetID();
	const int HideID = NUM_TAB_MENU + ItemID;
	const char* pNameItem = pItemPlayer.Info().GetName();

	// overwritten or not
	if (pItemPlayer.Info().IsEnchantable())
	{
		char aEnchantBuf[16];
		pItemPlayer.FormatEnchantLevel(aEnchantBuf, sizeof(aEnchantBuf));
		GS()->AVHI(ClientID, pItemPlayer.Info().GetIcon(), HideID, LIGHT_GRAY_COLOR, "{STR} {STR}{STR}",
			pNameItem, (pItemPlayer.m_Enchant > 0 ? aEnchantBuf : "\0"), (pItemPlayer.m_Settings ? " ✔" : "\0"));
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pItemPlayer.Info().GetDesc());

		char aAttributes[64];
		pItemPlayer.FormatAttributes(pPlayer, aAttributes, sizeof(aAttributes));
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
	}
	else
	{
		GS()->AVHI(ClientID, pItemPlayer.Info().GetIcon(), HideID, LIGHT_GRAY_COLOR, "{STR}{STR} x{INT}",
			(pItemPlayer.m_Settings ? "Dressed - " : "\0"), pNameItem, pItemPlayer.m_Value);
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pItemPlayer.Info().GetDesc());
	}

	if (pItemPlayer.Info().m_Function == FUNCTION_ONE_USED || pItemPlayer.Info().m_Function == FUNCTION_USED)
	{
		GS()->AVM(ClientID, "null", NOPE, HideID, "For bind command '/useitem {INT}'", ItemID);
		GS()->AVM(ClientID, "IUSE", ItemID, HideID, "Use {STR}", pNameItem);
	}

	if (pItemPlayer.Info().m_Type == ItemType::TYPE_POTION)
		GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, "Auto use {STR} - {STR}", pNameItem, (pItemPlayer.m_Settings ? "Enable" : "Disable"));
	else if (pItemPlayer.Info().m_Type == ItemType::TYPE_DECORATION)
	{
		GS()->AVM(ClientID, "DECOSTART", ItemID, HideID, "Add {STR} to your house", pNameItem);
		GS()->AVM(ClientID, "DECOGUILDSTART", ItemID, HideID, "Add {STR} to your guild house", pNameItem);
	}
	else if(pItemPlayer.Info().m_Type == ItemType::TYPE_EQUIP || pItemPlayer.Info().m_Function == FUNCTION_SETTINGS)
	{
		if((pItemPlayer.Info().m_Function == EQUIP_HAMMER && pItemPlayer.IsEquipped()))
			GS()->AVM(ClientID, "null", NOPE, HideID, "You can not undress equipping hammer", pNameItem);
		else
			GS()->AVM(ClientID, "ISETTINGS", ItemID, HideID, "{STR} {STR}", (pItemPlayer.m_Settings ? "Undress" : "Equip"), pNameItem);
	}

	if(pItemPlayer.Info().m_Function == FUNCTION_PLANTS)
	{
		const int HouseID = Job()->House()->OwnerHouseID(pPlayer->Acc().m_UserID);
		const int PlantItemID = Job()->House()->GetPlantsID(HouseID);
		if(HouseID > 0 && PlantItemID != ItemID)
		{
			const int random_change = random_int() % 900;
			GS()->AVD(ClientID, "HOMEPLANTSET", ItemID, random_change, HideID, "To plant {STR}, to house (0.06%)", pNameItem);
		}
	}

	if (pItemPlayer.Info().IsEnchantable() && !pItemPlayer.IsEnchantMaxLevel())
	{
		const int Price = pItemPlayer.GetEnchantPrice();
		GS()->AVM(ClientID, "IENCHANT", ItemID, HideID, "Enchant {STR} ({INT} materials)", pNameItem, Price);
	}

	// not allowed drop equipped hammer
	if (ItemID == pPlayer->GetEquippedItemID(EQUIP_HAMMER))
		return;

	if (pItemPlayer.Info().m_Dysenthis > 0)
		GS()->AVM(ClientID, "IDESYNTHESIS", ItemID, HideID, "Disassemble {STR} (+{INT} materials)", pNameItem, pItemPlayer.Info().m_Dysenthis);

	GS()->AVM(ClientID, "IDROP", ItemID, HideID, "Drop {STR}", pNameItem);

	if (pItemPlayer.Info().m_MinimalPrice > 0)
		GS()->AVM(ClientID, "AUCTIONSLOT", ItemID, HideID, "Create Slot Auction {STR}", pNameItem);
}

int CInventoryCore::GetValueItemsType(CPlayer *pPlayer, int Type) const
{
	const int ClientID = pPlayer->GetCID();
	return (int)std::count_if(CItemData::ms_aItems[ClientID].begin(), CItemData::ms_aItems[ClientID].end(), [Type](std::pair< const int, CItemData>& pItem)
	                          {return pItem.second.m_Value > 0 && pItem.second.Info().m_Type == Type; });
}

// TODO: FIX IT (lock .. unlock)
std::mutex lock_sleep;
void CInventoryCore::AddItemSleep(int AccountID, int ItemID, int Value, int Milliseconds)
{
	std::thread Thread([this, AccountID, ItemID, Value, Milliseconds]()
	{
		if(Milliseconds > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

		lock_sleep.lock();
		CPlayer* pPlayer = GS()->GetPlayerFromUserID(AccountID);
		if(pPlayer)
		{
			pPlayer->GetItem(ItemID).Add(Value);
			lock_sleep.unlock();
			return;
		}

		ResultPtr pRes = SJK.SD("Value", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, AccountID);
		if(pRes->next())
		{
			const int ReallyValue = (int)pRes->getInt("Value") + Value;
			SJK.UD("tw_accounts_items", "Value = '%d' WHERE UserID = '%d' AND ItemID = '%d'", ReallyValue, AccountID, ItemID);
			lock_sleep.unlock();
			return;
		}
		SJK.ID("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('%d', '%d', '%d', '0', '0')", ItemID, AccountID, Value);
		lock_sleep.unlock();
	});
	Thread.detach();
}