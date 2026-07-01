/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "trueorfalse.h"

#include <game/server/gamecontext.h>
#include <game/server/entities/character.h>
#include <game/server/core/entities/logic/logicwall.h>
#include <game/mapitems.h>

constexpr int TILE_TOF_LOBBY   = 146;
constexpr int TILE_TOF_TRUE    = 24;
constexpr int TILE_TOF_FALSE   = 25;
constexpr int TILE_TOF_NEUTRAL = 79;

static const vec2 s_Wall1Pos = vec2(562.f, 337.f);
static const vec2 s_Wall2Pos = vec2(722.f, 337.f);
static const vec2 s_NeutralCenter = vec2(640.f, 252.f);

CGameControllerTrueOrFalse::CGameControllerTrueOrFalse(CGS* pGS)
	: IGameController(pGS)
{
	m_GameFlags = 0;
}

void CGameControllerTrueOrFalse::OnInit()
{
	m_GameActive = false;
	m_PlayersMovedToNeutral = false;
	m_WallsOpen = false;
	m_QuestionActive = false;
	m_ResultsActive = false;
	m_CorrectAnswer = false;
	m_CountdownTick = 0;
	m_QuestionBroadcastTick = 0;
	m_ResultsTick = 0;
	m_pWall1 = nullptr;
	m_pWall2 = nullptr;

	GS()->Console()->Register("sv_trueorfalse", "r[question_seconds_answer]", CFGFLAG_SERVER, ConTrueOrFalse, this, "Start a True or False question: sv_trueorfalse \"question\" seconds true/false");
	GS()->Console()->Register("sv_trueorfalse_start", "i[active]", CFGFLAG_SERVER, ConTrueOrFalseStart, this, "Start or stop True or False minigame: sv_trueorfalse_start 1/0");
}

void CGameControllerTrueOrFalse::Tick()
{
	if(!m_GameActive)
		return;

	// phase 1: broadcast question for 8 seconds while walls are up
	if(m_QuestionActive && !m_WallsOpen)
	{
		if(m_QuestionBroadcastTick > 0)
		{
			m_QuestionBroadcastTick--;
			GS()->BroadcastWorld(GS()->GetWorldID(), BroadcastPriority::VeryImportant, 10, "Question: {} | Go to TRUE or FALSE zone!", m_aQuestion);
			if(m_QuestionBroadcastTick <= 0)
			{
				DestroyWalls();
				m_WallsOpen = true;
			}
		}
		return;
	}

	// phase 2: players pick a side, countdown runs
	if(m_QuestionActive && m_WallsOpen)
	{
		if(m_CountdownTick > 0)
		{
			m_CountdownTick--;
			if(m_CountdownTick % Server()->TickSpeed() == 0)
			{
				const int SecondsLeft = m_CountdownTick / Server()->TickSpeed();
				GS()->BroadcastWorld(GS()->GetWorldID(), BroadcastPriority::VeryImportant, Server()->TickSpeed() + 5, "Time left: {}s", SecondsLeft);
			}
		}
		else
		{
			KillWrongZonePlayers();
			SpawnWalls();
			m_QuestionActive = false;
			m_WallsOpen = false;

			m_ResultsActive = true;
			m_ResultsTick = 5 * Server()->TickSpeed();
		}
		return;
	}

	// phase 3: show the correct answer, then send winners back to neutral
	if(m_ResultsActive)
	{
		if(m_ResultsTick > 0)
		{
			m_ResultsTick--;
			const char* pCorrectStr = m_CorrectAnswer ? "TRUE" : "FALSE";
			GS()->BroadcastWorld(GS()->GetWorldID(), BroadcastPriority::VeryImportant, 10, "GG! The correct answer was: {}", pCorrectStr);

			if(m_ResultsTick <= 0)
			{
				m_ResultsActive = false;
				TeleportPlayersToNeutral(true);
			}
		}
	}
}

void CGameControllerTrueOrFalse::SpawnWalls()
{
	m_pWall1 = new CLogicWallWall(&GS()->m_World, s_Wall1Pos, 0, 100);
	m_pWall2 = new CLogicWallWall(&GS()->m_World, s_Wall2Pos, 0, 100);
	m_pWall1->SetDestroy(0);
	m_pWall2->SetDestroy(0);
}

void CGameControllerTrueOrFalse::DestroyWalls()
{
	if(m_pWall1)
	{
		m_pWall1->MarkForDestroy();
		m_pWall1 = nullptr;
	}
	if(m_pWall2)
	{
		m_pWall2->MarkForDestroy();
		m_pWall2 = nullptr;
	}
}

void CGameControllerTrueOrFalse::TeleportPlayersToNeutral(bool OnlyWinners)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pChr = GS()->GetPlayerChar(i);
		if(!pChr || !GS()->IsPlayerInWorld(i))
			continue;

		if(!OnlyWinners)
		{
			pChr->ChangePosition(s_NeutralCenter);
			pChr->GiveWeapon(WEAPON_HAMMER, -1);
			continue;
		}

		const auto* pSwitchTile = GS()->Collision()->GetSwitchTile(pChr->GetPos());
		if(!pSwitchTile)
			continue;

		const int TileType = pSwitchTile->m_Type;
		const bool InCorrectZone = m_CorrectAnswer
			? (TileType == TILE_TOF_TRUE)
			: (TileType == TILE_TOF_FALSE);

		if(InCorrectZone)
			pChr->ChangePosition(s_NeutralCenter);
	}
}

void CGameControllerTrueOrFalse::KillWrongZonePlayers()
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pChr = GS()->GetPlayerChar(i);
		if(!pChr || !GS()->IsPlayerInWorld(i))
			continue;

		const auto* pSwitchTile = GS()->Collision()->GetSwitchTile(pChr->GetPos());
		if(!pSwitchTile)
			continue;

		const int TileType = pSwitchTile->m_Type;

		if(TileType == TILE_TOF_LOBBY)
			continue;

		const bool InCorrectZone = m_CorrectAnswer
			? (TileType == TILE_TOF_TRUE)
			: (TileType == TILE_TOF_FALSE);

		if(!InCorrectZone)
			pChr->Die(i, WEAPON_WORLD);
	}
}

void CGameControllerTrueOrFalse::ConTrueOrFalseStart(IConsole::IResult* pResult, void* pUser)
{
	auto* pSelf = (CGameControllerTrueOrFalse*)pUser;
	const int Active = pResult->GetInteger(0);

	if(Active == 1)
	{
		if(pSelf->m_GameActive)
		{
			pSelf->GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trueorfalse", "Game is already active.");
			return;
		}

		pSelf->m_GameActive = true;
		pSelf->m_PlayersMovedToNeutral = false;
		pSelf->m_QuestionActive = false;
		pSelf->m_WallsOpen = false;

		pSelf->SpawnWalls();
		pSelf->TeleportPlayersToNeutral(false);
		pSelf->m_PlayersMovedToNeutral = true;

		pSelf->GS()->BroadcastWorld(pSelf->GS()->GetWorldID(), BroadcastPriority::VeryImportant, 300, "True or False game started! Wait for a question.");
	}
	else
	{
		pSelf->m_GameActive = false;
		pSelf->m_QuestionActive = false;
		pSelf->m_ResultsActive = false;
		pSelf->m_WallsOpen = false;
		pSelf->m_PlayersMovedToNeutral = false;
		pSelf->m_CountdownTick = 0;
		pSelf->m_QuestionBroadcastTick = 0;
		pSelf->m_ResultsTick = 0;
		pSelf->DestroyWalls();

		pSelf->GS()->BroadcastWorld(pSelf->GS()->GetWorldID(), BroadcastPriority::VeryImportant, 300, "True or False game stopped.");
	}
}

void CGameControllerTrueOrFalse::ConTrueOrFalse(IConsole::IResult* pResult, void* pUser)
{
	auto* pSelf = (CGameControllerTrueOrFalse*)pUser;

	if(!pSelf->m_GameActive)
	{
		pSelf->GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trueorfalse", "Game is not active. Run sv_trueorfalse_start 1 first.");
		return;
	}

	if(pSelf->m_QuestionActive)
	{
		pSelf->GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trueorfalse", "A question is already active.");
		return;
	}

	const char* pRaw = pResult->GetString(0);

	char aRaw[512];
	str_copy(aRaw, pRaw, sizeof(aRaw));

	// last token is the answer
	char* pLastSpace = strrchr(aRaw, ' ');
	if(!pLastSpace)
	{
		pSelf->GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trueorfalse", "Usage: sv_trueorfalse question seconds true/false");
		return;
	}
	char aAnswer[16];
	str_copy(aAnswer, pLastSpace + 1, sizeof(aAnswer));
	*pLastSpace = '\0';

	// second to last token is the duration
	char* pSecondLastSpace = strrchr(aRaw, ' ');
	if(!pSecondLastSpace)
	{
		pSelf->GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "trueorfalse", "Usage: sv_trueorfalse question seconds true/false");
		return;
	}
	const int Seconds = str_toint(pSecondLastSpace + 1);
	*pSecondLastSpace = '\0';

	const char* pQuestion = aRaw;
	const char* pAnswer = aAnswer;

	pSelf->m_CorrectAnswer = (str_comp_nocase(pAnswer, "true") == 0);
	pSelf->m_CountdownTick = Seconds * pSelf->Server()->TickSpeed();
	pSelf->m_QuestionBroadcastTick = 8 * pSelf->Server()->TickSpeed();
	pSelf->m_QuestionActive = true;
	pSelf->m_WallsOpen = false;
	str_copy(pSelf->m_aQuestion, pQuestion, sizeof(pSelf->m_aQuestion));

	pSelf->DestroyWalls();
	pSelf->SpawnWalls();
}

void CGameControllerTrueOrFalse::OnEntity(int Index, vec2 Pos, int Flags)
{
	if(Index == ENTITY_SPAWN)
		m_aaSpawnPoints[SPAWN_HUMAN].push_back(Pos);
	else if(Index == ENTITY_SPAWN_MOBS)
		m_aaSpawnPoints[SPAWN_BOT].push_back(Pos);
	else if(Index == ENTITY_SPAWN_PRISON)
		m_aaSpawnPoints[SPAWN_HUMAN_PRISON].push_back(Pos);
}