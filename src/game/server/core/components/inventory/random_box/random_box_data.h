/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_RANDOM_BOX_RANDOM_BOX_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_RANDOM_BOX_RANDOM_BOX_DATA_H

// forward
class CPlayerItem;
class CPlayer;

// simple element
class CElementBox
{
public:
	CElementBox() = default;
	CElementBox(int itemID, int value) : ItemID(itemID), Value(value) {}
	auto operator<=>(const CElementBox& other) const = default;

	bool isEmpty() const
	{
		return ItemID < 0 || Value < 0;
	}

	int ItemID {-1};
	int Value {-1};
};

// deterministic box
class CBox
{
	std::vector<CElementBox> m_vItems {};

public:
	CBox() = default;

	void Add(int ItemID, int Value)
	{
		if(ItemID < 0 || Value <= 0)
			return;

		m_vItems.emplace_back(ItemID, Value);
	}
	bool Give(CPlayer* pPlayer, CPlayerItem* pPlayerUsesItem = nullptr, int UseValue = 1) const;
	bool IsEmpty() const { return m_vItems.empty(); }
	const std::vector<CElementBox>& GetItems() const { return m_vItems; }
};

// random box
class CRandomBox
{
	bool m_NormalizedChances {};
	ChanceProcessor<CElementBox> m_vItems {};

public:
	CRandomBox() = default;

	void Add(int ItemID, int Value, float Chance)
	{
		m_vItems.addElement(CElementBox(ItemID, Value), Chance);
		m_vItems.sortElementsByChance();
	}
	void NormalizeChances()
	{
		if(m_NormalizedChances || m_vItems.isEmpty())
			return;

		m_vItems.normalizeChances();
		m_NormalizedChances = true;
	}
	bool Start(CPlayer* pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem = nullptr, int UseValue = 1);
	bool IsEmpty() const { return m_vItems.isEmpty(); }
	const ChanceProcessor<CElementBox>& GetItems() const { return m_vItems; }
};

#endif