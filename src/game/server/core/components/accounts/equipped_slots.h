#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_EQUIPPED_SLOTS_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_EQUIPPED_SLOTS_H

class CGS;
class CAccountData;

class EquippedSlots
{
public:
	using SlotEntry = std::pair<ItemType, std::optional<int>>;
	EquippedSlots() = default;

	void initSlot(ItemType type, std::optional<int> itemID = std::nullopt);
	bool equipItem(int itemID);
	bool unequipItem(int itemID);
	void load(const std::string& equippedSlotsJson);

	const std::vector<SlotEntry>& getSlots() const { return m_Slots; }
	bool hasSlot(ItemType type) const;
	std::optional<int> getEquippedItemID(ItemType type) const;
	nlohmann::json dumpJson() const;

private:
	std::vector<SlotEntry> m_Slots {};
	auto findSlotIter(const std::vector<EquippedSlots::SlotEntry>& slots, ItemType type) const
	{
		return std::ranges::find_if(slots, [type](const auto& entry)
			{ return entry.first == type; });
	}

	auto findSlotIter(std::vector<EquippedSlots::SlotEntry>& slots, ItemType type)
	{
		return std::ranges::find_if(slots, [type](const auto& entry)
			{ return entry.first == type; });
	}
};

#endif