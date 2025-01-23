#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_WALLPUSHER_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_WALLPUSHER_H
#include <game/server/entity.h>

class CEntityRifleWallPusher : public CEntity
{
	vec2 m_Direction {};
	int m_LifeTick {};

public:
	CEntityRifleWallPusher(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction, int LifeTick);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void CheckHitCharacter(const vec2 PrevPos) const;
};

#endif
