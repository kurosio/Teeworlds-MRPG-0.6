#include "attributes_tracker.h"
#include <game/server/gamecontext.h>

CAttributesTracker g_AttributesTracker;
constexpr const char* TRACKING_FILE_NAME = "server_data/attribute_tracking.json";


void CAttributesTracker::Init(CGS* pGS)
{
	Load();
	g_EventListenerManager.RegisterListener(IEventListener::Type::PlayerAttributeUpdate, this);
}


void CAttributesTracker::OnPlayerAttributeUpdate(CPlayer* pPlayer, int AttributeID, size_t Amount)
{
	auto& TrackingData = m_vTrackingData[AttributeID];

	// update highter amount
	bool NeededSafe = false;
	auto* pGS = pPlayer->GS();
	auto* pPlayerHighter = pGS->GetPlayerByUserID(TrackingData.AccountID);
	if(pPlayerHighter)
	{
		auto Current = pPlayerHighter->GetTotalAttributeValue((AttributeIdentifier)AttributeID);
		TrackingData.Amount = Current;
		NeededSafe = true;
	}

	// update personal amount
	if(TrackingData.Amount < Amount)
	{
		TrackingData.AccountID = pPlayer->Account()->GetID();
		TrackingData.Amount = Amount;
		NeededSafe = true;
	}

	// update
	if(NeededSafe)
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
