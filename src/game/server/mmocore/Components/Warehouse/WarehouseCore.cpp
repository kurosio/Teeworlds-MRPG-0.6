/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "WarehouseCore.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

void CWarehouseCore::OnInit()
{
	const auto InitStorages = Sqlpool.Prepare<DB::SELECT>("*", "tw_warehouses");
	InitStorages->AtExecute([](IServer*, ResultPtr pRes)
	{
		while (pRes->next())
		{
			const int ID = pRes->getInt("ID");
			std::string Name = pRes->getString("Name").c_str();
			vec2 Pos = vec2((float)pRes->getInt("PosX"), (float)pRes->getInt("PosY"));
			int Currency = pRes->getInt("Currency");
			int WorldID = pRes->getInt("WorldID");

			CWarehouse(ID).Init(Name, Pos, Currency, WorldID);
		}
	});
}

bool CWarehouseCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_SHOP_ZONE))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_SHOP_ZONE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = false;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	return false;
}

bool CWarehouseCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "REPAIRITEMS") == 0)
	{
		Job()->Item()->RepairDurabilityItems(pPlayer);
		GS()->Chat(ClientID, "You repaired all items.");
		return true;
	}

	return false;
}

int CWarehouseCore::GetWarehouseID(vec2 Pos) const
{
	const auto pStorage = std::find_if(CWarehouse::Data().begin(), CWarehouse::Data().end(), [Pos](const auto& pItem)
	{
		return (distance(pItem.second.GetPos(), Pos) < 200);
	});
	return pStorage != CWarehouse::Data().end() ? (*pStorage).first : -1;
}

void CWarehouseCore::ShowWarehouseMenu(CPlayer* pPlayer, int WarehouseID)
{
	const int ClientID = pPlayer->GetCID();
	if(CWarehouse::Data().find(WarehouseID) == CWarehouse::Data().end())
	{
		GS()->AV(ClientID, "null", "Storage Don't work");
		return;
	}

	GS()->AVH(ClientID, TAB_STORAGE, "Shop :: {STR}", CWarehouse::Data()[WarehouseID].GetName());
	GS()->AVM(ClientID, "REPAIRITEMS", WarehouseID, TAB_STORAGE, "Repair all items - FREE");
	GS()->AV(ClientID, "null");
	GS()->ShowVotesItemValueInformation(pPlayer, CWarehouse::Data()[WarehouseID].GetCurrency()->GetID());
	GS()->AV(ClientID, "null");
}
