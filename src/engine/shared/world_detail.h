#ifndef ENGINE_SERVER_WORLD_DETAIL_H
#define ENGINE_SERVER_WORLD_DETAIL_H

class IKernel;
class IEngineMap;
class CWorld;

class CWorldDetail
{
	int m_RespawnWorldID {};
	int m_JailWorldID {};
	int m_RequiredLevel {};
	WorldType m_Type {};

public:
	CWorldDetail() = default;
	CWorldDetail(const std::string_view& Type, int RespawnWorldID, int JailWorldID, int RequiredLevel)
	{
		if(Type == "default")
		{
			m_Type = WorldType::Default;
		}
		else if(Type == "dungeon")
		{
			m_Type = WorldType::Dungeon;
		}
		else if(Type == "tutorial")
		{
			m_Type = WorldType::Tutorial;
		}

		m_RespawnWorldID = RespawnWorldID;
		m_JailWorldID = JailWorldID;
		m_RequiredLevel = RequiredLevel;
	}

	int GetRespawnWorldID() const
	{
		return m_RespawnWorldID;
	}

	int GetJailWorldID() const
	{
		return m_JailWorldID;
	}

	int GetRequiredLevel() const
	{
		return m_RequiredLevel;
	}

	WorldType GetType() const
	{
		return m_Type;
	}
};

#endif