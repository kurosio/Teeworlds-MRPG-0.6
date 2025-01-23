/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H
#include <game/server/entity.h>

class CLaser : public CEntity
{
	vec2 m_Direction {};
	float m_Energy {};
	int m_Bounces {};
	int m_Damage {};
	int m_EvalTick {};
	bool m_Explosive {};

public:
	CLaser(CGameWorld *pGameWorld, int OwnerCID, int Damage, vec2 Pos, vec2 Direction, float StartEnergy, bool Explosive);

	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

protected:
	virtual bool HitCharacter(vec2 From, vec2 To);
	virtual void DoBounce();
};

#endif
