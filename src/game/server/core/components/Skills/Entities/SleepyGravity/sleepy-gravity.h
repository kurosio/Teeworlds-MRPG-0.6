/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_ENTITIES_SLEEPY_GRAVITY_H
#define GAME_SERVER_COMPONENT_SKILL_ENTITIES_SLEEPY_GRAVITY_H
#include <game/server/entity.h>

class CPlayer;

class CSleepyGravity : public CEntity
{
public:
	enum
	{
		NUM_IDS = 12,
	};

	CSleepyGravity(CGameWorld *pGameWorld, CPlayer* pPlayer, int SkillBonus, int PowerLevel, vec2 Pos);
	~CSleepyGravity() override;

	void Snap(int SnappingClient) override;
	void Tick() override;

private:
	int m_IDs[NUM_IDS];
	int m_LifeSpan;
	int m_Radius;
	int m_PowerLevel;

public:
	CPlayer *m_pPlayer;
};

#endif
