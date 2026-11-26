/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_DATA_H

#include <game/server/core/mmo_context.h>

class CGS;
class CPlayer;

// How an incoming event changes the stored progress.
enum class EAchievementProgress
{
	Accumulate,
	Absolute,
};

// Static description loaded from tw_achievements.
class CAchievementInfo : public MultiworldIdentifiableData< std::deque<CAchievementInfo*> >
{
	int m_ID {};
	AchievementType m_Type {};
	std::string m_Name {};
	int m_Criteria {};
	int m_Required {};
	int m_Point {};
	nlohmann::json m_RewardData {};
	std::string m_RewardText {};
	bool m_IsChainTail {};

public:
	explicit CAchievementInfo(int ID) : m_ID(ID) {}

	static CAchievementInfo* CreateElement(int ID)
	{
		return m_pData.emplace_back(new CAchievementInfo(ID));
	}

	void Init(const std::string& Name, AchievementType Type, int Criteria, int Required,
		const std::string& RewardData, int Point);
	// Called after all item descriptions are loaded, so item names are known.
	void BuildRewardText();

	int GetID() const { return m_ID; }
	int GetCriteria() const { return m_Criteria; }
	int GetRequired() const { return m_Required; }
	int GetPoint() const { return m_Point; }
	const char* GetName() const { return m_Name.c_str(); }
	AchievementType GetType() const { return m_Type; }
	const nlohmann::json& GetRewardData() const { return m_RewardData; }
	const std::string& GetRewardText() const { return m_RewardText; }

	// All achievement types share one rule: reach the required progress.
	// Criteria <= 0 means "any criteria" and is handled by MatchesCriteria().
	bool MatchesCriteria(int Criteria) const { return m_Criteria <= 0 || m_Criteria == Criteria; }
	bool IsProgressEnough(int Progress) const { return Progress >= m_Required; }

	// Only the last level of a chain persists the shared progress row.
	bool IsChainTail() const { return m_IsChainTail; }
	void SetChainTail() { m_IsChainTail = true; }
};

// Per-client progress. The map is keyed by achievement ID so that events can
// reach a level directly instead of scanning every achievement of the player.
class CAchievement : public MultiworldIdentifiableData< std::map< int, std::map< int, CAchievement* > > >
{
	const CAchievementInfo* m_pInfo {};
	int m_ClientID {};
	int m_Progress {};
	bool m_Completed {};
	bool m_NotifiedSoonComplete {};
	int m_LastSaveTick {};

public:
	explicit CAchievement(const CAchievementInfo* pInfo, int ClientID) : m_pInfo(pInfo), m_ClientID(ClientID) {}

	static CAchievement* CreateElement(const CAchievementInfo* pInfo, int ClientID)
	{
		auto* pData = new CAchievement(pInfo, ClientID);
		return m_pData[ClientID][pInfo->GetID()] = pData;
	}

	void Init(int Progress, bool Completed)
	{
		m_Progress = Progress;
		m_Completed = Completed;
		m_LastSaveTick = 0;
	}

	const CAchievementInfo* Info() const { return m_pInfo; }
	bool IsCompleted() const { return m_Completed; }
	int GetProgress() const { return m_Progress; }
	bool UpdateProgress(int Criteria, int Progress, EAchievementProgress Mode);

private:
	CGS* GS() const;
	CPlayer* GetPlayer() const;
	void NotifyPlayerProgress();
	void RewardPlayer(CPlayer* pPlayer) const;
};

#endif
