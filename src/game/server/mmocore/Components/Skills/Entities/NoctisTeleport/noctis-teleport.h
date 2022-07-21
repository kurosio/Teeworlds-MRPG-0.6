/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_ENTITIES_NOCTIS_TELEPORT_H
#define GAME_SERVER_COMPONENT_SKILL_ENTITIES_NOCTIS_TELEPORT_H
#include <game/server/entity.h>

class CNoctisTeleport : public CEntity
{
public:
	CNoctisTeleport(CGameWorld *pGameWorld, vec2 Pos, CCharacter* pPlayerChar, int SkillBonus);

	void Snap(int SnappingClient) override;
	void Reset() override;
	void Tick() override;

private:
	int m_LifeSpan;
	int m_SkillBonus;
	vec2 m_Direction;

public:
	CCharacter *m_pPlayerChar;

};

#endif
