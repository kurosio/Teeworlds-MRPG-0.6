/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_TUTORIAL_H
#define GAME_SERVER_GAMEMODES_TUTORIAL_H

#include <game/server/gamecontroller.h>

class CGameControllerTutorial : public IGameController
{
public:
	CGameControllerTutorial(class CGS* pGameServer);

	void Tick() override;
	void CreateLogic(int Type, int Mode, vec2 Pos, int ParseID) override {}

	bool OnCharacterSpawn(CCharacter* pChr) override;
};
#endif
