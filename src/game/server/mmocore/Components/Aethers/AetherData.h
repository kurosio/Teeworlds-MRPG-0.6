/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AETHER_DATA_H
#define GAME_SERVER_COMPONENT_AETHER_DATA_H

using AetherIdentifier = int;

class CAether : public MultiworldIdentifiableStaticData< std::map< int, CAether > >
{
	AetherIdentifier m_ID{};
	char m_aName[64]{};
	vec2 m_Pos{};
	int m_WorldID{};

public:
	CAether() = default;
	CAether(AetherIdentifier ID) : m_ID(ID) {}

	void Init(const char* pName, vec2 Pos, int WorldID)
	{
		str_copy(m_aName, pName, sizeof(m_aName));
		m_Pos = Pos;
		m_WorldID = WorldID;
	}

	const char* GetName() const { return m_aName; }
	vec2 GetPosition() const { return m_Pos; }
	int GetWorldID() const { return m_WorldID; }
};

#endif