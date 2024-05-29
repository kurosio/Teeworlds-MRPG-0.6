/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_FLYING_POINT_H
#define GAME_SERVER_ENTITIES_FLYING_POINT_H

#include <game/server/entity.h>

class CEntityFlyingPoint : public CEntity
{
	typedef std::function<void(class CPlayer*, class CPlayer*)> FlyingPointCallback;

	vec2 m_InitialVel{};
	float m_InitialAmount{};
	int m_ClientID{};
	int m_FromID{};
	int m_Type{};
	FlyingPointCallback m_pFunctionCollised{};

public:
	CEntityFlyingPoint(CGameWorld* pGameWorld, vec2 Pos, vec2 InitialVel, int ClientID, int FromID);
	void Register(FlyingPointCallback pFunc) { m_pFunctionCollised = std::move(pFunc); };
	void SetType(int Type) { m_Type = Type; }

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
