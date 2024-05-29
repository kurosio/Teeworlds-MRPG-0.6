/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "craft_manager.h"

#include <game/server/gamecontext.h>

void CCraftManager::OnInit()
{
	// load crafts
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_crafts_list");
	while(pRes->next())
	{
		int ItemID = pRes->getInt("ItemID");
		int ItemValue = pRes->getInt("ItemValue");
		int Price = pRes->getInt("Price");
		int WorldID = pRes->getInt("WorldID");

		// initialize required ingredients
		CItemsContainer RequiredIngredients {};
		std::string JsonRequiredData = pRes->getString("RequiredItems").c_str();
		Tools::Json::parseFromString(JsonRequiredData, [&](nlohmann::json& pJson)
		{
			RequiredIngredients = CItem::FromArrayJSON(pJson, "items");
		});

		// initialize new craft element
		CraftIdentifier ID = pRes->getInt("ID");
		CCraftItem::CreateElement(ID)->Init(RequiredIngredients, CItem(ItemID, ItemValue), Price, WorldID);
	}

	// sort craft item's by function
	std::sort(CCraftItem::Data().begin(), CCraftItem::Data().end(), [](const CCraftItem* p1, const CCraftItem* p2)
	{
		return p1->GetItem()->Info()->GetFunctional() > p2->GetItem()->Info()->GetFunctional();
	});

	// show information about initilized craft item's
	Core()->ShowLoadingProgress("Craft item's", (int)CCraftItem::Data().size());
}

bool CCraftManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	if (pChr->GetTiles()->IsEnter(TILE_CRAFT_ZONE))
	{
		DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_CRAFT_LIST);
		return true;
	}
	else if (pChr->GetTiles()->IsExit(TILE_CRAFT_ZONE))
	{
		DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}
	return false;
}

void CCraftManager::CraftItem(CPlayer *pPlayer, CCraftItem* pCraft) const
{
	if(!pPlayer || !pCraft)
		return;

	const int ClientID = pPlayer->GetCID();
	CPlayerItem* pPlayerCraftItem = pPlayer->GetItem(*pCraft->GetItem());

	// check enchant
	if (pPlayerCraftItem->Info()->IsEnchantable() && pPlayerCraftItem->GetValue() > 0)
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return;
	}

	// first podding set what is available and required for removal
	std::string strBuffer {};
	for(auto& RequiredItem : pCraft->GetRequiredItems())
	{
		if(pPlayer->GetItem(RequiredItem)->GetValue() < RequiredItem.GetValue())
		{
			const int ItemLeft = (RequiredItem.GetValue() - pPlayer->GetItem(RequiredItem)->GetValue());
			strBuffer += fmt_handle(ClientID, "{}x{} ", RequiredItem.Info()->GetName(), ItemLeft);
		}
	}
	if(!strBuffer.empty())
	{
		GS()->Chat(ClientID, "Item left: {}", strBuffer.c_str());
		return;
	}

	// we are already organizing the crafting
	const int Price = pCraft->GetPrice(pPlayer);
	if(!pPlayer->Account()->SpendCurrency(Price))
		return;

	// delete ticket if equipped
	if(pPlayer->GetItem(itTicketDiscountCraft)->IsEquipped())
	{
		pPlayer->GetItem(itTicketDiscountCraft)->Remove(1);
		GS()->Chat(ClientID, "You used item {} and get discount 25%.", GS()->GetItemInfo(itTicketDiscountCraft)->GetName());
	}

	// action get and remove
	for(auto& RequiredItem : pCraft->GetRequiredItems())
	{
		pPlayer->GetItem(RequiredItem)->Remove(RequiredItem.GetValue());
	}

	// add craft item
	const int CraftGetValue = pCraft->GetItem()->GetValue();
	pPlayerCraftItem->Add(CraftGetValue);
	if(pPlayerCraftItem->Info()->IsEnchantable())
	{
		GS()->Chat(-1, "{} crafted [{}x{}].", Server()->ClientName(ClientID), pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	}
	else
	{
		GS()->Chat(ClientID, "You crafted [{}x{}].", pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	}

	// achievement
	pPlayer->UpdateAchievement(ACHIEVEMENT_CRAFT_ITEM, pCraft->GetID(), CraftGetValue, PROGRESS_ADD);
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

	return false;
}

bool CCraftManager::OnPlayerMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// craft list menu
	if(Menulist == MENU_CRAFT_LIST)
	{
		// show information
		VoteWrapper VCraftInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2692 Crafting Information");
		VCraftInfo.Add("If you will not have enough items for crafting");
		VCraftInfo.Add("You will write those and the amount that is still required");
		VCraftInfo.AddItemValue(itGold);

		// show craft tabs
		ShowCraftList(pPlayer, "Can be used's", ItemType::TYPE_USED);
		ShowCraftList(pPlayer, "Potion's", ItemType::TYPE_POTION);
		ShowCraftList(pPlayer, "Equipment's", ItemType::TYPE_EQUIP);
		ShowCraftList(pPlayer, "Module's", ItemType::TYPE_MODULE);
		ShowCraftList(pPlayer, "Decoration's", ItemType::TYPE_DECORATION);
		ShowCraftList(pPlayer, "Craft's", ItemType::TYPE_CRAFT);
		ShowCraftList(pPlayer, "Other's", ItemType::TYPE_OTHER);
		ShowCraftList(pPlayer, "Quest and all the rest's", ItemType::TYPE_INVISIBLE);
		return true;
	}

	// craft selected item
	if(Menulist == MENU_CRAFT_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_CRAFT_LIST);

		// show craft by id
		int CraftID = pPlayer->m_VotesData.GetGroupID();
		ShowCraftItem(pPlayer, GetCraftByID(CraftID));
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
	VCraftItem.Add("Crafting: {}x{}", pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue());
	VCraftItem.Add("{}", pCraftItemInfo->GetDescription());
	if(pCraftItemInfo->IsEnchantable())
	{
		char aAttributes[128];
		pCraftItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), 0);
		VCraftItem.Add(aAttributes);
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
			CPlayerItem* pGold = pPlayer->GetItem(itGold);
			bool Has = pPlayer->GetItem(itGold)->GetValue() >= CraftPrice;
			VCraftRequired.MarkList().Add("{} {}x{} ({})", Has ? "\u2714" : "\u2718", pGold->Info()->GetName(), CraftPrice, pGold->GetValue());
		}

		// requied items
		for(auto& pRequiredItem : pCraft->GetRequiredItems())
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequiredItem);
			bool Has = pPlayerItem->GetValue() >= pRequiredItem.GetValue();
			VCraftRequired.MarkList().Add("{} {}x{} ({})", Has ? "\u2714" : "\u2718", 
				pRequiredItem.Info()->GetName(), pRequiredItem.GetValue(), pPlayerItem->GetValue());
		}
		VCraftRequired.EndDepth();
	}
	VoteWrapper::AddEmptyline(ClientID);

	// add craft button
	VoteWrapper(ClientID).AddOption("CRAFT", pCraft->GetID(), "\u2699 Craft ({} gold)", pCraft->GetPrice(pPlayer));

	// add backpage
	VoteWrapper::AddEmptyline(ClientID);
	VoteWrapper::AddBackpage(ClientID);
}

void CCraftManager::ShowCraftList(CPlayer* pPlayer, const char* TypeName, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();

	// order only by type
	if(std::all_of(CCraftItem::Data().begin(), CCraftItem::Data().end(), [Type](const CCraftItem* p)
		{ return p->GetItem()->Info()->GetType() != Type; }))
		return;

	// add empty line
	VoteWrapper::AddEmptyline(ClientID);

	// craft tab list
	VoteWrapper VCraftList(ClientID, VWF_SEPARATE_OPEN, TypeName);
	for(const auto& pCraft : CCraftItem::Data())
	{
		CItemDescription* pCraftItemInfo = pCraft->GetItem()->Info();

		// check by type and world
		if(pCraftItemInfo->GetType() != Type || pCraft->GetWorldID() != GS()->GetWorldID())
			continue;

		CraftIdentifier ID = pCraft->GetID();
		ItemIdentifier ItemID = pCraft->GetItem()->GetID();
		const int Price = pCraft->GetPrice(pPlayer);

		// set title name by enchant type (or stack item, or only once)
		if(pCraftItemInfo->IsEnchantable())
		{
			VCraftList.AddMenu(MENU_CRAFT_SELECTED, ID, "{}{} - {} gold", 
				(pPlayer->GetItem(ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), Price);
		}
		else
		{
			VCraftList.AddMenu(MENU_CRAFT_SELECTED, ID, "[{}]{}x{} - {} gold",
				pPlayer->GetItem(ItemID)->GetValue(), pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue(), Price);
		}
	}

	// add line
	VoteWrapper::AddLine(ClientID);
}

CCraftItem* CCraftManager::GetCraftByID(CraftIdentifier ID) const
{
	auto iter = std::find_if(CCraftItem::Data().begin(), CCraftItem::Data().end(), [ID](const CCraftItem* p){ return p->GetID() == ID; });
	return iter != CCraftItem::Data().end() ? *iter : nullptr;
}
