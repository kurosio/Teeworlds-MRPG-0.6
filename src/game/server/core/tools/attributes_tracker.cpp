#include "attributes_tracker.h"

constexpr const char* TRACKING_FILE_NAME = "attribute_tracking.json";

namespace nlohmann
{
	inline void to_json(nlohmann::json& j, const TrackingAttributeData& data)
	{
		j = nlohmann::json {
			{"AccountID", data.AccountID},
			{"Amount", data.Amount}
		};
	}

	inline void from_json(const nlohmann::json& j, TrackingAttributeData& data)
	{
		j.at("AccountID").get_to(data.AccountID);
		j.at("Amount").get_to(data.Amount);
	}
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

void CAttributesTracker::Update(int AttributeID, TrackingAttributeData Data)
{
}

std::optional<TrackingAttributeData> CAttributesTracker::Get(int AttributeID) const
{
	if(!m_vTrackingData.contains(AttributeID))
		return std::nullopt;

	return m_vTrackingData.at(AttributeID);
}
