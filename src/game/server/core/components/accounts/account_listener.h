#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LISTENER_H

#include <engine/server/db/db_result.h>
#include <game/server/core/tools/event_listener.h>

class CPlayer;

struct TrackingLevelingData
{
	int AccountID {};
	int Level {};
};

struct TrackingAttributeData
{
	int AccountID {};
	int Amount {};
};

inline void to_json(nlohmann::json& j, const TrackingAttributeData& Data)
{
	j = { {"account_id", Data.AccountID}, {"amount", Data.Amount} };
}

inline void from_json(const nlohmann::json& j, TrackingAttributeData& Data)
{
	if(j.is_object())
	{
		Data.AccountID = j.value("account_id", 0);
		Data.Amount = j.value("amount", 0);
	}
}

// One place for profession leaders and attribute records.
class CTrackingData
{
	static constexpr const char* PROFESSION_TAG = "profession_leaders";
	static constexpr int NUM_PROFESSIONS = (int)ProfessionIdentifier::NUM_PROFESSIONS;

	std::array<TrackingLevelingData, NUM_PROFESSIONS> m_aProfessionLeaders {};
	std::unordered_map<int, TrackingAttributeData> m_AttributeLeaders {};
	bool m_ProfessionsLoaded = false;
	bool m_RefreshingProfessions = false;
	int m_RefreshDelay = 0;

	void SetProfessionLeader(int ProfessionID, int AccountID, int Level);
	void RefreshProfessions();
	void ApplyProfessions(Db::CResultSet&& Rows);
	void LoadAttributes();

public:
	static constexpr const char* ProfessionTag() { return PROFESSION_TAG; }

	void Initialize();
	void Tick();
	void UpdateProfession(CPlayer* pPlayer, ProfessionIdentifier Profession, int Level);
	bool UpdateAttribute(CPlayer* pPlayer, AttributeIdentifier Attribute, int Value);
	void SaveAttributes() const;

	const TrackingLevelingData* GetProfessionLeader(ProfessionIdentifier Profession) const;
	const std::unordered_map<int, TrackingAttributeData>& GetAttributes() const { return m_AttributeLeaders; }
	bool IsProfessionLoaded() const { return m_ProfessionsLoaded; }
};

class CAccountListener : public IEventListener
{
	CTrackingData m_TrackingData;

public:
	void Initialize();
	void OnTick();
	CTrackingData& TrackingData() { return m_TrackingData; }

protected:
	void OnCharacterSpawn(CPlayer* pPlayer) override;
	void OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount) override;
	void OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel) override;
};

extern CAccountListener g_AccountListener;

#endif
