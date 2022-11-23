/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDDATA_H
#define GAME_SERVER_COMPONENT_WORLDDATA_H

using WorldIdentifier = int;
using WorldDataPtr = std::shared_ptr< class CWorldData >;

/*
 * World Swapper
 */
class CWorldSwapData
{
	int m_ID {};
	vec2 m_Position[2] {};
	int m_WorldID[2] {};

public:
	CWorldSwapData() = default;
	CWorldSwapData(int ID, std::pair <vec2, vec2 > Positions, std::pair < int, int > Worlds)
	{
		m_ID = ID;
		m_Position[0] = Positions.first;
		m_Position[1] = Positions.second;
		m_WorldID[0] = Worlds.first;
		m_WorldID[1] = Worlds.second;
	}

	vec2 GetFirstSwapPosition() const { return m_Position[0]; }
	vec2 GetSecondSwapPosition() const { return m_Position[1]; }

	int GetFirstWorldID() const { return m_WorldID[0]; }
	int GetSecondWorldID() const { return m_WorldID[1]; }
};

/*
 * World Data
 */
class CWorldData : public MultiworldIdentifiableStaticData< std::deque< WorldDataPtr > >
{
	WorldIdentifier m_ID{};
	char m_aName[64]{};
	int m_RequiredQuestID{};
	int m_RespawnWorldID{};
	std::deque < CWorldSwapData > m_Swappers{};

public:
	CWorldData() = default;

	static WorldDataPtr CreateDataItem(WorldIdentifier ID)
	{
		WorldDataPtr pData = std::make_shared<CWorldData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int RespawnWorldID, int RequiredQuestID, const std::deque <CWorldSwapData>& Worlds);
	void Move(class CPlayer* pPlayer);

	WorldIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_aName; }
	class CQuestDataInfo* GetRequiredQuest() const;
	CWorldData* GetRespawnWorld() ;
	CWorldSwapData* GetSwapperByPos(vec2 Pos);
	std::deque <CWorldSwapData>& GetSwappers() { return m_Swappers; }
};


/* for pathfined
 * Variant World Swap Positions
 */
struct CWorldSwapPosition
{
	int m_BaseWorldID;
	int m_FindWorldID;
	vec2 m_Position;

	static std::list< CWorldSwapPosition > ms_aWorldPositionLogic;
};

#endif