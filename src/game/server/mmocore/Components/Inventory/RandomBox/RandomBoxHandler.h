/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_HANDLER_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_HANDLER_H
#include <game/server/entity.h>

class CPlayerItem;
class CPlayer;

class CEntityRandomBoxRandomizer : public CEntity
{
	int m_UseValue;
	int m_LifeTime;
	int m_PlayerAccountID;
	CPlayer* m_pPlayer;
	CPlayerItem* m_pPlayerUsesItem;
	std::vector<StRandomItem> m_List;

public:
	CEntityRandomBoxRandomizer(CGameWorld* pGameWorld, CPlayer* pPlayer, int PlayerAccountID, int LifeTime, std::vector<StRandomItem> List, CPlayerItem* pPlayerUsesItem, int UseValue);

	std::vector<StRandomItem>::iterator SelectRandomItem();
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif