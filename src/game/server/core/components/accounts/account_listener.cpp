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
	if(!m_vTrackingData.contains(ProfessionID))
		return std::nullopt;

	return m_vTrackingData.at(ProfessionID);
}
