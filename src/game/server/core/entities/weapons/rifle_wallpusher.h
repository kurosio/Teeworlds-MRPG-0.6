#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_WALLPUSHER_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_RIFLE_WALLPUSHER_H
#include <game/server/entity.h>

class CEntityRifleWallPusher : public CEntity
{
	vec2 m_Direction {};
	int m_LifeTick {};
	int m_ID2 {};

public:
	CEntityRifleWallPusher(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction, int LifeTick);
	~CEntityRifleWallPusher();

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void TryHitCharacter(const vec2 PrevPos);
};

#endif
