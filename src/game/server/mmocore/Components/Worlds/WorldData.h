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
	std::pair<vec2, vec2> m_Positions {};
	std::pair<int, int> m_Worlds {};

public:
	CWorldSwapData() = default;
	CWorldSwapData(int ID, const std::pair <vec2, vec2 > Positions, const std::pair < int, int > Worlds)
		: m_ID(ID), m_Positions(Positions), m_Worlds(Worlds)
	{}

	vec2 GetFirstSwapPosition() const { return m_Positions.first; } // Get the first swap position
	vec2 GetSecondSwapPosition() const { return m_Positions.second; } // Get the second swap position

	int GetFirstWorldID() const { return m_Worlds.first; } // Get the first world ID
	int GetSecondWorldID() const { return m_Worlds.second; } // Get the second world ID
};

/*
 * World Data
 */
class CWorldData : public MultiworldIdentifiableStaticData< std::deque< WorldDataPtr > >
{
	WorldIdentifier m_ID {};
	char m_aName[64] {};
	int m_RequiredQuestID {};
	int m_RespawnWorldID {};
	std::deque < CWorldSwapData > m_Swappers {};

public:
	CWorldData() = default;

	static WorldDataPtr CreateElement(WorldIdentifier ID)
	{
		WorldDataPtr pData = std::make_shared<CWorldData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(int RespawnWorldID, int RequiredQuestID, const std::deque <CWorldSwapData>& Worlds);
	void Move(class CPlayer* pPlayer);

	WorldIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_aName; }
	class CQuestDescription* GetRequiredQuest() const;
	CWorldData* GetRespawnWorld();
	CWorldSwapData* GetSwapperByPos(vec2 Pos);
	std::deque <CWorldSwapData>& GetSwappers() { return m_Swappers; }
};

#endif