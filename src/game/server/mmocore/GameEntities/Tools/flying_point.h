/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_FLYING_POINT_H
#define GAME_SERVER_ENTITIES_FLYING_POINT_H

#include <game/server/entity.h>

class CFlyingPoint : public CEntity
{
	typedef std::function<void(class CPlayer*)> FlyingPointCallback;

	vec2 m_InitialVel;
	float m_InitialAmount;
	int m_ClientID;
	FlyingPointCallback m_pFunctionCollised;

public:
	CFlyingPoint(CGameWorld* pGameWorld, vec2 Pos, int ClientID, vec2 InitialVel);
	void Register(FlyingPointCallback pFunc) { m_pFunctionCollised = std::move(pFunc); };

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
