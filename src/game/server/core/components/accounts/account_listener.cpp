#include "account_listener.h"

#include "account_db.h"
#include <game/server/gamecontext.h>

CAccountListener g_AccountListener;

// A leader can also gain levels while nobody is online (another server, or the
// database directly), so the list is re-read periodically. A failed or empty
// answer is retried sooner: a database that is not up yet costs a few attempts
// instead of an empty list until the next restart.
constexpr int LEVELING_REFRESH_SECONDS = 300;
constexpr int LEVELING_RETRY_MIN_SECONDS = 5;
constexpr int LEVELING_RETRY_MAX_SECONDS = 60;

// account listener
void CAccountListener::Initialize()
{
	g_EventListenerManager.RegisterListener(IEventListener::CharacterSpawn, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerLogin, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionLeveling, this);

	// The first read answers with the whole list, so there is nothing to load
	// from disk first: a cache file would only be a copy of these rows that can
	// disagree with them.
	m_LevelingTracker.Refresh();
}

void CAccountListener::OnTick()
{
	// The list is one global object; refreshing it from every world would run
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


	// Each matching profession gets two aura layers.
	const auto AccountID = pPlayer->Account()->GetID();
	bool IsTopExpert = false;
	for(int Layer = 0; Layer < 2; ++Layer)
	{
		for(const auto& Profession : s_aLevelingData)
		{
			const auto* pLeader = m_LevelingTracker.GetLeader(Profession.m_Identifier);
			if(pLeader && pLeader->AccountID == AccountID)
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
	for(auto& Profession : pAccount->GetProfessions())
		m_LevelingTracker.UpdateIfNecessary(pPlayer, Profession.GetProfessionID(), Profession.GetLevel());
}


void CAccountListener::OnPlayerProfessionLeveling(CPlayer* pPlayer, CProfession* pProfession, int NewLevel)
{
	m_LevelingTracker.UpdateIfNecessary(pPlayer, pProfession->GetProfessionID(), NewLevel);
}


// leveling tracker
void CLevelingTracker::Tick()
{
	if(m_RefreshDelay > 0)
		--m_RefreshDelay;
	Refresh();
}


void CLevelingTracker::Refresh()
{
	// The wait belongs to the tracker, not to the caller: a menu that opens
	// while a read is running or while a retry is backing off must not queue
	// another one.
	if(m_Refreshing || m_RefreshDelay > 0)
		return;

	m_Refreshing = true;
	AccountDb::ListProfessionLeaders(TRACKING_TAG).Then([this](Db::CResultSet&& Rows)
	{
		Apply(std::move(Rows));
	});
}


void CLevelingTracker::Apply(Db::CResultSet&& Rows)
{
	m_Refreshing = false;

	if(!Rows.Ok())
	{
		// Never drop what is known because a read failed: an empty specialists
		// list is exactly the state this tracker exists to avoid.
		dbg_msg("leveling_tracking", "could not read the profession leaders (%s)", Rows.Error().ToString().c_str());
	}
	else
	{
		// The rows come ordered by level, so the first row of a profession is
		// its leader. Rows that the database could not order (a document without
		// a level) are not part of the answer.
		bool Any = false;
		while(Rows.Next())
		{
			const int ProfessionID = Rows.GetInt("ProfessionID");
			const int AccountID = Rows.GetInt("UserID");
			const int Level = Rows.GetInt("Level");
			if(ProfessionID < 0 || ProfessionID >= NUM_ENTRIES || AccountID <= 0 || Level <= 0)
				continue;

			// Merged, not assigned: a level gained while the read was in flight
			// is newer than the answer and must not be rolled back.
			auto& Leader = m_aLeaders[ProfessionID];
			if(Level > Leader.Level)
				Leader = { AccountID, Level };
			Any = true;
		}

		if(Any)
			m_Loaded = true;
	}

	// An answer with a leader is repeated every few minutes; a failed or empty
	// one is retried with a growing pause.
	if(m_Loaded)
	{
		m_Retries = 0;
		m_RefreshDelay = LEVELING_REFRESH_SECONDS * std::max(1, Instance::Server()->TickSpeed());
		return;
	}

	m_Retries = std::min(m_Retries + 1, 16);
	int Delay = LEVELING_RETRY_MIN_SECONDS;
	for(int i = 1; i < m_Retries && Delay < LEVELING_RETRY_MAX_SECONDS; ++i)
		Delay = std::min(Delay * 2, LEVELING_RETRY_MAX_SECONDS);
	m_RefreshDelay = Delay * std::max(1, Instance::Server()->TickSpeed());
}


void CLevelingTracker::UpdateIfNecessary(CPlayer* pPlayer, ProfessionIdentifier Profession, int Level)
{
	const int ProfessionID = (int)Profession;
	if(ProfessionID < 0 || ProfessionID >= NUM_ENTRIES || Level <= 0)
		return;

	auto& Leader = m_aLeaders[ProfessionID];
	if(Leader.Level < Level)
	{
		Leader = { pPlayer->Account()->GetID(), Level };
		m_Loaded = true;
	}
}


const TrackingLevelingData* CLevelingTracker::GetLeader(ProfessionIdentifier Profession) const
{
	const int ProfessionID = (int)Profession;
	if(ProfessionID < 0 || ProfessionID >= NUM_ENTRIES)
		return nullptr;

	const auto& Leader = m_aLeaders[ProfessionID];
	return Leader.AccountID > 0 && Leader.Level > 0 ? &Leader : nullptr;
}
