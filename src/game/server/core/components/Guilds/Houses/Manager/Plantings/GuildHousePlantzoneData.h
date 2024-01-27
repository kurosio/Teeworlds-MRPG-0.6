/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_PLANTZONE_DATA_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_PLANTZONE_DATA_H

class CJobItems;

class CGuildHousePlantzoneData
{
	std::string m_Name {};
	int m_ItemID{};
	vec2 m_Pos{};
	float m_Radius{};
	std::vector<CJobItems*> m_vPlants{};

public:
	CGuildHousePlantzoneData() = delete;
	CGuildHousePlantzoneData(std::string&& Name, int ItemID, vec2 Pos, float Radius)
	{
		m_Name = Name;
		m_ItemID = ItemID;
		m_Pos = Pos;
		m_Radius = Radius;
	}

	const char* GetName() const { return m_Name.c_str(); }
	float GetRadius() const { return m_Radius; }
	int GetItemID() const { return m_ItemID; }
	vec2 GetPos() const { return m_Pos; }
	std::vector<CJobItems*>& GetContainer() { return m_vPlants; }

	void Add(CJobItems* pItem) { m_vPlants.push_back(pItem); }
	void Remove(CJobItems* pItem) { m_vPlants.erase(std::remove(m_vPlants.begin(), m_vPlants.end(), pItem), m_vPlants.end()); }
};

#endif