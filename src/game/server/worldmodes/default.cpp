/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "default.h"

#include <game/server/gamecontext.h>
#include <game/server/core/entities/logic/logicwall.h>

#include "game/server/core/entities/items/money_bag.h"
#include "game/server/core/tools/path_finder.h"

CGameControllerDefault::CGameControllerDefault(CGS *pGS)
: IGameController(pGS)
{
	m_GameFlags = 0;
	m_MoneyBagTick = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvGenerateMoneyBagPerMinute * 60);
}

void CGameControllerDefault::Tick()
{
	TryGenerateMoneyBag();
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

void CGameControllerDefault::TryGenerateMoneyBag()
{
	// try get prepared path
	if(m_PathMoneyBag.TryGetPath())
	{
		new CEntityMoneyBag(&GS()->m_World, m_PathMoneyBag.vPath.back());
		m_PathMoneyBag.Reset();
	}

	// prepare random getter pos
	if(m_MoneyBagTick < Server()->Tick())
	{
		vec2 Pos;
		CanSpawn(SPAWN_HUMAN, &Pos);
		const auto Radius = (float)(GS()->Collision()->GetHeight() * GS()->Collision()->GetWidth());

		GS()->PathFinder()->RequestRandomPath(m_PathMoneyBag, Pos, Radius);
		m_MoneyBagTick = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvGenerateMoneyBagPerMinute * 60);
		GS()->Chat(-1, "A Money Bag has appeared in the area! Who will claim it?");
	}
}
