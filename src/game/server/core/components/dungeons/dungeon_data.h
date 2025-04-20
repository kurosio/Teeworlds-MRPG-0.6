#ifndef GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_DATA_H

class CDungeonData : public MultiworldIdentifiableData< std::deque< CDungeonData* > >
{
	int m_ID {};
	int m_Level {};
	std::string m_Name {};
	vec2 m_WaitingDoorPos {};

	bool m_Playing {};
	int m_Progress {};
	int m_Players {};
	int m_WorldID {};

public:
	static CDungeonData* CreateElement(int ID)
	{
		auto pData = new CDungeonData();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(const vec2& WaitingDoorPos, int Level, std::string_view Name, int WorldID)
	{
		m_Level = Level;
		m_Name = Name;
		m_WorldID = WorldID;
		m_WaitingDoorPos = WaitingDoorPos;
	}

	int GetID() const { return m_ID; }
	int GetLevel() const { return m_Level; }
	std::string_view GetName() const { return m_Name; }
	int GetWorldID() const { return m_WorldID; }

	bool IsPlaying() const { return m_Playing; }
	int GetProgress() const { return m_Progress; }
	int GetPlayersNum() const { return m_Players; }
};

#endif