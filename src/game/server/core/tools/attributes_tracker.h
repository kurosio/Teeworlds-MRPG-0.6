#ifndef GAME_SERVER_CORE_TOOLS_ATTRIBUTES_TRACKER_H
#define GAME_SERVER_CORE_TOOLS_ATTRIBUTES_TRACKER_H

struct TrackingAttributeData
{
	int AccountID {};
	size_t Amount {};
};


class CAttributesTracker
{
	std::unordered_map<int, TrackingAttributeData> m_TrackingData {};

public:
	void Load();
	void Save();
	void Update(int AttributeID, TrackingAttributeData Data);
	std::optional<TrackingAttributeData> Get(int AttributeID) const;
};

#endif