/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_H
#include <game/server/entity.h>

#include "ItemData.h"

class CPlayer;

struct StructRandomItem
{
	ItemIdentifier m_ItemID;
	int m_Value;
	float m_Chance;
};

class CRandomBox
{
	std::vector <StructRandomItem> m_ArrayItems;

public:
	void Add(ItemIdentifier ItemID, int Value, float Chance)
	{
		StructRandomItem Item;
		Item.m_Chance = Chance;
		Item.m_Value = Value;
		Item.m_ItemID = ItemID;
		m_ArrayItems.push_back(Item);
	}

	bool Start(CPlayer* pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem = nullptr, int UseValue = 1);
};

class CRandomBoxRandomizer : public CEntity
{
	int m_UseValue;
	int m_LifeTime;
	int m_PlayerAccountID;
	CPlayer* m_pPlayer;
	CPlayerItem* m_pPlayerUsesItem;
	std::vector<StructRandomItem> m_List;

public:
	CRandomBoxRandomizer(CGameWorld* pGameWorld, CPlayer* pPlayer, int PlayerAccountID, int LifeTime, std::vector<StructRandomItem> List, CPlayerItem* pPlayerUsesItem, int UseValue);

	std::vector<StructRandomItem>::iterator SelectRandomItem();
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif