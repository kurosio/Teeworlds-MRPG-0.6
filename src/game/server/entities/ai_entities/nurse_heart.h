/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_AI_ENTITIES_NURSE_HEART_H
#define GAME_SERVER_ENTITIES_AI_ENTITIES_NURSE_HEART_H
#include <game/server/entity.h>

class CEntityNurseHeart : public CEntity
{
public:
	CEntityNurseHeart(CGameWorld* pGameWorld, int ClientID);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
