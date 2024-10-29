/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PICKUP_H
#define GAME_SERVER_ENTITIES_PICKUP_H
#include <game/server/entity.h>

class CPickup : public CEntity
{
	int m_Type;
	int m_SubType;
	int m_SpawnTick;
	bool m_Projectile;

public:
	CPickup(CGameWorld *pGameWorld, int Type, int SubType, vec2 Pos);
	CPickup(CGameWorld *pGameWorld, int ProjType, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void Init(int Type, int Subtype, bool Projectile);
};

#endif
