/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/mapitems.h>
#include "gamecontroller.h"

#include "gamecontext.h"

#include "entities/pickup.h"
#include "core/entities/logic/botwall.h"
#include "core/entities/items/gathering_node.h"

#include "core/components/achievements/achievement_data.h"
#include "core/components/guilds/guild_manager.h"
#include "core/components/houses/house_manager.h"

#include "entities/character_bot.h"

/*
	Here you need to put it in order make more events
	For modes that each map can have one of them
*/

IGameController::IGameController(CGS* pGS)
{
	m_pGS = pGS;
	m_GameFlags = 0;
	m_pServer = m_pGS->Server();
}

void IGameController::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	g_EventListenerManager.Notify<IEventListener::CharacterDamage>(pFrom, pTo, Damage);
}

void IGameController::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	g_EventListenerManager.Notify<IEventListener::Type::CharacterDeath>(pVictim, pKiller, Weapon);

	// update rating
	if(pVictim && pKiller && pVictim != pKiller && !pVictim->IsBot() && !pKiller->IsBot())
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
		pVictim->GetTempData().m_LastKilledByWeapon = Weapon;

		// Clear all effects on the player
		if(Weapon != WEAPON_WORLD)
		{
			pVictim->m_Effects.RemoveAll();
			pVictim->UpdateTempData(0, 0);
		}
	}
}

bool IGameController::OnCharacterSpawn(CCharacter* pChr)
{
	g_EventListenerManager.Notify<IEventListener::Type::CharacterSpawn>(pChr->GetPlayer());

	// Health
	pChr->IncreaseHealth(pChr->GetPlayer()->GetMaxHealth());
	pChr->IncreaseMana(3);

	// Weapons
	const int MaximumAmmo = 10 + pChr->GetPlayer()->GetTotalAttributeValue(AttributeIdentifier::Ammo);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	for(int i = WEAPON_GUN; i < NUM_WEAPONS - 1; i++)
		pChr->GiveWeapon(i, MaximumAmmo);

	// eidolons
	pChr->GetPlayer()->TryCreateEidolon();
	return true;
}

bool IGameController::OnCharacterBotSpawn(CCharacterBotAI* pChr)
{
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

void IGameController::OnEntity(int Index, vec2 Pos, int Flags)
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
		new CBotWall(&GS()->m_World, Pos, Direction, CBotWall::Flags::WALLLINEFLAG_AGRESSED_BOT);
	}

	else if(Index == ENTITY_PLANT)
	{
		const auto roundPos = vec2(round_to_int(Pos.x) / 32 * 32, round_to_int(Pos.y) / 32 * 32);
		const auto range = 16.f / (float)g_Config.m_SvGatheringEntitiesPerTile;
		const auto multiplier = range * 2;

		// entity farming point
		for(int i = 0; i < g_Config.m_SvGatheringEntitiesPerTile; i++)
		{
			const float offset = range + i * multiplier;
			auto newPos = vec2(roundPos.x + offset, Pos.y);

			// handle collision flags to adjust the position
			if(GS()->Collision()->GetCollisionFlagsAt(roundPos.x - range, Pos.y) ||
				GS()->Collision()->GetCollisionFlagsAt(roundPos.x + (30.f + range), Pos.y))
			{
				newPos = vec2(Pos.x, roundPos.y + offset);
			}

			// try create guild house plant
			if(auto* pFarmzone = GS()->Core()->GuildManager()->GetHouseFarmzoneByPos(newPos))
				pFarmzone->Add(GS(), newPos);

			// try create house plant
			if(auto* pFarmzone = GS()->Core()->HouseManager()->GetHouseFarmzoneByPos(newPos))
				pFarmzone->Add(GS(), newPos);
		}
	}
}

void IGameController::OnEntitySwitch(int Index, vec2 Pos, int Flags, int Number)
{
	if(Index == ENTITY_PLANT || Index == ENTITY_ORE)
	{
		const auto roundPos = vec2(round_to_int(Pos.x) / 32 * 32, round_to_int(Pos.y) / 32 * 32);
		const auto range = 16.f / (float)g_Config.m_SvGatheringEntitiesPerTile;
		const auto multiplier = range * 2;

		// initialize gathering node and type
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

		// create entities for node
		if(pNode)
		{
			for(int i = 0; i < g_Config.m_SvGatheringEntitiesPerTile; i++)
			{
				const float offset = range + i * multiplier;
				auto newPos = vec2(roundPos.x + offset, Pos.y);

				// handle collision flags to adjust the position
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

void IGameController::OnPlayerConnect(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID) && pPlayer->GetCurrentWorldID() == GS()->GetWorldID())
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), pPlayer->GetTeam());
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
		UpdateGameInfo(ClientID);
	}
}

void IGameController::OnPlayerDisconnect(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(Server()->ClientIngame(ClientID) && pPlayer->GetCurrentWorldID() == GS()->GetWorldID())
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
		GS()->Core()->SaveAccount(pPlayer, SAVE_POSITION);
	}

	pPlayer->OnDisconnect();
}

// general
void IGameController::Snap()
{
	// vanilla snap
	CNetObj_GameInfo* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	pGameInfoObj->m_RoundStartTick = Server()->GetOffsetGameTime();
	pGameInfoObj->m_WarmupTimer = 0;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	// ddnet snap
	CNetObj_GameInfoEx* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_ALLOW_ZOOM | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE | GAMEINFOFLAG2_HUD_HEALTH_ARMOR | GAMEINFOFLAG2_HUD_AMMO;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}

void IGameController::Tick() { }

void IGameController::UpdateGameInfo(int ClientID)
{
	/*	CNetMsg_Sv_GameInfo GameInfoMsg;
		GameInfoMsg.m_GameFlags = m_GameFlags;
		GameInfoMsg.m_ScoreLimit = 0;
		GameInfoMsg.m_TimeLimit = 0;
		GameInfoMsg.m_MatchNum = 0;
		GameInfoMsg.m_MatchCurrent = 0;

		if(ClientID == -1)
		{
			for(int i = 0; i < MAX_PLAYERS; ++i)
			{
				if(!GS()->m_apPlayers[i] || !Server()->ClientIngame(i))
					continue;

				if((!GS()->IsMmoClient(i) && Server()->GetClientProtocolVersion(i) < MIN_RACE_CLIENTVERSION))
					GameInfoMsg.m_GameFlags &= ~GAMEFLAG_RACE;

				Server()->SendPackMsg(&GameInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i, Server()->GetClientWorldID(ClientID));
			}
		}
		else
		{
			if((!GS()->IsMmoClient(ClientID) && Server()->GetClientProtocolVersion(ClientID) < MIN_RACE_CLIENTVERSION))
				GameInfoMsg.m_GameFlags &= ~GAMEFLAG_RACE;

			Server()->SendPackMsg(&GameInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID, Server()->GetClientWorldID(ClientID));
		}*/
}

bool IGameController::CanSpawn(int SpawnType, vec2* pOutPos, std::pair<vec2, float> LimiterSpread) const
{
	if(SpawnType < SPAWN_HUMAN || SpawnType >= NUM_SPAWN)
		return false;

	CSpawnEval Eval;
	EvaluateSpawnType(&Eval, SpawnType, LimiterSpread);
	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void IGameController::EvaluateSpawnType(CSpawnEval* pEval, int SpawnType, std::pair<vec2, float> LimiterSpread) const
{
	for(auto& currentPos : m_aaSpawnPoints[SpawnType])
	{
		if(LimiterSpread.second >= 1.f)
		{
			if(distance(LimiterSpread.first, currentPos) > LimiterSpread.second)
				continue;
		}

		CCharacter* aEnts[MAX_CLIENTS];
		int Num = GS()->m_World.FindEntities(currentPos, 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		const vec2 Positions[5] = {
			vec2(0.0f, 0.0f),
			vec2(-32.0f, 0.0f),
			vec2(0.0f, -32.0f),
			vec2(32.0f, 0.0f),
			vec2(0.0f, 32.0f)
		};

		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			vec2 spawnPos = currentPos + Positions[Index];
			bool isValid = true;

			for(int c = 0; c < Num; ++c)
			{
				if(GS()->Collision()->CheckPoint(spawnPos) ||
					distance(aEnts[c]->GetPos(), spawnPos) <= aEnts[c]->GetRadius())
				{
					isValid = false;
					break;
				}
			}

			if(isValid)
			{
				Result = Index;
			}
		}

		if(Result == -1)
			continue;

		const vec2 spawnPos = currentPos + Positions[Result];
		if(!pEval->m_Got)
		{
			pEval->m_Got = true;
			pEval->m_Pos = spawnPos;
		}
	}
}

void IGameController::DoTeamChange(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int Team = pPlayer->GetTeam();

	pPlayer->GetTempData().m_LastKilledByWeapon = WEAPON_WORLD;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientID, Server()->ClientName(ClientID), Team);
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	Server()->ExpireServerInfo();
}
