#include "attributes_tracker.h"
#include <game/server/gamecontext.h>

constexpr const char* TRACKING_FILE_NAME = "server_data/attribute_tracking.json";


CAttributesTracker::CAttributesTracker()
{
	auto* pGS = (CGS*)Instance::GameServer();
	pGS->EventListener()->RegisterListener(IEventListener::Type::PlayerAttributeUpdate, this);
}


void CAttributesTracker::OnPlayerAttributeUpdate(CPlayer* pPlayer, int AttributeID, size_t Amount)
{
	if(m_vTrackingData[AttributeID].Amount >= Amount)
		return;

	TrackingAttributeData Data;
	Data.AccountID = pPlayer->Account()->GetID();
	Data.Amount = Amount;
	m_vTrackingData[AttributeID] = Data;
	Save();
}


void CAttributesTracker::Load()
{
	ByteArray RawData;
	if(!mystd::file::load(TRACKING_FILE_NAME, &RawData))
	{
		Save();
		return;
	}

	auto j = nlohmann::json::parse((char*)RawData.data());
	m_vTrackingData = j.value("tracking", std::unordered_map<int, TrackingAttributeData>{});
}


void CAttributesTracker::Save()
{
	nlohmann::json j =
	{
		{"tracking", m_vTrackingData}
	};

	std::string Data = j.dump(4);
	auto Result = mystd::file::save(TRACKING_FILE_NAME, Data.data(), static_cast<unsigned>(Data.size()));
	if(Result == mystd::file::result::SUCCESSFUL)
	{
		dbg_msg("attributes_tracking", "Failed to save the attributes.");
	}
}


std::optional<TrackingAttributeData> CAttributesTracker::Get(int AttributeID) const
{
	if(!m_vTrackingData.contains(AttributeID))
		return std::nullopt;

	return m_vTrackingData.at(AttributeID);
}
