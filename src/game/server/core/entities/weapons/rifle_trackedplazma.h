#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_TRACKEDPLAZMA_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_TRACKEDPLAZMA_H
#include <game/server/entity.h>

class CCharacter;
class CEntityRifleTrackedPlazma : public CEntity
{
	vec2 m_Direction {};
	int m_TrackedCID {};
	bool m_Tracking {};
	Interpolation<float> m_Speed{};

public:
	CEntityRifleTrackedPlazma(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void Explode();
	void SearchPotentialTarget();
};

#endif
