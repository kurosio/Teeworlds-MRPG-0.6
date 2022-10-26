/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "main.h"

#include <game/server/gamecontext.h>
#include <game/server/mmocore/GameEntities/jobitems.h>
#include <game/server/mmocore/GameEntities/npcwall.h>
#include <game/server/mmocore/GameEntities/Logics/logicwall.h>

#include <game/server/mmocore/Components/Accounts/AccountMinerCore.h>
#include <game/server/mmocore/Components/Accounts/AccountPlantCore.h>
#include <game/server/mmocore/Components/Houses/HouseCore.h>

CGameControllerMain::CGameControllerMain(class CGS *pGS)
: IGameController(pGS)
{
	m_GameFlags = 0;
}

void CGameControllerMain::Tick()
{
	IGameController::Tick();
}

void CGameControllerMain::CreateLogic(int Type, int Mode, vec2 Pos, int ParseInt)
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

bool CGameControllerMain::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	if(Index == ENTITY_NPC_WALLUP)
	{
		new CNPCWall(&GS()->m_World, Pos, false);
		return true;
	}

	if(Index == ENTITY_NPC_WALLLEFT)
	{
		new CNPCWall(&GS()->m_World, Pos, true);
		return true;
	}

	if(Index == ENTITY_PLANTS)
	{
		const int HouseID = GS()->Mmo()->House()->GetHouse(Pos, true);
		const int PlantsID = GS()->Mmo()->House()->GetPlantsID(HouseID);
		if(HouseID > 0 && PlantsID > 0)
		{
			new CJobItems(&GS()->m_World, PlantsID, 1, Pos, CJobItems::JOB_ITEM_FARMING, 100, HouseID);
			return true;
		}

		const int ItemID = GS()->Mmo()->PlantsAcc()->GetPlantItemID(Pos), Level = GS()->Mmo()->PlantsAcc()->GetPlantLevel(Pos);
		if(ItemID > 0)
			new CJobItems(&GS()->m_World, ItemID, Level, Pos, CJobItems::JOB_ITEM_FARMING, 100);

		return true;
	}

	if(Index == ENTITY_MINER)
	{
		const int ItemID = GS()->Mmo()->MinerAcc()->GetOreItemID(Pos), Level = GS()->Mmo()->MinerAcc()->GetOreLevel(Pos);
		if(ItemID > 0)
		{
			const int Health = GS()->Mmo()->MinerAcc()->GetOreHealth(Pos);
			new CJobItems(&GS()->m_World, ItemID, Level, Pos, CJobItems::JOB_ITEM_MINING, Health);
		}

		return true;
	}

	return false;
}