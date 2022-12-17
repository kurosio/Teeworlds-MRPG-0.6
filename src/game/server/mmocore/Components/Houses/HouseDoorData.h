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
	CHouseDoorData(class CGS* pGS, vec2 Pos, std::string AccessData, class CHouseData* pHouse) : m_pGS(pGS), m_Pos(Pos), m_pHouse(pHouse)
	{
		// init access list
		if(!AccessData.empty())
		{
			size_t start;
			size_t end = 0;
			const std::string delim = ",";

			while((start = AccessData.find_first_not_of(delim, end)) != std::string::npos)
			{
				end = AccessData.find(delim, start);

				int UserID;
				if(sscanf(AccessData.substr(start, end - start).c_str(), "%d", &UserID) == 1)
					m_AccessUserIDs.push_back(UserID);
			}
		}
	}
	~CHouseDoorData();

	const vec2& GetPos() const { return m_Pos; }
	bool GetState() const { return m_pDoor; }
	std::vector<int>& GetAccessVector() { return m_AccessUserIDs; }

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