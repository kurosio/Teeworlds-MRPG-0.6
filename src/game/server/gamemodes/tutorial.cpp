/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tutorial.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Tutorial/TutorialManager.h"

CGameControllerTutorial::CGameControllerTutorial(class CGS* pGS)
	: IGameController(pGS)
{
	m_GameFlags = 0;
}

void CGameControllerTutorial::Tick()
{
	IGameController::Tick();

	// handle tutorial world
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(GS()->IsPlayerEqualWorld(i))
		{
			if(CPlayer* pPlayer = GS()->GetPlayer(i, true, true); pPlayer)
			{
				CTutorialManager* pTutorialManager = GS()->Mmo()->Tutorial();
				pTutorialManager->HandleTutorial(pPlayer);

				if(pPlayer->m_TutorialStep >= pTutorialManager->GetSize() && !pPlayer->GetItem(itAdventurersBadge)->HasItem())
					pPlayer->GetItem(itAdventurersBadge)->Add(1);
			}
		}
	}
}

bool CGameControllerTutorial::OnCharacterSpawn(CCharacter* pChr)
{
	if(!pChr->GetPlayer()->IsBot())
	{
		pChr->CreateSnapProj(pChr->GetSnapFullID(), 1, POWERUP_NINJA, true, false);
		pChr->CreateSnapProj(pChr->GetSnapFullID(), 1, POWERUP_ARMOR_LASER, true, false);
		pChr->CreateSnapProj(pChr->GetSnapFullID(), 1, POWERUP_ARMOR_NINJA, true, false);
		pChr->CreateSnapProj(pChr->GetSnapFullID(), 1, WEAPON_GRENADE, true, true);
		pChr->CreateSnapProj(pChr->GetSnapFullID(), 1, WEAPON_GUN, true, true);
		pChr->CreateSnapProj(pChr->GetSnapFullID(), 1, WEAPON_SHOTGUN, true, true);
	}
	return IGameController::OnCharacterSpawn(pChr);
}
