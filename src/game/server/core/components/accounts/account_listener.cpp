#include "account_listener.h"
#include <game/server/gamecontext.h>

CAccountListener g_AccountListener;
constexpr const char* LEVELING_TRACKING_FILE_NAME = "server_data/leveling_tracking.json";

// account listener
void CAccountListener::Initialize()
{
	g_EventListenerManager.RegisterListener(IEventListener::CharacterSpawn, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerLogin, this);
	g_EventListenerManager.RegisterListener(IEventListener::PlayerProfessionLeveling, this);
	m_LevelingTracker.LoadTrackingData();
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

	// sync by database
	auto pRes = Database->Execute<DB::SELECT>("UserID, ProfessionID, Data", "tw_accounts_professions", "");
	std::unordered_map<int, TrackingLevelingData> vFreshTracking {};
	while(pRes->next())
	{
		const int ProfessionID = pRes->getInt("ProfessionID");
		const int AccountID = pRes->getInt("UserID");
		if(ProfessionID < 0 || AccountID <= 0)
			continue;

		int Level = 1;
		const auto Data = pRes->getString("Data");
		const bool ParseError = mystd::json::parse(Data, [&Level](nlohmann::json& jsonData)
		{
			Level = maximum(1, jsonData.value("level", 1));
		});

		if(ParseError)
			continue;

		auto& Entry = vFreshTracking[ProfessionID];
		if(Level > Entry.Level)
		{
			Entry.AccountID = AccountID;
			Entry.Level = Level;
		}
	}

	m_vTrackingData = std::move(vFreshTracking);
	SaveTrackingData();
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

	std::string Data = j.dump(4);
	auto Result = mystd::file::save(LEVELING_TRACKING_FILE_NAME, Data.data(), static_cast<unsigned>(Data.size()));
	if(Result != mystd::file::result::SUCCESSFUL)
	{
		dbg_msg("leveling_tracking", "Failed to save the leveling. Re-creating file.");
		mystd::file::remove(LEVELING_TRACKING_FILE_NAME);
		m_vTrackingData.clear();
		SaveTrackingData();
	}
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
