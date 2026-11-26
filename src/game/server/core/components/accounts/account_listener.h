#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LISTENER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_LISTENER_H

#include <engine/server/db/db_result.h>
#include <game/server/core/tools/event_listener.h>

#include <array>

class CGS;
class CPlayer;

// The best account of one profession.
struct TrackingLevelingData
{
	int AccountID {};
	int Level {};
};

// The specialists, one entry per profession.
//
// The profession rows are the only source: the level lives inside the
// profession document, so the database reduces the rows to one per profession
// and the tracker merges that answer. An empty answer is not an answer - the
// list it would replace stays and the read is retried.
class CLevelingTracker
{
	friend class CAccountListener;

	// The tag of every read, so the menu that shows this list can follow it.
	static constexpr const char* TRACKING_TAG = "profession_leaders";
	static constexpr int NUM_ENTRIES = (int)ProfessionIdentifier::NUM_PROFESSIONS;

	bool m_Loaded = false;
	bool m_Refreshing = false;
	// Ticks until the next refresh: a countdown, because the server tick wraps.
	int m_RefreshDelay = 0;
	// Consecutive answers without a leader, for the backoff of the retries.
	int m_Retries = 0;
	std::array<TrackingLevelingData, NUM_ENTRIES> m_aLeaders {};

	void UpdateIfNecessary(CPlayer* pPlayer, ProfessionIdentifier Profession, int Level);
	void Apply(Db::CResultSet&& Rows);

public:
	static constexpr const char* UpdateTag() { return TRACKING_TAG; }

	// Reads the league again in the background; harmless while a read runs.
	void Refresh();
	// Drives the periodic refresh. Called once per tick by the initializer
	// world only: the list is global, not per world.
	void Tick();

	// Null when the profession has no leader yet.
	const TrackingLevelingData* GetLeader(ProfessionIdentifier Profession) const;
	// True after an answer that named at least one leader.
	bool IsLoaded() const { return m_Loaded; }
};

// account and leveling listener
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
