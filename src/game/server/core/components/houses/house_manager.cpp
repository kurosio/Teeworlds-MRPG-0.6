/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_manager.h"

#include <game/server/core/components/inventory/inventory_manager.h>
#include <game/server/gamecontext.h>

#include "entities/house_door.h"

constexpr int g_UpdateTextLifeTime = SERVER_TICK_SPEED * 2;

void CHouseManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	// initialize houses
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_HOUSES_TABLE, SqlQueryWhereWorld.c_str());
	while(pRes->next())
	{
		const auto ID = pRes->getInt("ID");
		const auto AccountID = pRes->getInt("UserID");
		const auto ClassName = pRes->getString("Class");
		const auto InitialFee = pRes->getInt("InitialFee");
		const auto RentDays = pRes->getInt("RentDays");
		const auto Bank = pRes->getBigInt("Bank");
		const auto WorldID = pRes->getInt("WorldID");
		std::string DoorsData = pRes->getString("Doors");
		std::string FarmzonesData = pRes->getString("Farmzones");
		std::string PropertiesData = pRes->getString("Properties");

		CHouse::CreateElement(ID)->Init(AccountID, ClassName, RentDays, InitialFee, Bank, WorldID, DoorsData, FarmzonesData, PropertiesData);
	}
}

void CHouseManager::OnTick()
{
	// check if we are in the main world
	if(GS()->GetWorldID() != INITIALIZER_WORLD_ID)
		return;

	// update houses text
	if(Server()->Tick() % g_Config.m_SvUpdateEntityTextNames == 0)
	{
		for(const auto& p : CHouse::Data())
			p->UpdateText(g_Config.m_SvUpdateEntityTextNames);
	}
}

void CHouseManager::OnGlobalTimePeriod(ETimePeriod Period)
{
	for(auto& p : CHouse::Data())
		p->HandleTimePeriod(Period);
}

void CHouseManager::OnCharacterTile(CCharacter* pChr)
{
	auto* pPlayer = pChr->GetPlayer();
	const auto ClientID = pPlayer->GetCID();

	HANDLE_TILE_MOTD_MENU(pPlayer, pChr, TILE_PLAYER_HOUSE, MOTD_MENU_PLAYER_HOUSE_DETAIL)
}

bool CHouseManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// menu house
	if(Menulist == MENU_HOUSE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house sell
	if(Menulist == MENU_HOUSE_SELL)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowSell(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house doors
	if(Menulist == MENU_HOUSE_DOOR_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowDoorsController(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house farm zones list
	if(Menulist == MENU_HOUSE_FARMZONE_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowFarmzonesControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house farm zone selected
	if(Menulist == MENU_HOUSE_FARMZONE_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE_FARMZONE_LIST);

		if(const auto FarmzoneID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowFarmzoneEdit(pPlayer, FarmzoneID.value());
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CHouseManager::OnSendMenuMotd(CPlayer* pPlayer, int Menulist)
{
	if(Menulist == MOTD_MENU_PLAYER_HOUSE_DETAIL)
	{
		ShowDetail(pPlayer, GetHouseByPos(pPlayer->GetCharacter()->m_Core.m_Pos));
		return true;
	}

	return false;
}

bool CHouseManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pText)
{
	const int ClientID = pPlayer->GetCID();

	// start decoration edit mode
	if(PPSTR(pCmd, "HOUSE_DECORATION") == 0)
	{
		// check house valid
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// result
		if(pHouse->GetDecorationManager()->StartDrawing(pPlayer))
			GS()->Chat(ClientID, "You can now draw decorations.");
		else
			GS()->Chat(ClientID, "You can't draw decorations.");
		return true;
	}

	// house farm zone try plant
	if(PPSTR(pCmd, "HOUSE_FARMZONE_REMOVE_PLANT") == 0)
	{
		// check valid house
		auto *pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// initialize variables
		const int& FarmzoneID = Extra1;
		const ItemIdentifier& ItemID = Extra2;

		// check farmzone valid
		auto* pManager = pHouse->GetFarmzonesManager();
		auto* pFarmzone = pManager->GetFarmzoneByID(FarmzoneID);
		if(!pFarmzone)
		{
			GS()->Chat(ClientID, "Farm zone not found.");
			return true;
		}

		// check is same planted item with current
		if(pFarmzone->RemoveItemFromNode(ItemID))
		{
			auto* pItemInfo = GS()->GetItemInfo(ItemID);
			GS()->Chat(ClientID, "You have successfully removed the '{}' from '{}'.", pItemInfo->GetName(), pFarmzone->GetName());
			pManager->Save();
		}

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_FARMZONE_SELECT);
		return true;
	}

	// house farm zone try plant
	if(PPSTR(pCmd, "HOUSE_FARMZONE_TRY_PLANT") == 0)
	{
		// check valid house
		auto *pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// initialize variables
		const int& Useds = maximum(1, ReasonNumber);
		const int& FarmzoneID = Extra1;
		const ItemIdentifier& ItemID = Extra2;

		// check farmzone valid
		auto* pManager = pHouse->GetFarmzonesManager();
		auto* pFarmzone = pManager->GetFarmzoneByID(FarmzoneID);
		if(!pFarmzone)
		{
			GS()->Chat(ClientID, "Farm zone not found.");
			return true;
		}

		// check is same planted item with current
		if(pFarmzone->GetNode().m_vItems.hasElement(ItemID))
		{
			GS()->Chat(ClientID, "This item has already been planted on this farm.");
			return true;
		}

		// check spend currency (planting item)
		if(pPlayer->Account()->SpendCurrency(Useds, ItemID))
		{
			bool Success = false;
			Chance result(s_GuildChancePlanting);
			for(int i = 0; i < Useds && !Success; i++)
			{
				Success = result() ? true : Success;
				result.Update();
			}

			// result
			if(Success)
			{
				GS()->Chat(ClientID, "You have successfully plant to farm zone.");
				pFarmzone->AddItemToNode(ItemID);
				pManager->Save();
			}
			else
			{
				GS()->Chat(ClientID, "You failed plant to farm zone.");
			}

			pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_FARMZONE_SELECT);
		}

		return true;
	}

	// teleport to house
	if(PPSTR(pCmd, "HOUSE_TELEPORT") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// change world in case the house is in another world
		if(!GS()->IsPlayerInWorld(ClientID, pHouse->GetWorldID()))
		{
			pPlayer->ChangeWorld(pHouse->GetWorldID(), pHouse->GetPos());
			return true;
		}

		// set new position
		pPlayer->GetCharacter()->ChangePosition(pHouse->GetPos());
		return true;
	}

	// sell house
	if(PPSTR(pCmd, "HOUSE_SELL") == 0)
	{
		// check player house
		auto *pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check captcha accidental press
		if(ReasonNumber != 777)
		{
			GS()->Chat(ClientID, "A verification number was entered incorrectly!");
			return true;
		}

		// sell house
		pHouse->Sell();
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		return true;
	}

	// add gold to house bank
	if(PPSTR(pCmd, "HOUSE_BANK_ADD") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check minimal
		if(ReasonNumber < 100)
		{
			GS()->Chat(ClientID, "The minimum interaction cannot be below '100 gold'!");
			return true;
		}

		// add gold to house bank
		pHouse->GetBankManager()->Add(ReasonNumber);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE);
		return true;
	}

	// take gold from house bank
	if(PPSTR(pCmd, "HOUSE_BANK_TAKE") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check minimal
		if(ReasonNumber < 100)
		{
			GS()->Chat(ClientID, "The minimum interaction cannot be below '100 gold'!");
			return true;
		}

		// take gold from house bank
		pHouse->GetBankManager()->Take(ReasonNumber);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE);
		return true;
	}

	// interact with house door
	if(PPSTR(pCmd, "HOUSE_DOOR") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// reverse door house
		int UniqueDoorID = Extra1;
		pHouse->GetDoorManager()->Reverse(UniqueDoorID);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
		return true;
	}

	// extend rent
	if(PPSTR(pCmd, "HOUSE_EXTEND_RENT") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check maximal rent days
		if(pHouse->GetRentDays() >= MAX_HOUSE_RENT_DAYS)
		{
			GS()->Chat(ClientID, "You can not extend the rent anymore then {# (day|days)}.", (int)MAX_HOUSE_RENT_DAYS);
			return true;
		}

		// check minimal
		ReasonNumber = clamp(ReasonNumber, 0, MAX_HOUSE_RENT_DAYS - pHouse->GetRentDays());
		if(!ReasonNumber)
		{
			GS()->Chat(ClientID, "Minimum is 1 day. Maximum {# total (day|days)}.", (int)MAX_HOUSE_RENT_DAYS);
			return true;
		}

		// extend
		if(pHouse->ExtendRentDays(ReasonNumber))
		{
			GS()->Chat(ClientID, "Your house was extended by {# total (day|days)}.", ReasonNumber);
			pPlayer->m_VotesData.UpdateCurrentVotes();
			return true;
		}

		// failed
		GS()->Chat(ClientID, "Not enough gold.");
		return true;
	}

	return false;
}

bool CHouseManager::OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd)
{
	// buy house
	if(PPSTR(pCmd, "HOUSE_BUY") == 0)
	{
		const auto& [pHouse] = pMotdData->GetCurrent()->Unpack<CHouse*>();
		if(pHouse)
			pHouse->Buy(pPlayer);

		return true;
	}

	return false;
}

void CHouseManager::ShowSell(CPlayer* pPlayer) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Selling a house (Information)");
	VInfo.Add("The gold will be returned by inbox.");
	VoteWrapper::AddEmptyline(ClientID);

	// sell house
	VoteWrapper(ClientID).AddOption("HOUSE_SELL", "Sell. (in reason send 777)");
	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowDoorsController(CPlayer* pPlayer) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	int DoorsNum = pHouse->GetDoorManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 House doors information");
	VInfo.Add("You can control your doors in the house");
	VInfo.Add("Your home has: {} doors.", DoorsNum);
	VoteWrapper::AddEmptyline(ClientID);

	// doors control
	VoteWrapper VDoors(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2743 Door's control");
	for(auto& [Number, DoorData] : pHouse->GetDoorManager()->GetContainer())
	{
		bool StateDoor = DoorData->IsClosed();
		VDoors.AddOption("HOUSE_DOOR", Number, "[{}] {} door", StateDoor ? "Closed" : "Open", DoorData->GetName());
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowFarmzonesControl(CPlayer* pPlayer) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	const auto FarmzonesSize = (int)pHouse->GetFarmzonesManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Farm zones information");
	VInfo.Add("You can control your farm zones in the house");
	VInfo.Add("Your home has: {} farm zones.", FarmzonesSize);
	VoteWrapper::AddEmptyline(ClientID);

	// farm zones control
	VoteWrapper VFarmzones(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2743 Farm zone's control");
	for(auto& [ID, Farmzone] : pHouse->GetFarmzonesManager()->GetContainer())
	{
		const auto sizeItems = Farmzone.GetNode().m_vItems.size();
		VFarmzones.AddMenu(MENU_HOUSE_FARMZONE_SELECT, ID, "Farm {} zone / {} variety", Farmzone.GetName(), sizeItems);
	}

	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowFarmzoneEdit(CPlayer* pPlayer, int FarmzoneID) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	auto* pFarmzone = pHouse->GetFarmzonesManager()->GetFarmzoneByID(FarmzoneID);
	if(!pFarmzone)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	auto& Node = pFarmzone->GetNode();

	// information
	VoteWrapper VInfo(ClientID, VWF_ALIGN_TITLE | VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2741 Farm {} zone", pFarmzone->GetName());
	VInfo.Add("You can grow a plant on the property");
	VInfo.Add("Chance: {}%", s_GuildChancePlanting);
	VoteWrapper::AddEmptyline(ClientID);

	// planted list
	for(auto& Elem : Node.m_vItems)
	{
		auto ItemID = Elem.Element;
		auto* pItemInfo = GS()->GetItemInfo(ItemID);
		VoteWrapper VPlanted(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "{} - chance {~.2}%", pItemInfo->GetName(), Elem.Chance);
		VPlanted.AddOption("HOUSE_FARMZONE_REMOVE_PLANT", FarmzoneID, ItemID, "Remove {} from plant", pItemInfo->GetName());
	}
	VoteWrapper::AddEmptyline(ClientID);

	// items list availables can be planted
	auto vItems = CInventoryManager::GetItemIDsCollectionByType(ItemType::ResourceHarvestable);
	VoteWrapper VPossiblePlanting(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2741 Possible items for planting");
	for(auto& ID : vItems)
	{
		bool AllowPlant = true;
		for(auto& Elem : Node.m_vItems)
		{
			if(Elem.Element == ID)
			{
				AllowPlant = false;
				break;
			}
		}

		auto* pPlayerItem = pPlayer->GetItem(ID);
		if(AllowPlant && pPlayerItem->HasItem())
		{
			VPossiblePlanting.AddOption("HOUSE_FARMZONE_TRY_PLANT", FarmzoneID, ID, "Try plant {} (has {})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
		}
	}

	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowDetail(CPlayer* pPlayer, CHouse* pHouse)
{
	// check valid player and house
	if(!pHouse || !pPlayer)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	const char* pOwnerNickname = pHouse->HasOwner() ? Instance::Server()->GetAccountNickname(pHouse->GetAccountID()) : "No owner";

	// house detail purchease
	MotdMenu MHouseDetail(ClientID, MTFLAG_CLOSE_BUTTON, "Every player has the right to rent houses.\nAn admission price is required for rentals.");
	MHouseDetail.AddText("Class: {}", pHouse->GetClassName());
	MHouseDetail.AddText("Rent: {$}", pHouse->GetRentPrice());
	MHouseDetail.AddSeparateLine();
	if(!pHouse->HasOwner())
	{
		MHouseDetail.AddText("Price: {$}", pHouse->GetInitialFee());
		MHouseDetail.AddOption("HOUSE_BUY", "Purchase").Pack(pHouse);
	}
	else
	{
		MHouseDetail.AddText("Owner: {}", pOwnerNickname);
		MHouseDetail.AddText("Days left: {}", pHouse->GetRentDays());
	}
	MHouseDetail.AddSeparateLine();
	MHouseDetail.Send(MOTD_MENU_PLAYER_HOUSE_DETAIL);
}

void CHouseManager::ShowMenu(CPlayer* pPlayer) const
{
	// check valid house
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2747 Information about house");
	VInfo.Add("Class: {}", pHouse->GetClassName());
	VInfo.Add("Bank: {$}", pHouse->GetBankManager()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// house bank
	VoteWrapper VBank(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2727 Bank Management (rented for {# (day|days)})", pHouse->GetRentDays());
	VBank.Add("Your: {$} | Bank: {$}", pPlayer->Account()->GetTotalGold(), pHouse->GetBankManager()->Get());
	VBank.AddOption("HOUSE_BANK_ADD", "Add. (amount in a reason)");
	VBank.AddOption("HOUSE_BANK_TAKE", "Take. (amount in a reason)");
	VBank.AddLine();
	VBank.Add("Bank: {$} | Rent per day: {$} gold", pHouse->GetBankManager()->Get(), pHouse->GetRentPrice());
	VBank.AddOption("HOUSE_EXTEND_RENT", "Extend. (amount in a reason)");
	VoteWrapper::AddEmptyline(ClientID);

	// house management
	VoteWrapper VManagement(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2302 House Management");
	VManagement.AddOption("HOUSE_TELEPORT", "Teleport to house");
	VManagement.AddOption("HOUSE_DECORATION", "Decoration editor");
	VManagement.AddMenu(MENU_HOUSE_FARMZONE_LIST, "Farms");
	VManagement.AddMenu(MENU_HOUSE_DOOR_LIST, "Doors");
	VManagement.AddMenu(MENU_HOUSE_SELL, "Sell");
}

CHouse* CHouseManager::GetHouse(HouseIdentifier ID) const
{
	auto pHouse = std::ranges::find_if(CHouse::Data(), [ID](auto& p)
	{
		return p->GetID() == ID;
	});

	return pHouse != CHouse::Data().end() ? *pHouse : nullptr;
}

CHouse* CHouseManager::GetHouseByPos(vec2 Pos) const
{
	const auto switchNumber = GS()->Collision()->GetSwitchTileNumberAtTileIndex(Pos, TILE_SW_HOUSE_ZONE);
	if(!switchNumber)
		return nullptr;

	auto pHouse = std::ranges::find_if(CHouse::Data(), [&](auto& p)
	{
		return *switchNumber == p->GetID();
	});

	return pHouse != CHouse::Data().end() ? *pHouse : nullptr;
}

CFarmzone* CHouseManager::GetHouseFarmzoneByPos(vec2 Pos) const
{
	const auto switchNumber = GS()->Collision()->GetSwitchTileNumberAtTileIndex(Pos, TILE_SW_HOUSE_ZONE);
	if(!switchNumber)
		return nullptr;

	for(auto& p : CHouse::Data())
	{
		if(*switchNumber != p->GetID())
			continue;

		for(auto& Farmzone : p->GetFarmzonesManager()->GetContainer())
		{
			if(distance(Pos, Farmzone.second.GetPos()) < Farmzone.second.GetRadius())
				return &Farmzone.second;
		}
	}

	return nullptr;
}
