/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H
#define GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H

// define for quick fine-tuning of enchantment
#define PERCENT_OF_ENCHANT 8
#define PERCENT_MAXIMUM_ENCHANT 40

#include "attribute_data.h"
#include "RandomBox/RandomBoxData.h"

using ItemIdentifier = int;
class CItemDescription : public MultiworldIdentifiableData < std::map< int, CItemDescription > >
{
public:
	struct PotionContext
	{
		std::string Effect {};
		int Value {};
		int Lifetime {};
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
	int m_Dysenthis {};
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

	void Init(const std::string& Name, const std::string& Description, const DBSet& GroupSet,
		const DBSet& TypeSet, int Dysenthis, int InitialPrice, ContainerAttributes aAttributes, const std::string& Data, const std::string& ScenarioData)
	{
		m_Data = Data;
		m_ScenarioData = ScenarioData;
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aDescription, Description.c_str(), sizeof(m_aDescription));
		m_Dysenthis = Dysenthis;
		m_InitialPrice = InitialPrice;
		m_aAttributes = std::move(aAttributes);

		m_pData[m_ID] = *this;
		m_pData[m_ID].InitData(GroupSet, TypeSet);
	}
	void InitData(const DBSet& GroupSet, const DBSet& TypeSet);

	// main functions
	ItemIdentifier GetID() const { return m_ID; }
	std::string GetScenarioData() const { return m_ScenarioData; }
	const char* GetName() const { return m_aName; }
	const char* GetDescription() const { return m_aDescription; }
	int GetInitialPrice() const { return m_InitialPrice; }
	int GetDysenthis(int Enchant) const { return m_Dysenthis ? (m_Dysenthis + (maximum(GetEnchantPrice(Enchant) / 4, 1) * Enchant)) : 0; }
	ItemType GetType() const { return m_Type; }
	bool IsType(ItemType Type) const { return m_Type == Type; }
	ItemGroup GetGroup() const { return m_Group; }
	bool IsGroup(ItemGroup Group) const { return m_Group == Group; }

	bool IsEquipmentSlot() const
	{
		return (m_Group == ItemGroup::Equipment && (m_Type == ItemType::EquipHammer || m_Type == ItemType::EquipGun || m_Type == ItemType::EquipShotgun
			|| m_Type == ItemType::EquipGrenade || m_Type == ItemType::EquipLaser || m_Type == ItemType::EquipArmor
			|| m_Type == ItemType::EquipEidolon || m_Type == ItemType::EquipPickaxe || m_Type == ItemType::EquipRake
			|| m_Type == ItemType::EquipTitle || m_Type == ItemType::EquipPotionHeal || m_Type == ItemType::EquipPotionMana
			|| m_Type == ItemType::EquipFishrod || m_Type == ItemType::EquipGloves));
	}

	bool IsEquipmentNonSlot() const
	{
		return (m_Group == ItemGroup::Equipment && !IsEquipmentSlot());
	}

	class CRandomBox* GetRandomBox() { return m_RandomBox.IsEmpty() ? nullptr : &m_RandomBox; }
	ContainerAttributes& GetAttributes() { return m_aAttributes; }
	std::optional<PotionContext>& GetPotionContext() { return m_PotionContext; }
	std::optional<BonusesContext>& GetBonusContext() { return m_BonusContext; }

	bool IsStackable() const;
	bool IsEnchantable() const;
	bool IsEnchantMaxLevel(int Enchant) const;
	bool HasAttributes() const;

	// equip modules types functions
	int GetInfoEnchantStats(AttributeIdentifier ID) const;
	int GetInfoEnchantStats(AttributeIdentifier ID, int Enchant) const;
	int GetEnchantPrice(int EnchantLevel) const;
	std::string GetStringAttributesInfo(CPlayer* pPlayer, int Enchant) const;
	std::string GetStringEnchantLevel(int Enchant) const;

	// total data
	inline static std::unordered_map<int, PotionContext> s_vTotalPotionByItemIDList{};
};

#endif