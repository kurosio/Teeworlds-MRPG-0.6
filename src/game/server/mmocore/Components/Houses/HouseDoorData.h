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

	class CHouseDoorInfo
	{
		friend class CHouseDoorData;
		std::string m_Name {};
		class HouseDoor* m_pDoor{};
		vec2 m_Pos{};

	public:
		CHouseDoorInfo() = default;
		CHouseDoorInfo(std::string&& Name, vec2 Pos) : m_Name(std::move(Name)), m_Pos(Pos) {}

		const char* GetName() const { return m_Name.c_str(); }
		bool GetState() const { return m_pDoor != nullptr; }
		vec2 GetPos() const { return m_Pos; }
	};
	ska::unordered_map<int, CHouseDoorInfo> m_apDoors {};
	ska::unordered_set<int> m_AccessUserIDs {};

public:
	CHouseDoorData(class CGS* pGS, std::string&& AccessData, std::string&& JsonDoorData, class CHouseData* pHouse);
	~CHouseDoorData();

	ska::unordered_set<int>& GetAccesses() { return m_AccessUserIDs; }
	ska::unordered_map<int, CHouseDoorInfo>& GetDoors() { return m_apDoors; }

	void AddAccess(int UserID);
	void RemoveAccess(int UserID);
	bool HasAccess(int UserID);
	int GetAvailableAccessSlots() const;

	void Open(int Number);
	void Close(int Number);
	void Reverse(int Number);

	void OpenAll();
	void CloseAll();
	void ReverseAll();

private:
	void SaveAccessList() const;
};

#endif