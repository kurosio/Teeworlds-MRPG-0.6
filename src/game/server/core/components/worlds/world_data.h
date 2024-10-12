/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDDATA_H
#define GAME_SERVER_COMPONENT_WORLDDATA_H

class CPlayer;
using WorldDataPtr = std::shared_ptr< class CWorldData >;

// World swap data
class CWorldSwapData
{
	std::pair<vec2, vec2> m_Positions {};
	std::pair<int, int> m_Worlds {};

public:
	CWorldSwapData() = default;
	CWorldSwapData(const std::pair<vec2, vec2> Positions, const std::pair<int, int> Worlds)
		: m_Positions(Positions), m_Worlds(Worlds) {}

	// Get the first swap position
	vec2 GetFirstSwapPosition() const
	{
		return m_Positions.first;
	}

	// Get the second swap position
	vec2 GetSecondSwapPosition() const
	{
		return m_Positions.second;
	}

	// Get the first world ID
	int GetFirstWorldID() const
	{
		return m_Worlds.first;
	}

	// Get the second world ID
	int GetSecondWorldID() const
	{
		return m_Worlds.second;
	}
};

// World data
class CWorldData : public MultiworldIdentifiableData< std::deque< WorldDataPtr > >
{
	int m_ID {};
	char m_aName[64] {};
	int m_RequiredLevel {};
	int m_RespawnWorldID {};
	int m_JailWorldID {};
	std::deque < CWorldSwapData > m_Swappers {};

public:
	// Default constructor for CWorldData class
	CWorldData() = default;

	// Create a new instance of CWorldData and return its pointer as a shared_ptr
	static WorldDataPtr CreateElement(int ID)
	{
		auto pData = std::make_shared<CWorldData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	// initialize the world data
	void Init(int RespawnWorldID, int JailWorldID, int RequiredLevel, std::deque <CWorldSwapData>&& Worlds);

	// move player to another world
	void Move(CPlayer* pPlayer);

	// get identifier of the world
	int GetID() const
	{
		return m_ID;
	}

	// get name of the world
	const char* GetName() const
	{
		return m_aName;
	}

	// get required level of the world
	int GetRequiredLevel() const
	{
		return m_RequiredLevel;
	}

	// get respawn world data
	CWorldData* GetRespawnWorld() const;

	// get jail world data
	CWorldData* GetJailWorld() const;

	// get swapper by position
	CWorldSwapData* GetSwapperByPos(vec2 Pos);

	// get collection of world swappers
	std::deque <CWorldSwapData>& GetSwappers()
	{
		return m_Swappers;
	}
};

#endif