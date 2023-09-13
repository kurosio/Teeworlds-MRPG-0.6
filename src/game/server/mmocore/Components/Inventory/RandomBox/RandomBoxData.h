/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_H

class CPlayerItem;
class CPlayer;

struct StRandomItem
{
	int m_ItemID{};
	int m_Value{};
	float m_Chance{};
};

class CRandomBox
{
	std::vector <StRandomItem> m_VectorItems{};

public:
	CRandomBox() = default;
	CRandomBox(CRandomBox&) = delete;
	CRandomBox(const CRandomBox&) = delete;
	CRandomBox(const std::initializer_list<StRandomItem>& pList) { m_VectorItems.insert(m_VectorItems.end(), pList.begin(), pList.end()); }
	void Add(const StRandomItem& element) { m_VectorItems.push_back(element); }
	void Add(int ItemID, int Value, float Chance) { m_VectorItems.push_back({ItemID, Value, Chance}); }
	bool Start(CPlayer* pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem = nullptr, int UseValue = 1);
	bool Empty() const { return m_VectorItems.empty(); }
};

#endif