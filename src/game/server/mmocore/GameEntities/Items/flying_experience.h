/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_FLYING_EXPERIENCE_H
#define GAME_SERVER_ENTITIES_FLYING_EXPERIENCE_H
#include <game/server/entity.h>

class CFlyingExperience : public CEntity
{
private:
	vec2 m_InitialVel;
	float m_InitialAmount;
	int m_ClientID;
	int m_Experience;

public:
	CFlyingExperience(CGameWorld* pGameWorld, vec2 Pos, int ClientID, int Experience, vec2 InitialVel);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
