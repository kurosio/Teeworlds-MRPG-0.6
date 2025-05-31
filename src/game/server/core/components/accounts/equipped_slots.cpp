#include "equipped_slots.h"

#include <game/server/gamecontext.h>

void EquippedSlots::initSlot(ItemType type, std::optional<int> itemID)
{
	auto it = findSlotIter(m_Slots, type);
	if(it != m_Slots.end())
		it->second = itemID;
	else
		m_Slots.emplace_back(type, itemID);
}

bool EquippedSlots::hasSlot(ItemType type) const
{
	return findSlotIter(m_Slots, type) != m_Slots.end();
}

std::optional<int> EquippedSlots::getEquippedItemID(ItemType type) const
{
	auto it = findSlotIter(m_Slots, type);
	return it != m_Slots.end() ? it->second : std::nullopt;
}

void EquippedSlots::load(const std::string& equippedSlotsJson)
{
	mystd::json::parse(equippedSlotsJson, [this](nlohmann::json& jsonData)
	{
		if(!jsonData.is_object())
			return;

		for(auto& [strType, jsonItemID] : jsonData.items())
		{
			const auto currentItemType = (ItemType)std::stoi(strType);
			auto it = findSlotIter(m_Slots, currentItemType);
			if(it == m_Slots.end())
				continue;

			if(jsonItemID.is_number_integer())
			{
				CItem item(jsonItemID.get<int>(), 1);
				it->second = (item.IsValid() && item.Info()->IsType(currentItemType) ? std::make_optional<int>(item.GetID()) : std::nullopt);
			}
			else if(jsonItemID.is_null())
			{
				it->second = std::nullopt;
			}
		}
	});
}

bool EquippedSlots::equipItem(int itemID)
{
	if(!CItemDescription::Data().contains(itemID))
		return false;

	const auto* pGS = (CGS*)Instance::GameServer();
	const auto* pItemInfo = pGS->GetItemInfo(itemID);
	const auto typeToEquip = pItemInfo->GetType();
	auto it = findSlotIter(m_Slots, typeToEquip);

	if(it == m_Slots.end() || (it->second.has_value() && it->second.value() == itemID))
		return false;

	it->second = itemID;
	return true;
}

bool EquippedSlots::unequipItem(int itemID)
{
	if(!CItemDescription::Data().contains(itemID))
		return false;

	auto* pGS = (CGS*)Instance::GameServer();
	auto* pItemInfo = pGS->GetItemInfo(itemID);
	const auto typeToUnequip = pItemInfo->GetType();
	auto it = findSlotIter(m_Slots, typeToUnequip);

	if(it == m_Slots.end() || !it->second.has_value() || it->second.value() != itemID)
		return false;

	it->second = std::nullopt;
	return true;
}

nlohmann::json EquippedSlots::dumpJson() const
{
	nlohmann::json js = nlohmann::json::object();
	for(const auto& [slotTypeId, slotItemId] : m_Slots)
		js[std::to_string((int)slotTypeId)] = slotItemId.value_or(NOPE);

	return js;
}
