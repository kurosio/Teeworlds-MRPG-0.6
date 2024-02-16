/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "CraftManager.h"

#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

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

		CItemsContainer RequiredIngredients {};
		std::string JsonRequiredData = pRes->getString("RequiredItems").c_str();
		Tools::Json::parseFromString(JsonRequiredData, [&](nlohmann::json& pJson)
		{
			RequiredIngredients = CItem::FromArrayJSON(pJson, "items");
		});

		CraftIdentifier ID = pRes->getInt("ID");
		CCraftItem::CreateElement(ID)->Init(RequiredIngredients, CItem(ItemID, ItemValue), Price, WorldID);
	}

	// sort by function
	std::sort(CCraftItem::Data().begin(), CCraftItem::Data().end(), [](const CCraftItem* p1, const CCraftItem* p2)
	{
		return p1->GetItem()->Info()->GetFunctional() > p2->GetItem()->Info()->GetFunctional();
	});

	Core()->ShowLoadingProgress("Crafts", (int)CCraftItem::Data().size());
}

bool CCraftManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_CRAFT_ZONE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_CRAFT_LIST);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_CRAFT_ZONE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
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
	dynamic_string Buffer;
	for(auto& RequiredItem : pCraft->GetRequiredItems())
	{
		if(pPlayer->GetItem(RequiredItem)->GetValue() < RequiredItem.GetValue())
		{
			const int ItemLeft = (RequiredItem.GetValue() - pPlayer->GetItem(RequiredItem)->GetValue());
			GS()->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}x{VAL} ", RequiredItem.Info()->GetName(), ItemLeft);
		}
	}
	if(Buffer.length() > 0)
	{
		GS()->Chat(ClientID, "Item left: {STR}", Buffer.buffer());
		Buffer.clear();
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
		GS()->Chat(ClientID, "You used item {STR} and get discount 25%.", GS()->GetItemInfo(itTicketDiscountCraft)->GetName());
	}

	// action get and remove
	for(auto& RequiredItem : pCraft->GetRequiredItems())
	{
		pPlayer->GetItem(RequiredItem)->Remove(RequiredItem.GetValue());
	}

	const int CraftGetValue = pCraft->GetItem()->GetValue();
	pPlayerCraftItem->Add(CraftGetValue);
	if(pPlayerCraftItem->Info()->IsEnchantable())
	{
		GS()->Chat(-1, "{STR} crafted [{STR}x{VAL}].", Server()->ClientName(ClientID), pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	}
	else
	{
		GS()->Chat(ClientID, "You crafted [{STR}x{VAL}].", pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	}

	pPlayer->m_VotesData.UpdateCurrentVotes();
}

bool CCraftManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	if(PPSTR(CMD, "CRAFT") == 0)
	{
		CraftItem(pPlayer, GetCraftByID(VoteID));
		return true;
	}

	return false;
}

bool CCraftManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_CRAFT_LIST)
	{
		// show craft list
		CVoteWrapper VCraftInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2692 Crafting Information");
		VCraftInfo.Add("If you will not have enough items for crafting");
		VCraftInfo.Add("You will write those and the amount that is still required");
		VCraftInfo.AddItemValue(itGold);
		VCraftInfo.AddLine();

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

	if(Menulist == MENU_CRAFT_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_CRAFT_LIST);

		int CraftID = pPlayer->m_VotesData.GetMenuTemporaryInteger();
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

	CVoteWrapper VCraftItem(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2692 Detail information about craft");
	CItemDescription* pCraftItemInfo = pCraft->GetItem()->Info();
	VCraftItem.Add("Crafting: {STR}x{VAL}", pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue());
	VCraftItem.Add("{STR}", pCraftItemInfo->GetDescription());
	if(pCraftItemInfo->IsEnchantable())
	{
		char aAttributes[128];
		pCraftItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), 0);
		VCraftItem.Add(aAttributes);
	}
	VCraftItem.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	// add craft reciepts
	CVoteWrapper VCraftElement(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Craft: {STR}", pCraftItemInfo->GetName());
	VCraftElement.Add("Required items:");
	{
		VCraftElement.BeginDepthList();
		for(auto& pRequiredItem : pCraft->GetRequiredItems())
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequiredItem);
			bool Has = pPlayerItem->GetValue() >= pRequiredItem.GetValue();
			VCraftElement.Add("* {STR} {STR}x{VAL} ({VAL})", Has ? "\u2714" : "\u2718", pRequiredItem.Info()->GetName(), pRequiredItem.GetValue(), pPlayerItem->GetValue());
		}
		VCraftElement.EndDepthList();
	}
	VCraftElement.AddLine();
	VCraftElement.AddItemValue(itGold);
	{
		VCraftElement.BeginDepthList();
		VCraftElement.AddOption("CRAFT", pCraft->GetID(), "\u2699 Craft ({VAL} gold)", pCraft->GetPrice(pPlayer));
		VCraftElement.EndDepthList();
	}
	VCraftElement.AddLine();

	// add backpage
	CVoteWrapper::AddEmptyline(ClientID);
	CVoteWrapper::AddBackpage(ClientID);
}

void CCraftManager::ShowCraftList(CPlayer* pPlayer, const char* TypeName, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();
	if(std::all_of(CCraftItem::Data().begin(), CCraftItem::Data().end(), [Type](const CCraftItem* p){ return p->GetItem()->Info()->GetType() != Type; }))
		return;

	CVoteWrapper VCraftList(ClientID, VWF_SEPARATE_OPEN, TypeName);
	for(const auto& pCraft : CCraftItem::Data())
	{
		CItemDescription* pCraftItemInfo = pCraft->GetItem()->Info();
		if(pCraftItemInfo->GetType() != Type || pCraft->GetWorldID() != GS()->GetWorldID())
			continue;

		CraftIdentifier ID = pCraft->GetID();
		ItemIdentifier ItemID = pCraft->GetItem()->GetID();
		const int Price = pCraft->GetPrice(pPlayer);

		if(pCraftItemInfo->IsEnchantable())
		{
			VCraftList.AddMenu(MENU_CRAFT_SELECTED, ID, "{STR}{STR} - {VAL} gold", 
				(pPlayer->GetItem(ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), Price);
		}
		else
		{
			VCraftList.AddMenu(MENU_CRAFT_SELECTED, ID, "[{VAL}]{STR}x{INT} - {VAL} gold",
				pPlayer->GetItem(ItemID)->GetValue(), pCraftItemInfo->GetName(), pCraft->GetItem()->GetValue(), Price);
		}
	}

	CVoteWrapper::AddLine(ClientID);
}

CCraftItem* CCraftManager::GetCraftByID(CraftIdentifier ID) const
{
	auto iter = std::find_if(CCraftItem::Data().begin(), CCraftItem::Data().end(), [ID](const CCraftItem* p){ return p->GetID() == ID; });
	return iter != CCraftItem::Data().end() ? *iter : nullptr;
}
