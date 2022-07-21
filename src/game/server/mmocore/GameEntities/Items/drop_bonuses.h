/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_DROPINGBONUSES_H
#define GAME_SERVER_ENTITIES_DROPINGBONUSES_H
#include <game/server/entity.h>

class CDropBonuses : public CEntity
{
	vec2 m_Vel;
	float m_Angle;
	float m_AngleForce;

	int m_Type;
	bool m_Flashing;
	int m_LifeSpan;
	int m_FlashTimer;
	int m_Value;

public:
	CDropBonuses(CGameWorld* pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, int Type, int Value);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
