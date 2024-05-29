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

	// calculate polar coordinates
	const vec2 roundPos = vec2((float)(round_to_int(Pos.x) / 32 * 32), (float)(round_to_int(Pos.y) / 32 * 32));
	const float iter = 16.f / (float)g_Config.m_SvAmountHarvestingOnTile;
	const float multiplier = iter * 2;

	// entity farming point
	if(Index == ENTITY_FARMING)
	{
		for(int i = 0; i < g_Config.m_SvAmountHarvestingOnTile; i++)
		{
			// calculate polar coordinates
			const float calculate = iter + (float)i * multiplier;
			vec2 newPos = vec2(roundPos.x + calculate, Pos.y);
			if(GS()->Collision()->GetCollisionAt(roundPos.x - iter, Pos.y) || GS()->Collision()->GetCollisionAt(roundPos.x + (30.f + iter), Pos.y))
				newPos = vec2(Pos.x, roundPos.y + calculate);

			// default farm positions
			if(auto* pItemInfo = GS()->Core()->AccountFarmingManager()->GetFarmingItemInfoByPos(newPos))
				new CEntityHarvestingItem(&GS()->m_World, pItemInfo->GetID(), newPos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_FARMING);

			// guild house farm positions
			if(CGuildHouse::CFarmzone* pFarmzone = GS()->Core()->GuildManager()->GetHouseFarmzoneByPos(newPos))
				pFarmzone->Add(new CEntityHarvestingItem(&GS()->m_World, pFarmzone->GetItemID(), newPos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_FARMING));

			// house farm positions
			if(CHouse::CFarmzone* pFarmzone = GS()->Core()->HouseManager()->GetHouseFarmzoneByPos(newPos))
				pFarmzone->Add(new CEntityHarvestingItem(&GS()->m_World, pFarmzone->GetItemID(), newPos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_FARMING));
		}
		return true;
	}

	// entity mining point
	if(Index == ENTITY_MINING)
	{
		for(int i = 0; i < g_Config.m_SvAmountHarvestingOnTile; i++)
		{
			// calculate polar coordinates
			const float calculate = iter + (float)i * multiplier;
			vec2 newPos = vec2(roundPos.x + calculate, Pos.y);
			if(GS()->Collision()->GetCollisionAt(roundPos.x - iter, Pos.y) || GS()->Collision()->GetCollisionAt(roundPos.x + (30.f + iter), Pos.y))
				newPos = vec2(Pos.x, roundPos.y + calculate);

			// default ores positions
			if(auto* pItemInfo = GS()->Core()->AccountMiningManager()->GetMiningItemInfoByPos(newPos))
				new CEntityHarvestingItem(&GS()->m_World, pItemInfo->GetID(), newPos, CEntityHarvestingItem::HARVESTINGITEM_TYPE_MINING);
		}
		return true;
	}

	return false;
}

void CGameControllerDefault::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
}

void CGameControllerDefault::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	IGameController::OnCharacterDamage(pFrom, pTo, Damage);
}
