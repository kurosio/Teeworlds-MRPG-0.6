/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_WORLDMODES_TRUEORFALSE_H
#define GAME_SERVER_WORLDMODES_TRUEORFALSE_H

#include <game/server/gamecontroller.h>

class CLogicWallWall;

class CGameControllerTrueOrFalse : public IGameController
{
public:
	CGameControllerTrueOrFalse(class CGS* pGS);

	void OnInit() override;
	void Tick() override;
	void OnEntity(int Index, vec2 Pos, int Flags) override;

	bool IsGameActive() const { return m_GameActive; }

private:
	bool m_GameActive {};
	bool m_PlayersMovedToNeutral {};
	bool m_WallsOpen {};
	bool m_QuestionActive {};
	bool m_ResultsActive {};
	bool m_CorrectAnswer {};
	char m_aQuestion[256] {};
	int m_CountdownTick {};
	int m_QuestionBroadcastTick {};
	int m_ResultsTick {};

	CLogicWallWall* m_pWall1 {};
	CLogicWallWall* m_pWall2 {};

	void KillWrongZonePlayers();
	void SpawnWalls();
	void DestroyWalls();
	void TeleportPlayersToNeutral(bool OnlyWinners = false);

	static void ConTrueOrFalse(IConsole::IResult* pResult, void* pUser);
	static void ConTrueOrFalseStart(IConsole::IResult* pResult, void* pUser);
};

#endif