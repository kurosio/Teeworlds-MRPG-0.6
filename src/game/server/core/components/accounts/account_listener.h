#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNTS_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNTS_LISTENER_H

#include <game/server/core/tools/event_listener.h>

class CGS;
class CPlayer;

struct TrackingLevelingData
{
	int AccountID {};
	int Level {};
};

inline void to_json(nlohmann::json& j, const TrackingLevelingData& data)
{
	j = nlohmann::json::object({
		{"account_id", data.AccountID},
		{"level", data.Level}
	});
}

inline void from_json(const nlohmann::json& j, TrackingLevelingData& data)
{
	j.at("account_id").get_to(data.AccountID);
	j.at("level").get_to(data.Level);
}

// leveling tracker
class CLevelingTracker
{
	friend class CAccountListener;
	std::unordered_map<int, TrackingLevelingData> m_vTrackingData {};

	void UpdateTrackingDataIfNecessary(CPlayer* pPlayer, int ProfessionID, int NewLevel);
	void LoadTrackingData();
	void SaveTrackingData();

public:
	std::optional<TrackingLevelingData> GetTrackingData(int ProfessionID) const;
	const std::unordered_map<int, TrackingLevelingData>& GetTrackings() const { return m_vTrackingData; }
};

// inventory listener
class CAccountListener : public IEventListener
{
	CLevelingTracker m_LevelingTracker;

public:
	void Initialize();
	CLevelingTracker& LevelingTracker() { return m_LevelingTracker; }

protected:
	void OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount) override;
	void OnCharacterSpawn(CPlayer* pPlayer) override;
	void OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel) override;
};

extern CAccountListener g_AccountListener;

#endif