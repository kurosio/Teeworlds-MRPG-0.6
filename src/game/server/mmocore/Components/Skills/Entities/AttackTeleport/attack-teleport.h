/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCORE_COMPONENTS_SKILLS_ENTITIES_ATTACKTELEPORT_ATTACK_TELEPORT_H
#define GAME_SERVER_MMOCORE_COMPONENTS_SKILLS_ENTITIES_ATTACKTELEPORT_ATTACK_TELEPORT_H
#include <game/server/entity.h>

class CAttackTeleport : public CEntity
{
public:
	CAttackTeleport(CGameWorld *pGameWorld, vec2 Pos, CCharacter* pPlayerChar, int SkillBonus);
	~CAttackTeleport() override;

	void Snap(int SnappingClient) override;
	void Tick() override;

private:
	int m_LifeSpan;
	int m_SkillBonus;
	vec2 m_Direction;

public:
	CCharacter *m_pPlayerChar;

};

#endif
