/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_EIDOLON_CORE_H
#define GAME_SERVER_COMPONENT_EIDOLON_CORE_H
#include <game/server/core/mmo_component.h>

#include "EidolonInfoData.h"

class CEidolonManager : public MmoComponent
{
	int m_EidolonItemSelected[MAX_PLAYERS] {};

	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

public:
	// return eidolon size by pair (first player collection / second maximum eidolons)
	std::pair<int, int> GetEidolonsSize(int ClientID) const;
};

#endif