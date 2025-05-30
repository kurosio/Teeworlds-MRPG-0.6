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


void CAccountListener::OnCharacterSpawn(CPlayer* pPlayer)
{
	if(pPlayer->IsBot())
		return;

	struct LevelingData
	{
		ProfessionIdentifier identifier;
		int weaponType;
		int orbiteType;
	};

	bool bestExpert = false;
	const auto AccountID = pPlayer->Account()->GetID();
	static std::array<LevelingData, (int)ProfessionIdentifier::NUM_PROFESSIONS> aLevelingData = {
		LevelingData{ProfessionIdentifier::Tank, WEAPON_SHOTGUN, MULTIPLE_ORBITE_TYPE_EIGHT},
		LevelingData{ProfessionIdentifier::Dps, WEAPON_SHOTGUN, MULTIPLE_ORBITE_TYPE_DYNAMIC_CENTER},
		LevelingData{ProfessionIdentifier::Healer, WEAPON_SHOTGUN, MULTIPLE_ORBITE_TYPE_PULSATING},
		LevelingData{ProfessionIdentifier::Miner, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_EIGHT},
		LevelingData{ProfessionIdentifier::Farmer, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_DYNAMIC_CENTER},
		LevelingData{ProfessionIdentifier::Fisherman, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_PULSATING},
		LevelingData{ProfessionIdentifier::Loader, WEAPON_HAMMER, MULTIPLE_ORBITE_TYPE_ELLIPTICAL}
	};

	for(const auto& data : aLevelingData)
	{
		const auto& Biggest = m_LevelingTracker.GetTrackingData((int)data.identifier);
		if(Biggest && AccountID == (*Biggest).AccountID)
		{
			pPlayer->GetCharacter()->AddMultipleOrbite(true, 2, data.weaponType, 0, data.orbiteType);
			bestExpert = true;
		}
	}

	if(bestExpert)
	{
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You are the top expert in your profession on the server. Visual highlighting is now active.");
	}
}


// leveling tracker
void CLevelingTracker::LoadTrackingData()
{
	ByteArray RawData;
	if(!mystd::file::load(LEVELING_TRACKING_FILE_NAME, &RawData))
	{
		SaveTrackingData();
		return;
	}

	std::string rawString = (char*)RawData.data();
	bool hasError = mystd::json::parse(rawString, [this](nlohmann::json& jsonData)
	{
		for(const auto& item : jsonData["tracking"])
		{
			int professionID = item.value("profession_id", -1);
			TrackingLevelingData data = item.value("detail", TrackingLevelingData {});
			m_vTrackingData[professionID] = data;
		}
	});

	if(hasError)
	{
		dbg_msg("leveling_tracking", "Error with initialized '%s'. Creating new...", LEVELING_TRACKING_FILE_NAME);
		m_vTrackingData.clear();
		SaveTrackingData();
	}
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