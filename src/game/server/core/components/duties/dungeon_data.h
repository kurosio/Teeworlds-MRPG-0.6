#ifndef GAME_SERVER_CORE_COMPONENTS_DUTIES_DUNGEON_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_DUTIES_DUNGEON_DATA_H

class CGS;
class CPlayer;

class CDungeonData : public MultiworldIdentifiableData< std::deque< CDungeonData* > >
{
	CGS* GS() const;

	int m_ID {};
	int m_Level {};
	vec2 m_WaitingDoorPos {};

	int m_State {};
	int m_Progress {};
	int m_Players {};
	int m_WorldID {};
	nlohmann::json m_Scenario{};

public:
	enum DungeonState
	{
		STATE_UNINITIALIZED = -1,
		STATE_INACTIVE,
		STATE_WAITING,
		STATE_STARTED,
		STATE_FINISHED,
	};

	static CDungeonData* CreateElement(int ID)
	{
		auto pData = new CDungeonData();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(const vec2& WaitingDoorPos, int Level, int WorldID, const nlohmann::json& Scenario)
	{
		m_Level = Level;
		m_WorldID = WorldID;
		m_WaitingDoorPos = WaitingDoorPos;
		m_State = STATE_UNINITIALIZED;
		m_Scenario = Scenario;
	}

	int GetID() const { return m_ID; }
	int GetLevel() const { return m_Level; }
	const char* GetName() const;
	vec2 GetWaitingDoorPos() const { return m_WaitingDoorPos; }
	int GetWorldID() const { return m_WorldID; }

	void SetState(int State) { m_State = State; }
	int GetState() const { return m_State; }
	void UpdateProgress(int Progress) { m_Progress = Progress; }
	void UpdatePlayers(int Players) { m_Players = Players; }

	const nlohmann::json& GetScenario() const { return m_Scenario; }
	bool IsPlaying() const { return m_State >= STATE_STARTED; }
	int GetProgress() const { return m_Progress; }
	int GetPlayersNum() const { return m_Players; }
};

#endif