/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "craft_manager.h"

#include <game/server/gamecontext.h>

#include "../achievements/achievement_data.h"
#include "../Inventory/InventoryManager.h"


void CCraftManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_crafts_list");
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
		for(const auto& [Elem, Size] : ItemsSet.GetDataItems())
		{
			int ItemID{};
			int Value{};
			if(sscanf(Elem.c_str(), "[%d/%d]", &ItemID, &Value) == 2)
			{
				CItem Item(ItemID, Value);
				RequiredIngredients.push_back(Item);
			}
		}

		// initialize craft element
		CraftIdentifier ID = pRes->getInt("ID");
		for(auto& [Name, P] : GroupNameSet.GetDataItems())
		{
			auto* pCraftItem = CCraftItem::CreateElement(Name, ID);
			pCraftItem->Init(RequiredIngredients, CItem(ItemID, ItemValue), Price, WorldID);
		}
	}
}


void CCraftManager::OnInitWorld(const std::string&)
{
	const int currentWorldID = GS()->GetWorldID();

	// filter only current world and sorting by price
	for(auto& [Name, vItems] : CCraftItem::Data())
	{
		auto vResult = vItems | std::views::filter([currentWorldID](CCraftItem* pCraft)
		{ return pCraft->GetWorldID() == currentWorldID; });
		if(!vResult.empty())
		{
			std::deque<CCraftItem*> vFilteredItems(vResult.begin(), vResult.end());
			std::ranges::sort(vFilteredItems, [](const auto* pA, const auto* pB)
			{ return pA->GetPrice() < pB->GetPrice(); });
			m_vOrderedCraftList.emplace_back(Name, std::move(vFilteredItems));
		}
	}

	// ordered crafting groups
	std::ranges::sort(m_vOrderedCraftList, [](const auto& a, const auto& b)
	{
		auto maxA = std::ranges::max_element(a.second, [](CCraftItem* pLeft, CCraftItem* pRight) { return pLeft->GetPrice() < pRight->GetPrice(); });
		auto maxB = std::ranges::max_element(b.second, [](CCraftItem* pLeft, CCraftItem* pRight) { return pLeft->GetPrice() < pRight->GetPrice(); });
		return maxA != a.second.end() && maxB != b.second.end() && (*maxA)->GetPrice() < (*maxB)->GetPrice();
	});
}


void CCraftManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_CRAFT_ZONE, MENU_CRAFTING_LIST, {}, {});
}


void CCraftManager::CraftItem(CPlayer* pPlayer, CCraftItem* pCraft) const
{
	if(!pPlayer || !pCraft)
		return;

	const auto ClientID = pPlayer->GetCID();
	auto* pPlayerCraftItem = pPlayer->GetItem(*pCraft->GetItem());

	// check if we have all the necessary items
	std::string missingItems;
	for(const auto& RequiredItem : pCraft->GetRequiredItems())
	{
		const int playerItemCount = pPlayer->GetItem(RequiredItem)->GetValue();
		const int requiredItemCount = RequiredItem.GetValue();

		if(playerItemCount < requiredItemCount)
		{
			int itemShortage = requiredItemCount - playerItemCount;
			missingItems += fmt_default("{} x{} ", RequiredItem.Info()->GetName(), itemShortage);
		}
	}

	if(!missingItems.empty())
	{
		GS()->Chat(ClientID, "Item(s) left to gather: '{}'", missingItems.c_str());
		return;
	}

	// checking to see if there are enough funds for crafting
	const int craftPrice = pCraft->GetPrice(pPlayer);
	if(!pPlayer->Account()->SpendCurrency(craftPrice))
		return;

	// remove necessary items from the player's inventory
	for(const auto& RequiredItem : pCraft->GetRequiredItems())
	{
		pPlayer->GetItem(RequiredItem)->Remove(RequiredItem.GetValue());
	}

	// add the crafted item to the player's inventory
	const int craftedItemCount = pCraft->GetItem()->GetValue();
	pPlayerCraftItem->Add(craftedItemCount);

	// report a crafted item, either to everyone or only to a player, depending on its characteristics
	if(!pPlayerCraftItem->Info()->IsStackable())
	{
		GS()->Chat(-1, "'{}' crafted '[{} x{}]'.", Server()->ClientName(ClientID), pPlayerCraftItem->Info()->GetName(), craftedItemCount);
	}
	else
	{
		GS()->Chat(ClientID, "You crafted '[{} x{}]'.", pPlayerCraftItem->Info()->GetName(), craftedItemCount);
	}

	// notify event and votes
	g_EventListenerManager.Notify<IEventListener::PlayerCraftItem>(pPlayer, pCraft);
	GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_CRAFT);
	pPlayer->m_VotesData.UpdateCurrentVotes();
}


bool CCraftManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	// craft item function
	if(PPSTR(pCmd, "CRAFT") == 0)
	{
		CraftItem(pPlayer, GetCraftByID(Extra1));
		return true;
	}

	if(PPSTR(pCmd, "CRAFT_TAB_SELECT") == 0)
	{
		pPlayer->m_ActiveCraftGroupID = Extra1;
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}

	return false;
}


bool CCraftManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// craft list menu
	if(Menulist == MENU_CRAFTING_LIST)
	{
		// craft selector
		VoteWrapper VCraftSelector(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2692 Crafting List");
		VCraftSelector.AddItemValue(itGold);
		VCraftSelector.AddLine();
		for(int i = 0; i < (int)m_vOrderedCraftList.size(); i++)
			VCraftSelector.AddOption("CRAFT_TAB_SELECT", i, m_vOrderedCraftList[i].first.c_str());
		VoteWrapper::AddEmptyline(ClientID);

		// show tab items
		const auto activeCraftGroupID = pPlayer->m_ActiveCraftGroupID;
		if(activeCraftGroupID >= 0 && activeCraftGroupID < (int)m_vOrderedCraftList.size())
		{
			const auto& [Name, vItems] = m_vOrderedCraftList[activeCraftGroupID];
			ShowCraftGroup(pPlayer, Name, vItems);
		}

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


void CCraftManager::ShowCraftGroup(CPlayer* pPlayer, const std::string& GroupName, const std::deque<CCraftItem*>& vItems) const
{
	// initialize variables
	const auto ClientID = pPlayer->GetCID();

	// add group menu
	VoteWrapper VCraftList(ClientID, VWF_STYLE_SIMPLE|VWF_SEPARATE_OPEN, GroupName.c_str());
	for(auto& pCraft : vItems)
	{
		const auto ID = pCraft->GetID();
		const auto ItemID = pCraft->GetItem()->GetID();
		const int Price = pCraft->GetPrice(pPlayer);
		const auto* pCraftItemInfo = pCraft->GetItem()->Info();

		// set title name by enchant type (or stack item, or only once)
		if(!pCraftItemInfo->IsStackable())
		{
			VCraftList.AddMenu(MENU_CRAFTING_SELECT, ID, "{}{} - {$} gold",
				(pPlayer->GetItem(ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), Price);
		}
		else
		{
			VCraftList.AddMenu(MENU_CRAFTING_SELECT, ID, "[{}]{} x{} - {$} gold",
				pPlayer->GetItem(ItemID)->GetValue(), pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue(), Price);
		}
	}
	VoteWrapper::AddEmptyline(ClientID);
}


CCraftItem* CCraftManager::GetCraftByID(CraftIdentifier ID) const
{
	for(const auto& [key, itemDeque] : CCraftItem::Data())
	{
		auto iter = std::ranges::find_if(itemDeque, [ID](const CCraftItem* p) { return p->GetID() == ID; });
		if(iter != itemDeque.end())
			return *iter;
	}

	return nullptr;
}