/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "default.h"

#include <game/server/gamecontext.h>
#include <game/server/core/entities/items/jobitems.h>
#include <game/server/core/entities/logic/logicwall.h>

#include <game/server/core/components/Accounts/AccountMinerManager.h>
#include <game/server/core/components/Accounts/AccountPlantManager.h>
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

	if(Index == ENTITY_PLANTS)
	{
		const int ItemID = GS()->Core()->AccountPlantManager()->GetPlantItemID(Pos), Level = GS()->Core()->AccountPlantManager()->GetPlantLevel(Pos);
		if(ItemID > 0)
		{
			new CJobItems(&GS()->m_World, ItemID, Level, Pos, CJobItems::JOB_ITEM_FARMING, 100);
			return true;
		}

		if(CGuildHouse::CPlantzone* pPlantzone = GS()->Core()->GuildManager()->GetGuildHousePlantzoneByPos(Pos))
		{
			pPlantzone->Add(new CJobItems(&GS()->m_World, pPlantzone->GetItemID(), 1, Pos, CJobItems::JOB_ITEM_FARMING, 100));
			return true;
		}

		return true;
	}

	if(Index == ENTITY_MINER)
	{
		const int ItemID = GS()->Core()->AccountMinerManager()->GetOreItemID(Pos), Level = GS()->Core()->AccountMinerManager()->GetOreLevel(Pos);
		if(ItemID > 0)
		{
			const int Health = GS()->Core()->AccountMinerManager()->GetOreHealth(Pos);
			new CJobItems(&GS()->m_World, ItemID, Level, Pos, CJobItems::JOB_ITEM_MINING, Health);
		}

		return true;
	}

	return false;
}

void CGameControllerDefault::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
}