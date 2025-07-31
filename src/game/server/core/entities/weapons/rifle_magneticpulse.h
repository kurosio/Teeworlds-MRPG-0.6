#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_MAGNETICPULSE_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_MAGNETICPULSE_H
#include <game/server/entity.h>

class CEntityRifleMagneticPulse : public CEntity
{
	enum
	{
		GROUP_PROJECTILE_CIRCLE = 0,
		GROUP_CIRCLE,

		NUM_IDS_PROJECTILE_CIRCLE = 2,
		NUM_IDS_CIRCLE = 8,
	};

	vec2 m_Direction {};
	bool m_LastPhase {};
	int m_LifeTick {};
	Interpolation<float> m_RadiusAnimation;

public:
	CEntityRifleMagneticPulse(CGameWorld* pGameWorld, int OwnerCID, float Radius, vec2 Pos, vec2 Direction);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void RunLastPhase();
	void TickFirstPhase();
	void TickLastPhase();
};

#endif
