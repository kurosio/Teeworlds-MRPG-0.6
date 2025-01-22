/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_H

// forward
class CPlayerItem;
class CPlayer;

// simple element
class CRandomItem
{
public:
	CRandomItem() = default;
	CRandomItem(int itemID, int value) : ItemID(itemID), Value(value) {}

	bool isEmpty() const { return ItemID < 0 || Value < 0; }

	int ItemID {-1};
	int Value {-1};
};

// random box
class CRandomBox
{
	ChanceProcessor<CRandomItem> m_vItems {};

public:
	CRandomBox() = default;

	void Add(int ItemID, int Value, float Chance)
	{
		m_vItems.addElement(CRandomItem(ItemID, Value), Chance);
	}
	bool Start(CPlayer* pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem = nullptr, int UseValue = 1);
	bool IsEmpty() const { return m_vItems.isEmpty(); }
};

#endif