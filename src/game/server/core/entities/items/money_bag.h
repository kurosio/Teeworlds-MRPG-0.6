#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_MONEY_BAG
#define GAME_SERVER_CORE_ENTITIES_ITEMS_MONEY_BAG
#include <game/server/entity.h>

class CEntityMoneyBag : public CEntity
{
public:
	CEntityMoneyBag(CGameWorld* pGameWorld, vec2 Pos);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
