/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_DATA_H

#define TW_ACHIEVEMENTS "tw_achievements"

// forward declaration
class CGS;
class CPlayer;
class CAchievement;

enum AchievementProgressType
{
	PROGRESS_ACCUMULATE = 0,
	PROGRESS_ABSOLUTE,
};

enum
{
	ACHIEVEMENT_GROUP_GENERAL = 0,
	ACHIEVEMENT_GROUP_BATTLE,
	ACHIEVEMENT_GROUP_ITEMS
};

// achievement information data
class CAchievementInfo : public MultiworldIdentifiableData< std::deque<CAchievementInfo*> >
{
	int m_ID {};
	AchievementType m_Type {};
	std::string m_aName {};
	int m_Criteria{};
	int m_Required{};
	int m_Point {};
	nlohmann::json m_RewardData {};

public:
	explicit CAchievementInfo(int ID) : m_ID(ID) {}
	static CAchievementInfo* CreateElement(const int& ID)
	{
		auto pData = new CAchievementInfo(ID);
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	int GetID() const { return m_ID; }
	int GetCriteria() const { return m_Criteria; }
	int GetRequired() const { return m_Required; }
	int GetPoint() const { return m_Point; }
	const char* GetName() const { return m_aName.c_str(); }
	AchievementType GetType() const { return m_Type; }
	nlohmann::json& GetRewardData() { return m_RewardData; }
	bool RewardExists() const { return !m_RewardData.empty(); }

	// initalize the Aether data
	void Init(const std::string& pName, AchievementType Type, int Criteria, int Required, const std::string& RewardData, int Point)
	{
		m_aName = pName;
		m_Type = Type;
		m_Criteria = Criteria;
		m_Required = Required;
		m_Point = Point;

		InitData(RewardData);
	}
	void InitData(const std::string& RewardData);

	// check if the achievement is completed
	bool IsCompleted(int Criteria, const CAchievement* pAchievement) const;
};

// achievement data
class CAchievement : public MultiworldIdentifiableData< std::map< int, std::deque< CAchievement* > > >
{
	friend class CAchievementInfo;
	friend class CAchievementManager;

	CGS* GS() const;
	CPlayer* GetPlayer() const;

	CAchievementInfo* m_pInfo {};
	int m_ClientID {};
	int m_Progress {};
	bool m_Completed {};
	bool m_NotifiedSoonComplete {};

public:
	explicit CAchievement(CAchievementInfo* pInfo, int ClientID) : m_pInfo(pInfo), m_ClientID(ClientID) {}
	static CAchievement* CreateElement(CAchievementInfo* pInfo, int ClientID)
	{
		const auto pData = new CAchievement(pInfo, ClientID);
		return m_pData[ClientID].emplace_back(pData);
	}

	// initalize the Aether data
	void Init(int Progress, bool Completed)
	{
		m_Progress = Progress;
		m_Completed = Completed;
	}

	CAchievementInfo* Info() const { return m_pInfo; }
	bool IsCompleted() const { return m_Completed; }
	int GetProgress() const { return m_Progress; }
	bool UpdateProgress(int Criteria, int Progress, int ProgressType);

private:
	void NotifyPlayerProgress();
	void RewardPlayer() const;
};

#endif