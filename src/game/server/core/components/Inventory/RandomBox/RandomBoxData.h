/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_INVENTORY_RANDOM_BOX_H
#define GAME_SERVER_INVENTORY_RANDOM_BOX_H

class CPlayerItem;
class CPlayer;

// Define a class called CRandomItem
class CRandomItem
{
public:
	// Default constructor
	CRandomItem() = default;

	// Constructor with parameters to initialize item ID, value, and chance
	CRandomItem(int ItemID, int Value, float Chance) :
		m_ItemID(ItemID), m_Value(Value), m_Chance(Chance)
	{}

	// Public member variables
	int m_ItemID {}; // The ID of the item
	int m_Value {}; // The value of the item
	float m_Chance {}; // The chance of getting the item
};

class CRandomBox
{
	std::vector <CRandomItem> m_VectorItems {}; // Create an empty vector to store CRandomItem objects

public:
	// Default constructor for CRandomBox
	CRandomBox() = default;

	// Constructor for CRandomBox that takes a list of CRandomItem objects as input
	CRandomBox(const std::initializer_list<CRandomItem>& pList)
	{
		m_VectorItems.insert(m_VectorItems.end(), pList.begin(), pList.end());
	}

	// Add a new CRandomItem object to the vector using perfect forwarding
	template < typename ... Args >
	void Add(Args&& ... args)
	{
		m_VectorItems.emplace_back(std::forward<Args>(args)...); 
	}

	// Check if the vector is empty
	bool IsEmpty() const
	{
		return m_VectorItems.empty();
	}

	// Start the random box process with given parameters
	bool Start(CPlayer* pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem = nullptr, int UseValue = 1);
};

#endif