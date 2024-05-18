/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_DATA_H

#define TW_ACHIEVEMENTS "tw_achievements"
#define TW_ACCOUNTS_ACHIEVEMENTS "tw_accounts_achievements"

// achievement types
enum AchievementType
{
	ACHIEVEMENT_TYPE_DEFEAT_PVP = 0,
	ACHIEVEMENT_TYPE_DEFEAT_PVE,
	ACHIEVEMENT_TYPE_DEFEAT_MOB,
	ACHIEVEMENT_TYPE_EQUIPPED,
	ACHIEVEMENT_TYPE_GET_ITEM,
	ACHIEVEMENT_TYPE_USE_ITEM,
	ACHIEVEMENT_TYPE_CRAFT_ITEM,
	ACHIEVEMENT_TYPE_ENCHANT_ITEM,
	ACHIEVEMENT_TYPE_UNLOCK_WORLD,
	ACHIEVEMENT_TYPE_LEVELING,
};

// achievement information data
class CAchievementInfo : public MultiworldIdentifiableStaticData< std::deque<CAchievementInfo*> >
{
	int m_ID {};
	int m_Type {};
	std::string m_aName {};
	std::string m_aDescription {};

	// union for the achievement data
	union AchievementDataUnion
	{
		struct { int m_Value; } m_DefeatPVP; // defeat player vs player
		struct { int m_Value; } m_DefeatPVE; // defeat player vs environment
		struct { int m_ID; int m_Value; } m_DefeatMob; // defeat mob
		struct { int m_ItemID; } m_Equipped; // equip item
		struct { int m_ItemID; int m_Value; } m_GetItem; // get item
		struct { int m_ItemID; int m_Value; } m_UseItem; // use item
		struct { int m_ItemID; int m_Value; } m_CraftItem; // craft item
		struct { int m_ItemID; int m_Value; } m_EnchantItem; // enchant item
		struct { int m_WorldID; } m_UnlockWorld; // unlock world
		struct { int m_Value; } m_Leveling; // leveling
	} m_Data{};

public:
	explicit CAchievementInfo(int ID) : m_ID(ID) {}

	static CAchievementInfo* CreateElement(const int& ID)
	{
		auto pData = new CAchievementInfo(ID);
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	int GetID() const { return m_ID; }
	const char* GetName() const { return m_aName.c_str(); }
	const char* GetDescription() const { return m_aDescription.c_str(); }
	int GetType() const { return m_Type; }

	// initalize the Aether data
	void Init(const std::string& pName, const std::string& pDescription, int Type, const std::string& Data)
	{
		m_aName = pName;
		m_aDescription = pDescription;
		m_Type = Type;

		InitCriteriaJson(Data);
	}
	void InitCriteriaJson(const std::string& JsonData);

	// check if the achievement is completed
	bool CheckAchievement(int Type, int Value, int Value2);
};

// achievement data
class CAchievement : public MultiworldIdentifiableStaticData< std::deque<CAchievement*> >
{
	int m_ID {};
	int m_AccountID {};
	int m_Progress {};
	CAchievementInfo* m_pInfo {};

public:
	explicit CAchievement(int ID) : m_ID(ID) {}

	static CAchievement* CreateElement(const int& ID)
	{
		auto pData = new CAchievement(ID);
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	// initalize the Aether data
	void Init(const int& pAchievementID, const int& pAccountID, const int& pProgress)
	{
		const auto iter = std::find_if(CAchievementInfo::Data().begin(), CAchievementInfo::Data().end(), 
			[pAchievementID](CAchievementInfo* p) { return p->GetID() == pAchievementID; });
		dbg_assert(iter != CAchievementInfo::Data().end(), "achievement not found");
		
		m_pInfo = *iter;
		m_AccountID = pAccountID;
		m_Progress = pProgress;
	}
};


#endif