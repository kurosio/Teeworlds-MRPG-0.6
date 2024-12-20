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

enum class ItemScenarioEvent
{
	OnEventGot,
	OnEventLost,
	OnEventEquip,
	OnEventUnequip,
};

class CItemDescription : public MultiworldIdentifiableData < std::map< int, CItemDescription > >
{
public:
	struct HarvestingContext
	{
		int Level  {1};
		int Health {100};
	};

	struct PotionContext
	{
		std::string Effect {};
		int Value {};
		int Lifetime {};
	};

	using ContainerAttributes = std::deque< CAttribute >;

private:
	friend class CInventoryManager;

	ItemIdentifier m_ID {};
	char m_aName[32] {};
	char m_aDescription[64] {};
	ItemType m_Type {};
	int m_Dysenthis {};
	int m_InitialPrice {};
	ItemFunctional m_Function {};
	ContainerAttributes m_aAttributes {};
	std::string m_Data {};
	CRandomBox m_RandomBox {};
	std::optional<HarvestingContext> m_HarvestingContext {};
	std::optional<PotionContext> m_PotionContext {};

public:
	CItemDescription() = default;
	CItemDescription(ItemIdentifier ID) : m_ID(ID) {}

	void Init(const std::string& Name, const std::string& Description, ItemType Type, int Dysenthis, int InitialPrice, ItemFunctional Function, ContainerAttributes aAttributes, std::string&& Data)
	{
		m_Data = std::move(Data);
		mystd::json::parse(m_Data, [this](nlohmann::json& pJson)
		{
			// try to initialize harversing
			m_HarvestingContext = HarvestingContext{
				pJson.value("harvesting", nlohmann::json::object()).value("level", 1),
				pJson.value("harvesting", nlohmann::json::object()).value("health", 100)
			};

			// try to initialize random box
			for(auto& p : pJson["random_box"])
				m_RandomBox.Add(p.value("item_id", -1), p.value("value", 1), p.value("chance", 100.0f));
		});

		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aDescription, Description.c_str(), sizeof(m_aDescription));
		m_Type = Type;
		m_Dysenthis = Dysenthis;
		m_InitialPrice = InitialPrice;
		m_Function = Function;
		m_aAttributes = std::move(aAttributes);

		m_pData[m_ID] = *this;
		m_pData[m_ID].InitJsonData();
	}
	void InitJsonData();

	ItemIdentifier GetID() const { return m_ID; }

	// main functions
	void StartItemScenario(CPlayer* pPlayer, ItemScenarioEvent Event) const;
	const char* GetName() const { return m_aName; }
	const char* GetDescription() const { return m_aDescription; }
	int GetInitialPrice() const { return m_InitialPrice; }
	int GetDysenthis(int Enchant) const { return m_Dysenthis ? (m_Dysenthis + (maximum(GetEnchantPrice(Enchant) / 4, 1) * Enchant)) : 0; }
	ItemFunctional GetFunctional() const { return m_Function; }
	bool IsFunctional(ItemFunctional Functional) const { return m_Function == Functional; }
	ItemType GetType() const { return m_Type; }
	bool IsType(ItemType Type) const { return m_Type == Type; }
	class CRandomBox* GetRandomBox() { return m_RandomBox.IsEmpty() ? nullptr : &m_RandomBox; }
	ContainerAttributes& GetAttributes() { return m_aAttributes; }
	std::optional<HarvestingContext>& GetHarvestingContext() { return m_HarvestingContext; }
	std::optional<PotionContext>& GetPotionContext() { return m_PotionContext; }

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