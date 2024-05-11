/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "default.h"

#include <game/server/gamecontext.h>
#include <game/server/core/entities/items/harvesting_item.h>
#include <game/server/core/entities/logic/logicwall.h>

#include <game/server/core/components/Accounts/AccountMiningManager.h>
#include <game/server/core/components/Accounts/AccountFarmingManager.h>
#include <game/server/core/components/houses/house_manager.h>

#include "game/server/core/components/guilds/guild_manager.h"
#include "game/server/core/components/guilds/guild_house_data.h"

CGameControllerDefault::CGameControllerDefault(class CGS *pGS)
: IGameController(pGS)
{
	m_GameFlags = 0;
}

void CGameControllerDefault::Tick()
{
	IGameController::Tick();
}

void CGameControllerDefault::CreateLogic(int Type, int Mode, vec2 Pos, int ParseInt)
{
	if(Type == 1)
	{
		new CLogicWall(&GS()->m_World, Pos);
	}
	if(Type == 2)
	{
		new CLogicWallWall(&GS()->m_World, Pos, Mode, ParseInt);
	}
	if(Type == 3)
	{
		new CLogicDoorKey(&GS()->m_World, Pos, ParseInt, Mode);
	}
}

bool CGameControllerDefault::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	if(Index == ENTITY_FARMING)
	{
		// default farm positions
		if(auto* pItemInfo = GS()->Core()->AccountFarmingManager()->GetFarmingItemInfoByPos(Pos))
		{
			new CEntityHarvestingItem(&GS()->m_World, pItemInfo->GetID(), Pos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_FARMING);
			return true;
		}

		// guild house farm positions
		if(CGuildHouse::CFarmzone* pFarmzone = GS()->Core()->GuildManager()->GetHouseFarmzoneByPos(Pos))
		{
			pFarmzone->Add(new CEntityHarvestingItem(&GS()->m_World, pFarmzone->GetItemID(), Pos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_FARMING));
			return true;
		}

		// house farm positions
		if(CHouse::CFarmzone* pFarmzone = GS()->Core()->HouseManager()->GetHouseFarmzoneByPos(Pos))
		{
			pFarmzone->Add(new CEntityHarvestingItem(&GS()->m_World, pFarmzone->GetItemID(), Pos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_FARMING));
			return true;
		}

		return true;
	}

	if(Index == ENTITY_MINING)
	{
		// default ores positions
		if(auto* pItemInfo = GS()->Core()->AccountMiningManager()->GetMiningItemInfoByPos(Pos))
		{
			new CEntityHarvestingItem(&GS()->m_World, pItemInfo->GetID(), Pos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_MINING);
			return true;
		}

		return true;
	}

	return false;
}

void CGameControllerDefault::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
}