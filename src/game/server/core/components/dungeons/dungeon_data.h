#ifndef GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_DATA_H

class CDungeonData : public MultiworldIdentifiableData< std::deque< CDungeonData* > >
{
	int m_ID {};
	vec2 m_DoorPos {};
	int m_Level {};
	std::string m_Name {};
	int m_State {};

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

	void Init(const vec2& DoorPos, int Level, std::string_view Name, int WorldID)
	{
		m_DoorPos = DoorPos;
		m_Level = Level;
		m_Name = Name;
		m_WorldID = WorldID;
	}

	int GetID() const { return m_ID; }
	bool IsPlaying() const { return m_State > 1; }
	int GetLevel() const { return m_Level; }
	int GetProgress() const { return m_Progress; }
	std::string_view GetName() const { return m_Name; }
	int GetPlayersNum() const { return m_Players; }
	int GetWorldID() const { return m_WorldID; }
};

#endif