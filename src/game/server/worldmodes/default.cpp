/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "default.h"

#include <game/server/gamecontext.h>
#include <game/server/core/entities/logic/logicwall.h>

#include "game/server/entities/pickup.h"
#include "game/server/core/entities/logic/botwall.h"
#include "game/server/core/entities/items/gathering_node.h"
#include "game/server/core/entities/items/money_bag.h"
#include "game/server/core/components/achievements/achievement_data.h"
#include "game/server/core/components/guilds/guild_manager.h"
#include "game/server/core/components/houses/house_manager.h"
#include "game/server/core/tools/path_finder.h"
#include "game/server/entities/character_bot.h"

constexpr int MAX_MONEY_BAGS_ON_WORLD = 30;

CGameControllerDefault::CGameControllerDefault(CGS* pGS)
	: IGameController(pGS)
{
	m_GameFlags = 0;
	m_vMoneyBags.reserve(MAX_MONEY_BAGS_ON_WORLD);
	m_MoneyBagTick = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvGenerateMoneyBagPerMinute * 60);
}

void CGameControllerDefault::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	g_EventListenerManager.Notify<IEventListener::CharacterDamage>(pFrom, pTo, Damage);
}

void CGameControllerDefault::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	g_EventListenerManager.Notify<IEventListener::Type::CharacterDeath>(pVictim, pKiller, Weapon);

	// update rating
	if(GS()->HasWorldFlag(WORLD_FLAG_RATING_SYSTEM) &&
		pVictim && pKiller && pVictim != pKiller && !pVictim->IsBot() && !pKiller->IsBot())
	{
		auto& pKillerRating = pKiller->Account()->GetRatingSystem();
		auto& pVictimRating = pVictim->Account()->GetRatingSystem();
		pKillerRating.UpdateRating(GS(), true, pVictim->Account());
		pVictimRating.UpdateRating(GS(), false, pKiller->Account());
	}

	// update last killed by weapon
	if(pVictim && !pVictim->IsBot())
	{
		pVictim->TryRemoveEidolon();
		pVictim->GetSharedData().m_LastKilledByWeapon = Weapon;

		// Clear all effects on the player
		if(Weapon != WEAPON_WORLD)
		{
			pVictim->m_Effects.RemoveAll();
			pVictim->UpdateSharedCharacterData(0, 0);
		}
	}
}

bool CGameControllerDefault::OnCharacterSpawn(CCharacter* pChr)
{
	g_EventListenerManager.Notify<IEventListener::Type::CharacterSpawn>(pChr->GetPlayer());

	// Health
	const int SpawnMana = GS()->HasWorldFlag(WORLD_FLAG_SPAWN_FULL_MANA) ? pChr->GetPlayer()->GetMaxMana() : 3;
	pChr->IncreaseHealth(pChr->GetPlayer()->GetMaxHealth());
	pChr->IncreaseMana(SpawnMana);

	// Weapons
	const int MaximumAmmo = 10 + pChr->GetPlayer()->GetTotalAttributeValue(AttributeIdentifier::Ammo);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	for(int i = WEAPON_GUN; i < NUM_WEAPONS - 1; i++)
		pChr->GiveWeapon(i, MaximumAmmo);

	// eidolons
	pChr->GetPlayer()->TryCreateEidolon();
	return true;
}

bool CGameControllerDefault::OnCharacterBotSpawn(CCharacterBotAI* pChr)
{
	g_EventListenerManager.Notify<IEventListener::Type::CharacterSpawn>(pChr->GetPlayer());

	auto* pPlayerBot = dynamic_cast<CPlayerBot*>(pChr->GetPlayer());
	const int MaxStartHP = pPlayerBot->GetTotalAttributeValue(AttributeIdentifier::HP);
	const int MaxStartMP = pPlayerBot->GetTotalAttributeValue(AttributeIdentifier::MP);
	pPlayerBot->InitBasicStats(MaxStartHP, MaxStartMP, MaxStartHP, MaxStartMP);

	pChr->IncreaseHealth(MaxStartHP);
	pChr->IncreaseMana(MaxStartMP);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);

	for(int i = WEAPON_GUN; i < WEAPON_LASER; i++)
		pChr->GiveWeapon(i, 10);

	return true;
}

void CGameControllerDefault::OnEntity(int Index, vec2 Pos, int Flags)
{
	if(Index == ENTITY_SPAWN)
	{
		m_aaSpawnPoints[SPAWN_HUMAN].push_back(Pos);
	}

	else if(Index == ENTITY_SPAWN_MOBS)
	{
		m_aaSpawnPoints[SPAWN_BOT].push_back(Pos);
	}

	else if(Index == ENTITY_SPAWN_PRISON)
	{
		m_aaSpawnPoints[SPAWN_HUMAN_PRISON].push_back(Pos);
	}

	else if(Index == ENTITY_ARMOR_1)
	{
		new CPickup(&GS()->m_World, POWERUP_ARMOR, 0, Pos);
	}

	else if(Index == ENTITY_HEALTH_1)
	{
		new CPickup(&GS()->m_World, POWERUP_HEALTH, 0, Pos);
	}

	else if(Index == ENTITY_PICKUP_SHOTGUN)
	{
		new CPickup(&GS()->m_World, POWERUP_WEAPON, WEAPON_SHOTGUN, Pos);
	}

	else if(Index == ENTITY_PICKUP_GRENADE)
	{
		new CPickup(&GS()->m_World, POWERUP_WEAPON, WEAPON_GRENADE, Pos);
	}

	else if(Index == ENTITY_PICKUP_LASER)
	{
		new CPickup(&GS()->m_World, POWERUP_WEAPON, WEAPON_LASER, Pos);
	}

	else if(Index == ENTITY_NPC_WALL)
	{
		vec2 Direction = GS()->Collision()->GetRotateDirByFlags(Flags);
		new CBotWall(&GS()->m_World, Pos, Direction, CBotWall::Flags::WALLLINEFLAG_FRIENDLY_BOT);
	}

	else if(Index == ENTITY_MOB_WALL)
	{
		vec2 Direction = GS()->Collision()->GetRotateDirByFlags(Flags);
		new CBotWall(&GS()->m_World, Pos, Direction, CBotWall::Flags::WALLLINEFLAG_AGGRESSED_BOT);
	}

	else if(Index == ENTITY_PLANT)
	{
		const auto roundPos = vec2(round_to_int(Pos.x) / 32 * 32, round_to_int(Pos.y) / 32 * 32);
		const auto range = 16.f / (float)g_Config.m_SvGatheringEntitiesPerTile;
		const auto multiplier = range * 2;

		for(int i = 0; i < g_Config.m_SvGatheringEntitiesPerTile; i++)
		{
			const float offset = range + i * multiplier;
			auto newPos = vec2(roundPos.x + offset, Pos.y);
			if(GS()->Collision()->GetCollisionFlagsAt(roundPos.x - range, Pos.y) ||
				GS()->Collision()->GetCollisionFlagsAt(roundPos.x + (30.f + range), Pos.y))
			{
				newPos = vec2(Pos.x, roundPos.y + offset);
			}

			if(auto* pFarmzone = GS()->Core()->GuildManager()->GetHouseFarmzoneByPos(newPos))
				pFarmzone->Add(GS(), newPos);
			if(auto* pFarmzone = GS()->Core()->HouseManager()->GetHouseFarmzoneByPos(newPos))
				pFarmzone->Add(GS(), newPos);
		}
	}
}

void CGameControllerDefault::OnEntitySwitch(int Index, vec2 Pos, int Flags, int Number)
{
	if(Index == ENTITY_PLANT || Index == ENTITY_ORE)
	{
		const auto roundPos = vec2(round_to_int(Pos.x) / 32 * 32, round_to_int(Pos.y) / 32 * 32);
		const auto range = 16.f / (float)g_Config.m_SvGatheringEntitiesPerTile;
		const auto multiplier = range * 2;

		GatheringNode* pNode = nullptr;
		int nodeType = 0;
		if(Index == ENTITY_ORE)
		{
			pNode = GS()->Collision()->GetOreNode(Number);
			nodeType = CEntityGatheringNode::GATHERING_NODE_ORE;
		}
		else if(Index == ENTITY_PLANT)
		{
			pNode = GS()->Collision()->GetPlantNode(Number);
			nodeType = CEntityGatheringNode::GATHERING_NODE_PLANT;
		}

		if(pNode)
		{
			for(int i = 0; i < g_Config.m_SvGatheringEntitiesPerTile; i++)
			{
				const float offset = range + i * multiplier;
				auto newPos = vec2(roundPos.x + offset, Pos.y);
				if(GS()->Collision()->GetCollisionFlagsAt(roundPos.x - range, Pos.y) ||
					GS()->Collision()->GetCollisionFlagsAt(roundPos.x + (30.f + range), Pos.y))
				{
					newPos = vec2(Pos.x, roundPos.y + offset);
				}

				new CEntityGatheringNode(&GS()->m_World, pNode, newPos, nodeType);
			}
		}
	}
}

void CGameControllerDefault::OnPlayerConnect(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID) && pPlayer->GetCurrentWorldID() == GS()->GetWorldID())
	{
		GS()->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_DEBUG, "game", "team_join player='%d:%s' team=%d",
			ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam());
		UpdateGameInfo(ClientID);
	}
}

void CGameControllerDefault::OnPlayerDisconnect(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID) && pPlayer->GetCurrentWorldID() == GS()->GetWorldID())
	{
		GS()->Console()->PrintFormat(IConsole::OUTPUT_LEVEL_STANDARD, "game", "leave player='%d:%s'",
			ClientID, Server()->ClientName(ClientID));
		GS()->Core()->SaveAccount(pPlayer, SAVE_POSITION);
	}

	pPlayer->OnDisconnect();
}

void CGameControllerDefault::Tick()
{
	TryGenerateMoneyBag();
	IGameController::Tick();
}

void CGameControllerDefault::TryGenerateMoneyBag()
{
	// clear container for invalid money bags
	std::erase_if(m_vMoneyBags, [this](CEntityMoneyBag* pMoneyBag)
	{ return !GS()->m_World.ExistEntity(pMoneyBag); });

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
