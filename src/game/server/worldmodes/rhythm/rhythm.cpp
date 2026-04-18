/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "rhythm.h"

#include <game/server/gamecontext.h>

CGameControllerRhythm::CGameControllerRhythm(CGS* pGameServer)
	: CGameControllerDefault(pGameServer)
{
}

void CGameControllerRhythm::OnInit()
{
	CGameControllerDefault::OnInit();
	GS()->ChatWorld(GS()->GetWorldID(), nullptr, "Rhythm world is active! Stay on beat.");
}
