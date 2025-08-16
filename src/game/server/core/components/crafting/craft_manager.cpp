/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "craft_manager.h"

#include <game/server/gamecontext.h>
#include <generated/server_data.h>

#include <components/inventory/inventory_manager.h>
#include <components/mails/mail_wrapper.h>

void CCraftManager::OnInitWorld(const std::string& Where)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_crafts_list", Where.c_str());
	while(pRes->next())
	{
		// initialize variables
		const auto GroupNameSet = DBSet(pRes->getString("GroupName"));
		const auto ItemID = pRes->getInt("ItemID");
		const auto ItemValue = pRes->getInt("ItemValue");
		const auto Price = pRes->getInt("Price");
		const auto WorldID = pRes->getInt("WorldID");

		// initialize required ingredients
		CItemsContainer RequiredIngredients {};
		DBSet ItemsSet(pRes->getString("RequiredItems"));
		for(const auto& Elem : ItemsSet.getItems())
		{
			int ItemID {};
			int Value {};
			if(sscanf(Elem.c_str(), "[%d/%d]", &ItemID, &Value) == 2)
			{
				CItem Item(ItemID, Value);
				RequiredIngredients.push_back(Item);
			}
		}

		// initialize craft element
		CraftIdentifier ID = pRes->getInt("ID");
		auto* pCraftItem = CCraftItem::CreateElement(ID);
		pCraftItem->Init(RequiredIngredients, CItem(ItemID, ItemValue), Price, WorldID);

		// add to grouped list
		for(const auto& NameFromSet : GroupNameSet.getItems())
		{
			if(!NameFromSet.empty())
			{
				auto [currentParsingGroup, currentParsingSubgroup] = mystd::string::split_by_delimiter(NameFromSet, ':');
				if(currentParsingSubgroup.empty())
					currentParsingSubgroup = m_vGroupedCrafts.get_default_subgroup_key();
				if(!currentParsingGroup.empty())
					m_vGroupedCrafts.add_item(currentParsingGroup, currentParsingSubgroup, pCraftItem);
			}
		}
	}

	// sort all items
	m_vGroupedCrafts.sort_all_items([](const CCraftItem* a, const CCraftItem* b)
	{
		return a->GetPrice() < b->GetPrice();
	});
}


void CCraftManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_CRAFT_ZONE, MENU_CRAFTING_LIST, {}, {});
}


void CCraftManager::CraftItem(CPlayer* pPlayer, CCraftItem* pCraft, int Value) const
{
	if(!pPlayer || !pCraft || Value <= 0)
		return;

	const auto ClientID = pPlayer->GetCID();
	auto* pPlayerCraftItem = pPlayer->GetItem(*pCraft->GetItem());
	const auto* pSkillCraft = pPlayer->GetSkill(SKILL_CRAFT_DISCOUNT);
	Value = pCraft->GetItem()->Info()->IsEnchantable() ? 1 : Value;

	// check if we have all the necessary items
	std::string missingItems;
	for(const auto& RequiredItem : pCraft->GetRequiredItems())
	{
		const int playerItemCount = pPlayer->GetItem(RequiredItem)->GetValue();
		const int requiredPerOne = RequiredItem.GetValue();
		const int totalRequiredCount = requiredPerOne * Value;

		if(playerItemCount < totalRequiredCount)
		{
			int itemShortage = totalRequiredCount - playerItemCount;
			missingItems += fmt_default("{} x{} ", RequiredItem.Info()->GetName(), itemShortage);
		}
	}

	if(!missingItems.empty())
	{
		GS()->Chat(ClientID, "Item(s) left to gather: '{}'", missingItems.c_str());
		return;
	}

	// checking to see if there are enough funds for crafting
	const int pricePerOne = pCraft->GetPrice(pPlayer);
	const int totalCraftPrice = pricePerOne * Value;
	if(!pPlayer->Account()->SpendCurrency(totalCraftPrice))
		return;

	// remove necessary items from the player's inventory
	for(const auto& RequiredItem : pCraft->GetRequiredItems())
	{
		const int amountToRemove = RequiredItem.GetValue() * Value;
		pPlayer->GetItem(RequiredItem)->Remove(amountToRemove);
	}

	// calculate extra crafted item
	int ExtraCraftedItemCount = 0;
	const int NumPerCraft = pCraft->GetItem()->GetValue();
	for(int i = 0; i < Value; i++)
	{
		float ChanceExtraItem = random_float(100.f);
		if(ChanceExtraItem < pSkillCraft->GetMod(SkillMod::MasterCraftExtraItemPct))
			ExtraCraftedItemCount += NumPerCraft;
	}

	// add the crafted item to the player's inventory
	const int CraftedItemCount = NumPerCraft * Value;
	if(ExtraCraftedItemCount > 0 && pPlayerCraftItem->Info()->IsEnchantable())
	{
		MailWrapper Mail("System", pPlayer->Account()->GetID(), "Extra craft item.");
		Mail.AddDescLine("We can't put it in inventory");
		Mail.AttachItem(*pCraft->GetItem());
		Mail.Send();
		pPlayerCraftItem->Add(CraftedItemCount);
	}
	else
	{
		const int TotalCraftedItemCount = CraftedItemCount + ExtraCraftedItemCount;
		pPlayerCraftItem->Add(TotalCraftedItemCount);

	}

	// report a crafted item, either to everyone or only to a player, depending on its characteristics
	std::string ValueInfo = ExtraCraftedItemCount ? fmt_default("{}(+{})", CraftedItemCount, ExtraCraftedItemCount) : fmt_default("{}", CraftedItemCount);
	if(!pPlayerCraftItem->Info()->IsStackable())
		GS()->Chat(-1, "'{}' crafted '[{} x{}]'.", Server()->ClientName(ClientID), pPlayerCraftItem->Info()->GetName(), ValueInfo);
	else
		GS()->Chat(ClientID, "You crafted '[{} x{}]'.", pPlayerCraftItem->Info()->GetName(), ValueInfo);

	// notify event and votes
	g_EventListenerManager.Notify<IEventListener::PlayerCraftItem>(pPlayer, pCraft);
	GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_CRAFT);
	pPlayer->m_VotesData.UpdateCurrentVotes();
}


bool CCraftManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	// craft item function
	if(PPSTR(pCmd, "CRAFT") == 0)
	{
        const int CraftID = GetIfExists<int>(Extras, 0, NOPE);
		CraftItem(pPlayer, GetCraftByID(CraftID), ReasonNumber);
		return true;
	}

	if(PPSTR(pCmd, "CRAFT_TAB_SELECT") == 0)
	{
        const int CraftGroupID = GetIfExists<int>(Extras, 0, NOPE);
		pPlayer->m_ActiveCraftGroupID = CraftGroupID;
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	return false;
}


bool CCraftManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// craft list menu
	if(Menulist == MENU_CRAFTING_LIST)
	{
		ShowGroupedSelector(pPlayer);
		return true;
	}

	// craft selected item
	if(Menulist == MENU_CRAFTING_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_CRAFTING_LIST);

		if(const auto CraftID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowCraftItem(pPlayer, GetCraftByID(CraftID.value()));
			VoteWrapper::AddEmptyline(ClientID);
		}

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}


void CCraftManager::ShowCraftItem(CPlayer* pPlayer, CCraftItem* pCraft) const
{
	const int ClientID = pPlayer->GetCID();
	if(!pCraft || pCraft->GetWorldID() != GS()->GetWorldID())
		return;

	// detail information
	VoteWrapper VCraftItem(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2692 Detail information");
	CItemDescription* pCraftItemInfo = pCraft->GetItem()->Info();
	VCraftItem.Add("Crafting: {} x{}", pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue());
	VCraftItem.Add("{}", pCraftItemInfo->GetDescription());
	if(pCraftItemInfo->HasAttributes())
	{
		VCraftItem.Add(pCraftItemInfo->GetStringAttributesInfo(pPlayer, 0).c_str());
	}
	VoteWrapper::AddEmptyline(ClientID);

	// add craft reciepts
	VoteWrapper VCraftRequired(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Required items", pCraftItemInfo->GetName());
	{
		VCraftRequired.BeginDepth();

		// required gold
		int CraftPrice = pCraft->GetPrice(pPlayer);
		if(CraftPrice > 0)
		{
			auto playerGold = pPlayer->Account()->GetTotalGold();
			bool hasEnoughGold = pPlayer->Account()->GetTotalGold() >= CraftPrice;
			VCraftRequired.MarkList().Add("{} Gold x{$} ({$})", hasEnoughGold ? "\u2714" : "\u2718", CraftPrice, playerGold);
		}

		// requied items
		for(const auto& pRequiredItem : pCraft->GetRequiredItems())
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequiredItem);
			bool hasEnoughItems = pPlayerItem->GetValue() >= pRequiredItem.GetValue();
			VCraftRequired.MarkList().Add("{} {} x{} ({})", hasEnoughItems ? "\u2714" : "\u2718",
				pRequiredItem.Info()->GetName(), pRequiredItem.GetValue(), pPlayerItem->GetValue());
		}
		VCraftRequired.EndDepth();
	}
	VoteWrapper::AddEmptyline(ClientID);

	// add craft button
	bool hasCraftItem = pPlayer->GetItem(pCraft->GetItem()->GetID())->HasItem();
	if(!pCraftItemInfo->IsStackable() && hasCraftItem)
		VoteWrapper(ClientID).Add("- You already have the item", pCraft->GetPrice(pPlayer));
	else
		VoteWrapper(ClientID).AddOption("CRAFT", pCraft->GetID(), "\u2699 Craft ({$} gold)", pCraft->GetPrice(pPlayer));
}


void CCraftManager::ShowCraftGroup(CPlayer* pPlayer, const std::string& GroupName, const std::vector<CCraftItem*>& vItems) const
{
	// initialize variables
	const auto ClientID = pPlayer->GetCID();

	// add group menu
    std::unordered_map<int, int> DuplicateItems;
    std::unordered_map<int, int> DuplicateIndex;
    for (const auto& item : vItems) {
        ++DuplicateItems[item->GetItem()->GetID()];
    }

	VoteWrapper VCraftList(ClientID, VWF_STYLE_SIMPLE|VWF_SEPARATE_OPEN, GroupName.c_str());
	for(auto& pCraft : vItems)
	{
		const auto ID = pCraft->GetID();
		const auto ItemID = pCraft->GetItem()->GetID();
		const int Price = pCraft->GetPrice(pPlayer);
		const auto* pCraftItemInfo = pCraft->GetItem()->Info();

        std::optional<int> DuplicateId = std::nullopt;
        if(DuplicateItems[ItemID] > 1)
            DuplicateId = ++DuplicateIndex[ItemID];

		// set title name by enchant type (or stack item, or only once)
		if(!pCraftItemInfo->IsStackable())
		{
            if(DuplicateId)
                VCraftList.AddMenu(MENU_CRAFTING_SELECT, ID, "{}{} ({}) - {$} gold",
                   (pPlayer->GetItem(ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), *DuplicateId, Price);
            else
			    VCraftList.AddMenu(MENU_CRAFTING_SELECT, ID, "{}{} - {$} gold",
				    (pPlayer->GetItem(ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), Price);
		}
		else
		{
            if(DuplicateId)
                VCraftList.AddMenu(MENU_CRAFTING_SELECT, ID, "[{}]{} ({}) x{} - {$} gold",
                   pPlayer->GetItem(ItemID)->GetValue(), pCraftItemInfo->GetName(), *DuplicateId, pCraft->GetItem()->GetValue(), Price);
            else
			    VCraftList.AddMenu(MENU_CRAFTING_SELECT, ID, "[{}]{} x{} - {$} gold",
				    pPlayer->GetItem(ItemID)->GetValue(), pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue(), Price);
		}
	}
	VoteWrapper::AddEmptyline(ClientID);
}


void CCraftManager::ShowGroupedSelector(CPlayer* pPlayer) const
{
	if(!pPlayer)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	const auto& groupedTradesContainer = m_vGroupedCrafts;
	const auto& allGroupData = groupedTradesContainer.get_all_data();
	const auto& groupIdOpt = pPlayer->m_GroupFilter;
	const auto& subgroupIdOpt = pPlayer->m_SubgroupFilter;

	// show selector by group
	VoteWrapper VSG(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2692 Crafting List");
	for(const auto& [groupName, subGroupMap] : allGroupData)
	{
		const auto GroupID = pPlayer->m_VotesData.GetStringMapper().string_to_id(groupName);
		const char* pSelectStr = GetSelectorStringByCondition(groupIdOpt && (*groupIdOpt) == GroupID);
		const auto countItemByGroup = groupedTradesContainer.get_item_group_count(groupName);
		VSG.AddOption("WAREHOUSE_SELECTOR_GROUP", GroupID, "{}({}){SELECTOR}", Instance::Localize(ClientID, groupName.c_str()), countItemByGroup, pSelectStr);
	}

	// show selector by subgroup
	if(groupIdOpt)
	{
		// check valid id by group name and valid group container
		const auto groupNameOpt = pPlayer->m_VotesData.GetStringMapper().id_to_string(*groupIdOpt);
		if(!groupNameOpt || !groupedTradesContainer.has_group(*groupNameOpt))
			return;

		const auto* pSubGroupMap = groupedTradesContainer.get_subgroups(*groupNameOpt);
		if(!pSubGroupMap)
			return;

		bool HasOnlyDefaultGroup = (pSubGroupMap->size() <= 1 &&
			std::ranges::any_of(*pSubGroupMap, [&](const auto& pair) { return pair.first == groupedTradesContainer.get_default_subgroup_key(); }));

		if(!HasOnlyDefaultGroup)
		{
			VSG.AddLine();
			for(const auto& [subGroupName, itemList] : *pSubGroupMap)
			{
				const auto SubgroupID = pPlayer->m_VotesData.GetStringMapper().string_to_id(subGroupName);
				const char* pSelectStr = GetSelectorStringByCondition(subgroupIdOpt && (*subgroupIdOpt) == SubgroupID);
				VSG.AddOption("WAREHOUSE_SELECTOR_SUBGROUP", SubgroupID, "{}({}){SELECTOR}", subGroupName.c_str(), itemList.size(), pSelectStr);
			}
		}
		else
		{
			pPlayer->m_SubgroupFilter = pPlayer->m_VotesData.GetStringMapper().string_to_id(groupedTradesContainer.get_default_subgroup_key());
		}
	}

	// show all item's by filter
	if(groupIdOpt && subgroupIdOpt)
	{
		// check valid id by group name and valid group container
		const auto groupNameOptStr = pPlayer->m_VotesData.GetStringMapper().id_to_string(*groupIdOpt);
		if(!groupNameOptStr || !groupedTradesContainer.has_group(*groupNameOptStr))
			return;

		// check valid id by subgroup name and valid subgroup container
		const auto subGroupNameOptStr = pPlayer->m_VotesData.GetStringMapper().id_to_string(*subgroupIdOpt);
		if(!subGroupNameOptStr || !groupedTradesContainer.has_subgroup(*groupNameOptStr, *subGroupNameOptStr))
			return;

		const auto* pItemList = groupedTradesContainer.get_items(*groupNameOptStr, *subGroupNameOptStr);
		if(!pItemList)
			return;

		// show all item's by selector filter
		VoteWrapper::AddEmptyline(ClientID);
		const auto groupName = (*subGroupNameOptStr) != groupedTradesContainer.get_default_subgroup_key()
			? (*subGroupNameOptStr) : (*groupNameOptStr);
		ShowCraftGroup(pPlayer, Instance::Localize(ClientID, groupName.c_str()), *pItemList);
	}
}



CCraftItem* CCraftManager::GetCraftByID(CraftIdentifier ID) const
{
	for(const auto& craftItem : CCraftItem::Data())
	{
		if(craftItem->GetID() == ID)
			return craftItem;
	}

	return nullptr;
}