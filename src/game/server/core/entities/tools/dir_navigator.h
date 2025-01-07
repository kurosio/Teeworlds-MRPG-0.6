/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_ENTITIES_TOOLS_DIR_NAVIGATOR_H
#define GAME_SERVER_CORE_ENTITIES_TOOLS_DIR_NAVIGATOR_H

#include <game/server/entity.h>

class CEntityDirectionNavigator : public CEntity
{
public:
	class CPlayer* m_pPlayer;

	CEntityDirectionNavigator(CGameWorld* pGameWorld);

	bool TryStart(int ClientID, vec2 Position, int WorldID);
	void Snap(int SnappingClient) override;
};

#endif
