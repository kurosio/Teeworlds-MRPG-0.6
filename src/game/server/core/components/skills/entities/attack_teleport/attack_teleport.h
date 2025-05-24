/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_ENTITIES_ATTACK_TELEPORT_ATTACK_TELEPORT_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_ENTITIES_ATTACK_TELEPORT_ATTACK_TELEPORT_H
#include <game/server/entity.h>

class CPlayer;
class CEntityLaserOrbite;

class CAttackTeleport : public CEntity
{
public:
	CAttackTeleport(CGameWorld *pGameWorld, vec2 Pos, CPlayer* pPlayer, int SkillBonus);
	~CAttackTeleport() override;

	void Snap(int SnappingClient) override;
	void Tick() override;

private:
	int m_LifeSpan{};
	int m_SkillBonus{};
	vec2 m_Direction{};

	bool m_SecondPart {};
	int m_SecondPartTimeleft{};
	int m_SecondPartCombo{};
	std::vector<CPlayer*> m_vMovingMap{};
	CEntityLaserOrbite* m_pEntOrbite {};

public:
	CPlayer* m_pPlayer{};
};

#endif