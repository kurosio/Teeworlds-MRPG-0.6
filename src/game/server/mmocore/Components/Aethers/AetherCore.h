/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AETHER_CORE_H
#define GAME_SERVER_COMPONENT_AETHER_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "AetherData.h"

class CAetherCore : public MmoComponent
{
	~CAetherCore() override
	{
		CAetherData::ms_aTeleport.clear();
	};

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	void UnlockLocation(CPlayer* pPlayer, vec2 Pos);
	void ShowTeleportList(CCharacter* pChar) const;
};

#endif