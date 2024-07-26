/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_ENTITIES_HEARTS_H
#define GAME_SERVER_COMPONENT_SKILL_ENTITIES_HEARTS_H
#include <game/server/entity.h>

class CPlayer;

class CHeartHealer : public CEntity
{
	CPlayer *m_pPlayer;
	vec2 m_InitialVel;
	float m_InitialAmount;
	int m_Health;
	bool m_ShowInformation;

public:
	CHeartHealer(CGameWorld *pGameWorld, vec2 Pos, CPlayer *pPlayer, int Health, vec2 InitialVel, bool ShowInformation = false);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
