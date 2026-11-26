#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LISTENER_H

#include <engine/server/db/db_result.h>
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
	if(!j.is_object())
		return;

	data.AccountID = j.value("account_id", 0);
	data.Level = j.value("level", 0);
}

// leveling tracker
//
// The file next to the database is a cache of the leaders; the profession rows
// are the authority. Reading them is a full table walk, so it never happens on
// the game thread and never replaces the list it could not read: an empty or
// failed answer keeps the previous state and is retried, instead of leaving the
// specialists list empty until the server restarts.
class CLevelingTracker
{
	friend class CAccountListener;
	// The tag every read of this tracker carries, so a menu that shows the
	// specialists can wait for the answer and tell "loading" from "none".
	static constexpr const char* TRACKING_TAG = "profession_leaders";
	bool m_Loaded = false;
	bool m_RefreshInFlight = false;
	bool m_Synchronized = false;
	int m_RefreshFailures = 0;
	// Remaining ticks until the next refresh. A countdown, not an absolute tick,
	// because the server tick is an int and wraps.
	int m_RefreshDelayTicks = 0;
	std::unordered_map<int, TrackingLevelingData> m_vTrackingData {};

	void UpdateTrackingDataIfNecessary(CPlayer* pPlayer, int ProfessionID, int NewLevel);
	void LoadTrackingData();
	void SaveTrackingData();
	void ApplyDatabaseRows(Db::CResultSet&& Rows);
	void ScheduleNextRefresh(bool Succeeded);

public:
	// Reads the leaders again in the background; harmless while one read runs.
	void RefreshFromDatabase();
	static constexpr const char* UpdateTag() { return TRACKING_TAG; }
	// Drives the periodic refresh and its retries. Called once per tick by the
	// initializer world only: the tracker is global, not per world.
	void Tick();

	bool IsRefreshing() const { return m_RefreshInFlight; }
	// True after the first successful read: an empty list is then a real answer.
	bool IsSynchronized() const { return m_Synchronized; }

	std::optional<TrackingLevelingData> GetTrackingData(int ProfessionID) const;
	const std::unordered_map<int, TrackingLevelingData>& GetTrackings() const { return m_vTrackingData; }
};

// inventory listener
class CAccountListener : public IEventListener
{
	CLevelingTracker m_LevelingTracker;

public:
	void Initialize();
	void OnTick();
	CLevelingTracker& LevelingTracker() { return m_LevelingTracker; }

protected:
	void OnCharacterSpawn(CPlayer* pPlayer) override;
	void OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount) override;
	void OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel) override;
};

extern CAccountListener g_AccountListener;

#endif