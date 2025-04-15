#include "equipped_slots.h"

#include <game/server/gamecontext.h>

void EquippedSlots::initSlot(ItemType type, std::optional<int> itemID)
{
	m_Slots[type] = itemID;
}

void EquippedSlots::load(std::string EquippedSlots)
{
	// loading from data
	mystd::json::parse(EquippedSlots, [this](nlohmann::json& json)
	{
		for(auto& [T, ItemID] : json.items())
		{
			auto Type = (ItemType)std::stoi(T);
			if(m_Slots.contains(Type))
				m_Slots[Type] = ItemID;
		}
	});
}

bool EquippedSlots::equipItem(int ItemID)
{
	if(!CItemDescription::Data().contains(ItemID))
		return false;

	auto* pGS = (CGS*)Instance::GameServer();
	auto* pItemInfo = pGS->GetItemInfo(ItemID);
	if(!m_Slots.contains(pItemInfo->GetType()))
		return false;

	if(m_Slots[pItemInfo->GetType()] == ItemID)
		return false;

	m_Slots[pItemInfo->GetType()] = ItemID;
	return true;
}

bool EquippedSlots::unequipItem(int ItemID)
{
	if(!CItemDescription::Data().contains(ItemID))
		return false;

	auto* pGS = (CGS*)Instance::GameServer();
	auto* pItemInfo = pGS->GetItemInfo(ItemID);
	if(!m_Slots.contains(pItemInfo->GetType()))
		return false;

	if(m_Slots[pItemInfo->GetType()] != ItemID)
		return false;

	m_Slots[pItemInfo->GetType()] = std::nullopt;
	return true;
}

nlohmann::json EquippedSlots::dumpJson() const
{
    nlohmann::json js {};

	for(auto& [Type, Value] : m_Slots)
	{
		js[std::to_string((int)Type)] = Value.value_or(-1);
	}

    return js;
}
