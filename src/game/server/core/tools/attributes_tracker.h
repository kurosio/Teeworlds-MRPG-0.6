#ifndef GAME_SERVER_CORE_TOOLS_ATTRIBUTES_TRACKER_H
#define GAME_SERVER_CORE_TOOLS_ATTRIBUTES_TRACKER_H

#include "event_listener.h"

class CGS;
class CPlayer;


struct TrackingAttributeData
{
	int AccountID {};
	size_t Amount {};
};

inline void to_json(nlohmann::json& j, const TrackingAttributeData& data)
{
	j = nlohmann::json {
		{"account_id", data.AccountID},
		{"amount", data.Amount}
	};
}

inline void from_json(const nlohmann::json& j, TrackingAttributeData& data)
{
	j.at("account_id").get_to(data.AccountID);
	j.at("amount").get_to(data.Amount);
}


class CAttributesTracker : public IEventListener
{
	std::unordered_map<int, TrackingAttributeData> m_vTrackingData {};

public:
	void Init(CGS* pGS);
	std::optional<TrackingAttributeData> Get(int AttributeID) const;

protected:
	void OnPlayerAttributeUpdate(CPlayer* pPlayer, int AttributeID, size_t NewValue) override;

private:
	void Load();
	void Save();
};

extern CAttributesTracker g_AttributesTracker;

#endif