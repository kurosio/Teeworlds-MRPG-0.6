/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/mapitems.h>
#include "gamecontroller.h"

#include "gamecontext.h"

IGameController::IGameController(CGS* pGS)
{
	m_pGS = pGS;
	m_GameFlags = 0;
	m_pServer = m_pGS->Server();
}

void IGameController::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage) {}
void IGameController::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) {}
bool IGameController::OnCharacterSpawn(CCharacter* pChr)
{
	return true;
}

bool IGameController::OnCharacterBotSpawn(CCharacterBotAI* pChr)
{
	return true;
}

void IGameController::OnEntity(int Index, vec2 Pos, int Flags) {}
void IGameController::OnEntitySwitch(int Index, vec2 Pos, int Flags, int Number) {}
void IGameController::OnPlayerConnect(CPlayer* pPlayer) {}
void IGameController::OnPlayerDisconnect(CPlayer* pPlayer) {}
void IGameController::Tick() { }
void IGameController::UpdateGameInfo(int ClientID) { }

void IGameController::SnapGameInfo(int RoundStartTick, int WarmupTimer, int GameStateFlags, int Flags, int Flags2) const
{
	auto* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = GameStateFlags;
	pGameInfoObj->m_RoundStartTick = RoundStartTick;
	pGameInfoObj->m_WarmupTimer = WarmupTimer;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	auto* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = Flags;
	pGameInfoEx->m_Flags2 = Flags2;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}

// general
void IGameController::Snap()
{
	constexpr int Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_ALLOW_ZOOM
		| GAMEINFOFLAG_PREDICT_VANILLA | GAMEINFOFLAG_PREDICT_DDRACE_TILES;
	constexpr int Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE
		| GAMEINFOFLAG2_HUD_HEALTH_ARMOR | GAMEINFOFLAG2_HUD_AMMO | GAMEINFOFLAG2_NO_WEAK_HOOK;
	SnapGameInfo(Server()->GetOffsetGameTime(), 0, 0, Flags, Flags2);
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

float IGameController::EvaluateSpawnPos(CSpawnEval* pEval, vec2 Pos) const
{
	float Score = 0.0f;
	CCharacter* pC = dynamic_cast<CCharacter*>(GS()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));

	while(pC)
	{
		float d = distance(Pos, pC->GetPos());
		Score += (d == 0.f) ? 1000000000.0f : 1.0f / d;
		pC = static_cast<CCharacter*>(pC->TypeNext());
	}

	return Score;
}

bool IsSpawnPointValid(CGS* pGS, const vec2& SpawnPoint)
{
	CEntity* apEnts[MAX_CLIENTS];
	int Num = pGS->m_World.FindEntities(SpawnPoint, 64, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	vec2 aPositions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) }; // start, left, up, right, down

	for(int Index = 0; Index < 5; ++Index)
	{
		for(int c = 0; c < Num; ++c)
		{
			auto* pChr = static_cast<CCharacter*>(apEnts[c]);
			if(pGS->Collision()->CheckPoint(SpawnPoint + aPositions[Index]) ||
				distance(pChr->GetPos(), SpawnPoint + aPositions[Index]) <= pChr->GetRadius())
				return false;
		}
	}

	return true;
}

void IGameController::EvaluateSpawnType(CSpawnEval* pEval, int SpawnType, std::pair<vec2, float> LimiterSpread) const
{
	for(int j = 0; j < 2 && !pEval->m_Got; ++j)
	{
		// Iterate through spawn points for the given type
		for(const auto& SpawnPoint : m_aaSpawnPoints[SpawnType])
		{
			vec2 P = SpawnPoint;
			if(LimiterSpread.second >= 1.f && distance(LimiterSpread.first, P) > LimiterSpread.second)
			{
				// Skip if outside the limiter range
				continue;
			}

			if(j == 0 && !IsSpawnPointValid(GS(), SpawnPoint))
			{
				// Check for collisions and other entities
				continue;
			}

			float S = EvaluateSpawnPos(pEval, P);
			if(!pEval->m_Got || (j == 0 && pEval->m_Score > S))
			{
				pEval->m_Got = true;
				pEval->m_Score = S;
				pEval->m_Pos = P;
			}
		}
	}
}

void IGameController::DoTeamChange(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int Team = pPlayer->GetTeam();

	pPlayer->GetSharedData().m_LastKilledByWeapon = WEAPON_WORLD;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", ClientID, Server()->ClientName(ClientID), Team);
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	Server()->ExpireServerInfo();
}
