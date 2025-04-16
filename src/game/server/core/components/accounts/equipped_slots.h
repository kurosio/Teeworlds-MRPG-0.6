#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_EQUIPPED_SLOTS_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_EQUIPPED_SLOTS_H

class CGS;
class CAccountData;

class EquippedSlots
{
	std::unordered_map<ItemType, std::optional<int>> m_Slots {};

public:
	EquippedSlots() = default;

	void initSlot(ItemType type, std::optional<int> itemID = std::nullopt);

	const std::unordered_map<ItemType, std::optional<int>>& getSlots() const
	{
		return m_Slots;
	}

	bool hasSlot(ItemType Slot)
	{
		return m_Slots.contains(Slot);
	}

	std::optional<int> getEquippedItemID(ItemType Type) const
	{
		return m_Slots.contains(Type) ? m_Slots.at(Type) : std::nullopt;
	}

	bool equipItem(int ItemID);
	bool unequipItem(int ItemID);
	void load(std::string EquippedSlots);

	nlohmann::json dumpJson() const;
};

#endif