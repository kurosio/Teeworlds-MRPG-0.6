/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_ITEM_INFO_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_ITEM_INFO_DATA_H

// define for quick fine-tuning of enchantment
#define PERCENT_OF_ENCHANT 8
#define PERCENT_MAXIMUM_ENCHANT 40

#include "attribute_data.h"
#include "random_box/random_box_data.h"

enum
{
	ITEMFLAG_CANT_DROP = 1 << 0,
	ITEMFLAG_CANT_TRADE = 1 << 1,
};

static void initItemFlags(int& outResult, const DBSet& set)
{
	outResult = 0;
	if(set.hasSet("Can't droppable"))
		outResult |= ITEMFLAG_CANT_DROP;
	if(set.hasSet("Can't tradeable"))
		outResult |= ITEMFLAG_CANT_TRADE;
}

using ItemIdentifier = int;
class CItemDescription : public MultiworldIdentifiableData < std::map< int, CItemDescription > >
{
public:
	struct PotionContext
	{
		std::string Effect {};
		int Value {};
		int Lifetime {};
		int Recasttime {};
	};
	struct BonusesContext
	{
		int Type{};
		int DurationDays {};
		int DurationHours {};
		int DurationMinutes {};
		float Amount {};
	};
	using ContainerAttributes = std::deque< CAttribute >;

private:
	friend class CInventoryManager;

	ItemIdentifier m_ID {};
	char m_aName[32] {};
	char m_aDescription[64] {};
	ItemGroup m_Group {};
	int m_Flags {};
	int m_InitialPrice {};
	ItemType m_Type {};
	ContainerAttributes m_aAttributes {};
	std::string m_Data {};
	std::string m_ScenarioData {};
	CRandomBox m_RandomBox {};
	std::optional<PotionContext> m_PotionContext {};
	std::optional<BonusesContext> m_BonusContext {};

public:
	CItemDescription() = default;
	CItemDescription(ItemIdentifier ID) : m_ID(ID) {}

	void Init(const std::string& Name, const std::string& Description, const DBSet& GroupSet, const DBSet& TypeSet, const DBSet& Flags,
		int InitialPrice, ContainerAttributes aAttributes, const std::string& Data, const std::string& ScenarioData)
	{
		m_Data = Data;
		m_ScenarioData = ScenarioData;
		str_copy(m_aDescription, Description.c_str(), sizeof(m_aDescription));
		m_InitialPrice = InitialPrice;
		m_aAttributes = std::move(aAttributes);
		initItemFlags(m_Flags, Flags);

		m_pData[m_ID] = *this;
		m_pData[m_ID].InitData(GroupSet, TypeSet);
		m_pData[m_ID].InitUniqueName(Name);
	}
	void InitData(const DBSet& GroupSet, const DBSet& TypeSet);
	void InitUniqueName(const std::string& Name);

	// main functions
	ItemIdentifier GetID() const { return m_ID; }
	std::string GetScenarioData() const { return m_ScenarioData; }
	const char* GetName() const { return m_aName; }
	const char* GetDescription() const { return m_aDescription; }
	int GetInitialPrice() const { return m_InitialPrice; }
	ItemType GetType() const { return m_Type; }
	bool IsType(ItemType Type) const { return m_Type == Type; }
	ItemGroup GetGroup() const { return m_Group; }
	bool IsGroup(ItemGroup Group) const { return m_Group == Group; }
	bool HasFlag(int Flag) const { return (m_Flags & Flag) != 0; }

	bool IsEquipmentSlot() const
	{
		return ((m_Group == ItemGroup::Equipment || m_Group == ItemGroup::Potion) && m_Type != ItemType::Default);
	}

	bool IsEquipmentModules() const
	{
		return (m_Group == ItemGroup::Equipment && m_Type == ItemType::Default);
	}

	bool IsGameSetting() const
	{
		return (m_Group == ItemGroup::Settings && m_Type == ItemType::Default);
	}

	class CRandomBox* GetRandomBox() { return m_RandomBox.IsEmpty() ? nullptr : &m_RandomBox; }
	ContainerAttributes& GetAttributes() { return m_aAttributes; }
	std::optional<PotionContext>& GetPotionContext() { return m_PotionContext; }
	std::optional<BonusesContext>& GetBonusContext() { return m_BonusContext; }

	bool IsStackable() const;
	bool IsEnchantable() const;
	bool IsEnchantMaxLevel(int Enchant) const;
	bool HasAttributes() const;
	int GetTotalAttributesLevel(int Enchant) const;

	// equip modules types functions
	int GetEnchantAttributeValue(AttributeIdentifier ID) const;
	int GetEnchantAttributeValue(AttributeIdentifier ID, int Enchant) const;
	std::optional<float> GetEnchantAttributeChance(AttributeIdentifier ID, int Enchant) const;
	int GetEnchantPrice(int EnchantLevel) const;
	std::string GetStringAttributesInfo(CPlayer* pPlayer, int Enchant) const;
	std::string GetStringEnchantLevel(int Enchant) const;

	// total data
	inline static std::unordered_map<int, PotionContext> s_vTotalPotionByItemIDList{};
};

#endif