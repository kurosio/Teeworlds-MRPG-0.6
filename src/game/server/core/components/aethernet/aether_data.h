/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_AETHER_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_AETHER_DATA_H

#define TW_AETHERS "tw_aethers"
#define TW_ACCOUNTS_AETHERS "tw_accounts_aethers"

using AetherIdentifier = int;

class CAetherData : public MultiworldIdentifiableData< std::deque<CAetherData*> >
{
	AetherIdentifier m_ID {};
	char m_aName[64] {};
	vec2 m_Pos {};
	int m_WorldID {};

public:
	// Constructor with AetherIdentifier parameter
	explicit CAetherData(AetherIdentifier ID) : m_ID(ID) {}

	// Create element
	static CAetherData* CreateElement(const AetherIdentifier& ID)
	{
		auto pData = new CAetherData(ID);
		return m_pData.emplace_back(pData);
	}

	// Initalize the Aether data
	void Init(const char* pName, vec2 Pos, int WorldID)
	{
		str_copy(m_aName, pName, sizeof(m_aName));
		m_Pos = Pos;
		m_WorldID = WorldID;
	}

	AetherIdentifier GetID() const { return m_ID; } // Get the Aether ID
	const char* GetName() const { return m_aName; } // Get the name
	vec2 GetPosition() const { return m_Pos; } // Get the position
	int GetWorldID() const { return m_WorldID; } // Get the world ID


};

#endif