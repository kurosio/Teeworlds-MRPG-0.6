#ifndef ENGINE_SERVER_WORLD_DETAIL_H
#define ENGINE_SERVER_WORLD_DETAIL_H

class IKernel;
class IEngineMap;
class CWorld;

class CWorldDetail
{
	int64_t m_Flags {};
	int m_RespawnWorldID {};
	int m_JailWorldID {};
	int m_RequiredLevel {};
	WorldType m_Type {};

public:
	CWorldDetail() = default;
	CWorldDetail(const std::string_view& Type, const DBSet& FlagsSet, int RespawnWorldID, int JailWorldID, int RequiredLevel)
	{
		if(Type == "default")
			m_Type = WorldType::Default;
		else if(Type == "mini_games")
			m_Type = WorldType::MiniGames;
		else if(Type == "dungeon")
			m_Type = WorldType::Dungeon;
		else if(Type == "deep_dungeon")
			m_Type = WorldType::DeepDungeon;
		else if(Type == "treasure_dungeon")
			m_Type = WorldType::TreasureDungeon;
		else if(Type == "pvp")
			m_Type = WorldType::PvP;
		else if(Type == "tutorial")
			m_Type = WorldType::Tutorial;

		m_RespawnWorldID = RespawnWorldID;
		m_JailWorldID = JailWorldID;
		m_RequiredLevel = RequiredLevel;
		InitFlags(FlagsSet);
	}

	void InitFlags(const DBSet& FlagsSet)
	{
		if(FlagsSet.hasSet("rating_system"))
			m_Flags |= WORLD_FLAG_RATING_SYSTEM;
		if(FlagsSet.hasSet("crime_score"))
			m_Flags |= WORLD_FLAG_CRIME_SCORE;
		if(FlagsSet.hasSet("lost_gold_death"))
			m_Flags |= WORLD_FLAG_LOST_DEATH_GOLD;
		if(FlagsSet.hasSet("spawn_full_mana"))
			m_Flags |= WORLD_FLAG_SPAWN_FULL_MANA;
		if(FlagsSet.hasSet("allowed_pvp"))
			m_Flags |= WORLD_FLAG_ALLOWED_PVP;
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

	bool HasFlag(int64_t Flag) const
	{
		return (m_Flags & Flag) != 0;
	}

	WorldType GetType() const
	{
		return m_Type;
	}
};

#endif