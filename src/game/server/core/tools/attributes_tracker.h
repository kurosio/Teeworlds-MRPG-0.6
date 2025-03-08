#ifndef GAME_SERVER_CORE_TOOLS_ATTRIBUTES_TRACKER_H
#define GAME_SERVER_CORE_TOOLS_ATTRIBUTES_TRACKER_H

struct TrackingAttributeData
{
	int AccountID {};
	size_t Amount {};
};


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


class CAttributesTracker
{
	std::unordered_map<int, TrackingAttributeData> m_vTrackingData {};

public:
	void Load();
	void Save();
	void ModifyIfNeeded(int AttributeID, int AccountID, size_t Amount);
	std::optional<TrackingAttributeData> Get(int AttributeID) const;
};

#endif