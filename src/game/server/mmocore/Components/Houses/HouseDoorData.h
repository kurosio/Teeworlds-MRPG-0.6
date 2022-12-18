/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DOOR_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DOOR_DATA_H

class CHouseDoorData
{
	friend class CHouseData;
	class CGS* m_pGS {};

	vec2 m_Pos {};
	class CHouseData* m_pHouse{};
	class HouseDoor* m_pDoor {};
	std::vector<int> m_AccessUserIDs{};

public:
	CHouseDoorData(class CGS* pGS, vec2 Pos, std::string AccessData, class CHouseData* pHouse);
	~CHouseDoorData();

	const vec2& GetPos() const { return m_Pos; }
	bool GetState() const { return m_pDoor; }
	std::vector<int>& GetAccesses() { return m_AccessUserIDs; }

	void AddAccess(int UserID);
	void RemoveAccess(int UserID);
	bool HasAccess(int UserID);
	int GetAvailableAccessSlots() const;

	void Open();
	void Close();
	void Reverse();

private:
	void SaveAccessList() const;
};

#endif