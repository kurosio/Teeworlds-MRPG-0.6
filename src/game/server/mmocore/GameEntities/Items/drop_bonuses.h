/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_DROPINGBONUSES_H
#define GAME_SERVER_ENTITIES_DROPINGBONUSES_H
#include <game/server/entity.h>

class CDropBonuses : public CEntity
{
	vec2 m_Vel;
	int m_Type;
	int m_Value;
	int m_LifeSpan;
	CFlashingTick m_Flash;

public:
	CDropBonuses(CGameWorld* pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, int Type, int Value);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
