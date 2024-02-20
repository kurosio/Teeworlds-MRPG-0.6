/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_QUESTAI_H
#define GAME_SERVER_ENTITIES_QUESTAI_H

#include <game/server/entity.h>

class CEntityArrowNavigator : public CEntity
{
public:
	class CPlayer* m_pPlayer;

	CEntityArrowNavigator(CGameWorld* pGameWorld, int ClientID, vec2 Position, int WorldID);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
