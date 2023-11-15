/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DOOR_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DOOR_DATA_H
#include <unordered_set>


class CHouseDoorData
{
	friend class CHouseData;
	class CGS* m_pGS {};
	class CHouseData* m_pHouse {};
	class HouseDoor* m_pDoor {};

	vec2 m_Pos {};
	std::unordered_set<int> m_AccessUserIDs {};

public:
	CHouseDoorData(class CGS* pGS, vec2 Pos, std::string AccessData, class CHouseData* pHouse);
	~CHouseDoorData();

	const vec2& GetPos() const { return m_Pos; }
	bool GetState() const { return m_pDoor; }
	std::unordered_set<int>& GetAccesses() { return m_AccessUserIDs; }

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