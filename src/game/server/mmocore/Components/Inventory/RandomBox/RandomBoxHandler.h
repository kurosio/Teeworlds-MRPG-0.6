/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_HANDLER_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_HANDLER_H
#include <game/server/entity.h>

class CPlayerItem;
class CPlayer;

class CEntityRandomBoxRandomizer : public CEntity
{
	int m_Used;
	int m_LifeTime;
	int m_AccountID;
	CPlayer* m_pPlayer;
	CPlayerItem* m_pPlayerUsesItem;
	std::vector<CRandomItem> m_aRandomItems;

public:
	// CEntityRandomBoxRandomizer is a class that represents a random box randomizer entity in the game world
	// It takes a pointer to the game world and a pointer to the player that activated the random box
	// It also takes the player's account ID, the lifetime of the random box, a list of random items that can be obtained from the random box,
	// a pointer to the player's current item and the value used for item usage
	CEntityRandomBoxRandomizer(CGameWorld* pGameWorld, CPlayer* pPlayer, int PlayerAccountID, int LifeTime, const std::vector<CRandomItem>& List, CPlayerItem* pPlayerUsesItem, int UseValue);

	// Selects a random item from the list of available random items
	std::vector<CRandomItem>::iterator SelectRandomItem();

	// Updates the state of the random box randomizer entity tick
	void Tick() override;

	// Sends network snapshots of the random box randomizer entity to clients
	void Snap(int SnappingClient) override;
};

#endif