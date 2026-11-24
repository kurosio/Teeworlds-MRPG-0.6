#include "equipped_slots.h"

#include <game/server/gamecontext.h>

void EquippedSlots::initSlot(ItemType type, std::optional<int> itemID)
{
	auto it = findSlotIter(type);
	if(it != m_Slots.end())
		it->second = itemID;
	else
		m_Slots.emplace_back(type, itemID);
}

bool EquippedSlots::hasSlot(ItemType type) const
{
	return findSlotIter(type) != m_Slots.end();
}

std::optional<int> EquippedSlots::getEquippedItemID(ItemType type) const
{
	auto it = findSlotIter(type);
	return it != m_Slots.end() ? it->second : std::nullopt;
}

void EquippedSlots::load(const std::string& equippedSlotsJson)
{
	mystd::json::parse(equippedSlotsJson, [this](nlohmann::json& Json) { load(Json); });
}

void EquippedSlots::load(const nlohmann::json& equippedSlotsJson)
{
	if(!equippedSlotsJson.is_object())
		return;

	// Walk our own slots and read their entry: the stored keys are the slot
	// types, so nothing has to be converted from a string to find them.
	for(auto& [slotType, itemID] : m_Slots)
	{
		const auto It = equippedSlotsJson.find(std::to_string((int)slotType));
		if(It == equippedSlotsJson.end())
			continue;

		if(It->is_number_integer())
		{
			CItem item(It->get<int>(), 1);
			itemID = (item.IsValid() && item.Info()->IsType(slotType) ? std::make_optional<int>(item.GetID()) : std::nullopt);
		}
		else if(It->is_null())
		{
			itemID = std::nullopt;
		}
	}
}

bool EquippedSlots::equipItem(int itemID)
{
	if(!CItemDescription::Data().contains(itemID))
		return false;

	const auto* pGS = (CGS*)Instance::GameServer();
	const auto* pItemInfo = pGS->GetItemInfo(itemID);
	const auto typeToEquip = pItemInfo->GetType();
	auto it = findSlotIter(typeToEquip);

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
	auto it = findSlotIter(typeToUnequip);

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
