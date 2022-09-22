/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "StorageCore.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

void CStorageCore::OnInit()
{
	const auto InitStorages = Sqlpool.Prepare<DB::SELECT>("*", "tw_storages");
	InitStorages->AtExecute([](IServer*, ResultPtr pRes)
	{
		while (pRes->next())
		{
			const int ID = pRes->getInt("ID");
			CStorageData::ms_aStorage[ID].m_PosX = pRes->getInt("PosX");
			CStorageData::ms_aStorage[ID].m_PosY = pRes->getInt("PosY");
			CStorageData::ms_aStorage[ID].m_Currency = pRes->getInt("Currency");
			CStorageData::ms_aStorage[ID].m_WorldID = pRes->getInt("WorldID");
			str_copy(CStorageData::ms_aStorage[ID].m_aName, pRes->getString("Name").c_str(), sizeof(CStorageData::ms_aStorage[ID].m_aName));
		}
	});
}

bool CStorageCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_SHOP_ZONE))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_SHOP_ZONE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = false;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	return false;
}

bool CStorageCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
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

int CStorageCore::GetStorageID(vec2 Pos) const
{
	const auto pStorage = std::find_if(CStorageData::ms_aStorage.begin(), CStorageData::ms_aStorage.end(), [Pos](const std::pair < int, CStorageData >& pItem)
	{
		const vec2 PosStorage(pItem.second.m_PosX, pItem.second.m_PosY);
		return (distance(PosStorage, Pos) < 200);
	});
	return pStorage != CStorageData::ms_aStorage.end() ? (*pStorage).first : -1;
}

void CStorageCore::ShowStorageMenu(CPlayer* pPlayer, int StorageID)
{
	const int ClientID = pPlayer->GetCID();
	if(CStorageData::ms_aStorage.find(StorageID) == CStorageData::ms_aStorage.end())
	{
		GS()->AV(ClientID, "null", "Storage Don't work");
		return;
	}

	GS()->AVH(ClientID, TAB_STORAGE, "Shop :: {STR}", CStorageData::ms_aStorage[StorageID].m_aName);
	GS()->AVM(ClientID, "REPAIRITEMS", StorageID, TAB_STORAGE, "Repair all items - FREE");
	GS()->AV(ClientID, "null");
	GS()->ShowVotesItemValueInformation(pPlayer, CStorageData::ms_aStorage[StorageID].m_Currency);
	GS()->AV(ClientID, "null");
}
