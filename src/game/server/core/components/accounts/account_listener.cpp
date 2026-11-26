#include "account_listener.h"

#include "account_db.h"
#include <engine/server/async_file.h>
#include <game/server/gamecontext.h>

CAccountListener g_AccountListener;
constexpr const char* LEVELING_TRACKING_FILE_NAME = "server_data/leveling_tracking.json";

// A refresh that failed (or answered nothing) is retried with a growing pause,
// so a database that is not up yet costs a few attempts, not a broken list. A
// read that succeeded is repeated slowly: a leader can also gain levels while
// nobody is online, from another server or directly in the database.
constexpr int LEVELING_REFRESH_RETRY_MIN_SECONDS = 5;
constexpr int LEVELING_REFRESH_RETRY_MAX_SECONDS = 60;
constexpr int LEVELING_REFRESH_INTERVAL_SECONDS = 120;

// account listener
void CAccountListener::Initialize()
{
	g_EventListenerManager.RegisterListener(IEventListener::CharacterSpawn, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerLogin, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionLeveling, this);
	m_LevelingTracker.LoadTrackingData();
}

void CAccountListener::OnTick()
{
	// The tracker is one global list; refreshing it from every world would run
	// the same read once per world, so the caller runs this in the initializer
	// world only.
	m_LevelingTracker.Tick();
}

void CAccountListener::OnCharacterSpawn(CPlayer* pPlayer)
{
	if(!g_Config.m_SvHighlightExperts || pPlayer->IsBot())
		return;

	struct LevelingData
	{
		ProfessionIdentifier m_Identifier;
		bool m_Projectile;
		int m_Type;
		int m_Subtype;
		int m_OrbitType;
	};

	static constexpr std::array<LevelingData, (int)ProfessionIdentifier::NUM_PROFESSIONS> s_aLevelingData { {
		{ ProfessionIdentifier::Tank,      false, POWERUP_ARMOR,  0,          MULTIPLE_ORBIT_TYPE_ELLIPTICAL },
		{ ProfessionIdentifier::Dps,       false, POWERUP_WEAPON, WEAPON_GUN, MULTIPLE_ORBIT_TYPE_ELLIPTICAL },
		{ ProfessionIdentifier::Healer,    false, POWERUP_HEALTH, 0,          MULTIPLE_ORBIT_TYPE_ELLIPTICAL },
		{ ProfessionIdentifier::Miner,     true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_EIGHT },
		{ ProfessionIdentifier::Farmer,    true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_DYNAMIC_CENTER },
		{ ProfessionIdentifier::Fisherman, true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_PULSATING },
		{ ProfessionIdentifier::Loader,    true,  WEAPON_HAMMER,  0,          MULTIPLE_ORBIT_TYPE_ELLIPTICAL }
	} };


	bool IsTopExpert = false;
	const auto AccountID = pPlayer->Account()->GetID();
	// Each matching profession gets two aura layers.
	for(int Layer = 0; Layer < 2; ++Layer)
	{
		for(const auto& Profession : s_aLevelingData)
		{
			const auto Leader = m_LevelingTracker.GetTrackingData((int)Profession.m_Identifier);
			if(Leader && AccountID == Leader->AccountID)
			{
				pPlayer->GetCharacter()->AddMultipleOrbit(Profession.m_Projectile, 1, Profession.m_Type, Profession.m_Subtype, Profession.m_OrbitType);
				IsTopExpert = true;
			}
		}
	}

	if(IsTopExpert)
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You are the top expert in your profession on the server. Visual highlighting is now active.");
}


void CAccountListener::OnPlayerLogin(CPlayer* pPlayer, CAccountData* pAccount)
{
	for(auto& Prof : pAccount->GetProfessions())
		m_LevelingTracker.UpdateTrackingDataIfNecessary(pPlayer, (int)Prof.GetProfessionID(), Prof.GetLevel());
}


void CAccountListener::OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel)
{
	m_LevelingTracker.UpdateTrackingDataIfNecessary(pPlayer, (int)pProfession->GetProfessionID(), pProfession->GetLevel());
}


// leveling tracker
void CLevelingTracker::LoadTrackingData()
{
	if(m_Loaded)
		return;

	m_Loaded = true;

	//  try load datafile
	ByteArray RawData;
	if(!mystd::file::load(LEVELING_TRACKING_FILE_NAME, &RawData))
	{
		m_vTrackingData.clear();
	}
	else
	{
		std::string RawString((const char*)RawData.data(), RawData.size());
		const bool HasError = mystd::json::parse(RawString, [this](nlohmann::json& jsonData)
		{
			if(!jsonData.is_object())
				return;

			for(const auto& item : jsonData["tracking"])
			{
				const int ProfessionID = item.value("profession_id", -1);
				if(ProfessionID < 0)
					continue;

				TrackingLevelingData Data = item.value("detail", TrackingLevelingData {});
				if(Data.AccountID <= 0 || Data.Level <= 0)
					continue;

				m_vTrackingData[ProfessionID] = Data;
			}
		});

		if(HasError)
		{
			dbg_msg("leveling_tracking", "Error with initialized '%s'. Creating new...", LEVELING_TRACKING_FILE_NAME);
			m_vTrackingData.clear();
		}
	}

	// The list above is a cache of the last run: it makes the specialists
	// available immediately (and a cold restart with a slow database keeps the
	// previous leaders instead of an empty menu). The rows are the authority and
	// are read in the background - the old blocking read here ran on the game
	// thread while the worlds were still starting.
	m_RefreshDelayTicks = 0;
	RefreshFromDatabase();
}


void CLevelingTracker::Tick()
{
	if(!m_Loaded || m_RefreshInFlight)
		return;

	if(m_RefreshDelayTicks > 0)
	{
		--m_RefreshDelayTicks;
		return;
	}

	RefreshFromDatabase();
}


void CLevelingTracker::RefreshFromDatabase()
{
	if(m_RefreshInFlight)
		return;

	m_RefreshInFlight = true;
	AccountDb::ListProfessionLeaders(TRACKING_TAG).Then([this](Db::CResultSet&& Rows)
	{
		ApplyDatabaseRows(std::move(Rows));
	});
}


void CLevelingTracker::ApplyDatabaseRows(Db::CResultSet&& Rows)
{
	m_RefreshInFlight = false;

	if(!Rows.Ok())
	{
		// Never drop what is already known because a read failed: an empty
		// specialists list is the symptom this tracker has to avoid.
		dbg_msg("leveling_tracking", "could not read the profession leaders (%s); keeping %zu tracked professions",
			Rows.Error().ToString().c_str(), m_vTrackingData.size());
		ScheduleNextRefresh(false);
		return;
	}

	// The rows arrive ordered by level, so the first row of a profession is its
	// leader. An answer without a single row is a database without professions,
	// which this code reads as "not ready" rather than "nobody is an expert" -
	// the same retry then picks the leaders up once the accounts exist.
	std::unordered_map<int, TrackingLevelingData> vFresh {};
	while(Rows.Next())
	{
		const int ProfessionID = Rows.GetInt("ProfessionID");
		const int AccountID = Rows.GetInt("UserID");
		if(ProfessionID < 0 || AccountID <= 0 || vFresh.contains(ProfessionID))
			continue;

		// The level is inside the document. A document without a usable level
		// is skipped instead of counted as level 0.
		int Level = 0;
		const bool ParseError = mystd::json::parse(Rows.GetString("Data"), [&Level](nlohmann::json& Json)
		{
			Level = Json.value("level", 0);
		});
		if(ParseError || Level <= 0)
			continue;

		vFresh[ProfessionID] = TrackingLevelingData { AccountID, Level };
	}

	// The answer describes the database at the moment the statement ran: a level
	// gained while it was in flight is newer and must not be rolled back. It is
	// also merged rather than assigned, so a stale file entry or a failed read
	// can never shorten the list.
	for(const auto& [ProfessionID, Data] : vFresh)
	{
		auto& Entry = m_vTrackingData[ProfessionID];
		if(Data.Level > Entry.Level)
			Entry = Data;
	}

	// A database that has not created a single profession yet is not an answer
	// to keep: the file is left alone and the next attempt reads again.
	if(!vFresh.empty())
	{
		m_Synchronized = true;
		SaveTrackingData();
	}

	ScheduleNextRefresh(!vFresh.empty());
}


void CLevelingTracker::ScheduleNextRefresh(bool Succeeded)
{
	int DelaySeconds = LEVELING_REFRESH_INTERVAL_SECONDS;
	if(!Succeeded)
	{
		m_RefreshFailures = std::min(m_RefreshFailures + 1, 16);
		DelaySeconds = LEVELING_REFRESH_RETRY_MIN_SECONDS;
		for(int i = 1; i < m_RefreshFailures; ++i)
			DelaySeconds = std::min(DelaySeconds * 2, LEVELING_REFRESH_RETRY_MAX_SECONDS);
	}
	else
	{
		m_RefreshFailures = 0;
	}

	m_RefreshDelayTicks = DelaySeconds * std::max(1, Instance::Server()->TickSpeed());
}


void CLevelingTracker::SaveTrackingData()
{
	nlohmann::json j;
	for(const auto& [professionID, data] : m_vTrackingData)
	{
		j["tracking"].push_back(
		{
			{"profession_id", professionID},
			{"detail", data}
		});
	}

	AsyncFile::Write(LEVELING_TRACKING_FILE_NAME, j.dump(4));
}


void CLevelingTracker::UpdateTrackingDataIfNecessary(CPlayer* pPlayer, int ProfessionID, int NewLevel)
{
	auto& TrackingData = m_vTrackingData[ProfessionID];
	if(NewLevel > TrackingData.Level)
	{
		TrackingData.AccountID = pPlayer->Account()->GetID();
		TrackingData.Level = NewLevel;
		SaveTrackingData();
	}
}


std::optional<TrackingLevelingData> CLevelingTracker::GetTrackingData(int ProfessionID) const
{
	const auto It = m_vTrackingData.find(ProfessionID);
	return It != m_vTrackingData.end() ? std::make_optional(It->second) : std::nullopt;
}
