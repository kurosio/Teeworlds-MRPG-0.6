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
	std::pair<vec2, vec2> m_Positions {};
	std::pair<int, int> m_Worlds {};

public:
	// Default constructor
	CWorldSwapData() = default;

	// Parameterized constructor
	CWorldSwapData(const std::pair<vec2, vec2> Positions, const std::pair<int, int> Worlds)
		: m_Positions(Positions), m_Worlds(Worlds) {}

	// Getter methods for world swap element
	vec2 GetFirstSwapPosition() const { return m_Positions.first; }     // Get the first swap position
	vec2 GetSecondSwapPosition() const { return m_Positions.second; }   // Get the second swap position

	int GetFirstWorldID() const { return m_Worlds.first; }              // Get the first world ID
	int GetSecondWorldID() const { return m_Worlds.second; }            // Get the second world ID
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
	int m_JailWorldID {};
	std::deque < CWorldSwapData > m_Swappers {};

public:
	// Default constructor for CWorldData class
	CWorldData() = default;

	// Create a new instance of CWorldData and return its pointer as a shared_ptr
	static WorldDataPtr CreateElement(WorldIdentifier ID)
	{
		WorldDataPtr pData = std::make_shared<CWorldData>();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	// Initialize the CWorldData instance with the specified parameters
	void Init(int RespawnWorldID, int JailWorldID, int RequiredQuestID, std::deque <CWorldSwapData>&& Worlds);

	// Move the player to a different world
	void Move(class CPlayer* pPlayer);

	// Getter methods for world data
	WorldIdentifier GetID() const { return m_ID; }                     // Return the identifier of the world
	const char* GetName() const { return m_aName; }                    // Return the name of the world
	class CQuestDescription* GetRequiredQuest() const;                 // Return the required quest for the world
	CWorldData* GetRespawnWorld() const;                               // Return the respawn world data
	CWorldData* GetJailWorld() const;                                  // Return the respawn world data
	CWorldSwapData* GetSwapperByPos(vec2 Pos);                         // Return the world swapper data based on a position
	std::deque <CWorldSwapData>& GetSwappers() { return m_Swappers; }  // Return the collection of world swappers
};

#endif