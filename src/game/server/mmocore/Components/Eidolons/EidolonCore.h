/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_EIDOLON_CORE_H
#define GAME_SERVER_COMPONENT_EIDOLON_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "game/server/mmocore/Components/Eidolons/EidolonInfoData.h"

class CEidolonCore : public MmoComponent
{
	int m_EidolonItemSelected[MAX_PLAYERS] {};

	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

public:
	int GetPlayerCollectedEidolonsSize(CPlayer* pPlayer) const;
};

#endif