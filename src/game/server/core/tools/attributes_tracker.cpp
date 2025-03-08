#include "attributes_tracker.h"

void CAttributesTracker::Load()
{
}

void CAttributesTracker::Save()
{
}

void CAttributesTracker::Update(int AttributeID, TrackingAttributeData Data)
{
}

std::optional<TrackingAttributeData> CAttributesTracker::Get(int AttributeID) const
{
	if(!m_TrackingData.contains(AttributeID))
		return std::nullopt;

	return m_TrackingData.at(AttributeID);
}
