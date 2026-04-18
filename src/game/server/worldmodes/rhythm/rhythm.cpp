/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "rhythm.h"

#include <game/server/gamecontext.h>
#include "entities/rhythm_field.h"

CGameControllerRhythm::CGameControllerRhythm(CGS* pGameServer)
	: IGameController(pGameServer)
{
}

void CGameControllerRhythm::OnInit()
{
	IGameController::OnInit();
	GS()->ChatWorld(GS()->GetWorldID(), nullptr, "Rhythm world is active! Stay on beat.");
}
