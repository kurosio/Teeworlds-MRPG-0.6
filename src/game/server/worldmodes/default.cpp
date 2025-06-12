/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "default.h"

#include <game/server/gamecontext.h>
#include <game/server/core/entities/logic/logicwall.h>

#include "game/server/core/entities/items/money_bag.h"
#include "game/server/core/tools/path_finder.h"

constexpr int MAX_MONEY_BAGS_ON_WORLD = 30;

CGameControllerDefault::CGameControllerDefault(CGS* pGS)
	: IGameController(pGS)
{
	m_GameFlags = 0;
	m_vMoneyBags.reserve(MAX_MONEY_BAGS_ON_WORLD);
	m_MoneyBagTick = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvGenerateMoneyBagPerMinute * 60);
}

void CGameControllerDefault::Tick()
{
	TryGenerateMoneyBag();
	IGameController::Tick();
}

void CGameControllerDefault::TryGenerateMoneyBag()
{
	if(m_vMoneyBags.size() >= MAX_MONEY_BAGS_ON_WORLD)
		return;

	// try get prepared path
	if(m_PathMoneyBag.TryGetPath())
	{
		m_vMoneyBags.push_back(new CEntityMoneyBag(&GS()->m_World, m_PathMoneyBag.vPath.back()));
		m_PathMoneyBag.Reset();
	}

	// prepare random getter pos
	if(m_MoneyBagTick < Server()->Tick())
	{
		vec2 Pos;
		if(CanSpawn(SPAWN_HUMAN, &Pos))
		{
			const auto Radius = (float)(GS()->Collision()->GetHeight() * GS()->Collision()->GetWidth());
			GS()->PathFinder()->RequestRandomPath(m_PathMoneyBag, Pos, Radius);
			GS()->ChatWorld(GS()->GetWorldID(), nullptr, "A Money Bag has appeared in the area! Who will claim it?");
		}

		m_MoneyBagTick = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvGenerateMoneyBagPerMinute * 60);
	}
}