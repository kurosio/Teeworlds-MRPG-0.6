/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H
#define GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H

// define for quick fine-tuning of enchantment
#define PERCENT_OF_ENCHANT 8
#define PERCENT_MAXIMUM_ENCHANT 40

#include "AttributeData.h"
#include "RandomBox.h"

using ItemIdentifier = int;

class CItemDescription : public MultiworldIdentifiableStaticData < std::map< int, CItemDescription > >
{
public:
	using ContainerAttributes = std::deque< CAttribute >;

private:
	friend class CInventoryCore;

	ItemIdentifier m_ID{};
	char m_aName[32]{};
	char m_aDescription[64]{};
	ItemType m_Type{};
	int m_Dysenthis{};
	int m_InitialPrice{};
	ItemFunctional m_Function{};
	ContainerAttributes m_aAttributes{};
	std::string m_Data{};
	CRandomBox m_RandomBox{};

public:
	CItemDescription() = default;
	CItemDescription(ItemIdentifier ID) : m_ID(ID) {}

	void Init(const std::string& Name, const std::string& Description, ItemType Type, int Dysenthis, int InitialPrice, ItemFunctional Function, ContainerAttributes aAttributes, std::string Data)
	{
		m_Data = Data;
		JsonTools::parseFromString(m_Data, [this](nlohmann::json& pJson)
		{
			// Parse random box
			if(pJson.find("random_box") != pJson.end())
			{
				for(auto& p : pJson["random_box"])
				{
					int ItemID = p.value("item_id", -1);
					int Value = p.value("value", 1);
					float Chance = p.value("chance", 100.0f);
					m_RandomBox.add(ItemID, Value, Chance);
				}
			}
		});

		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aDescription, Description.c_str(), sizeof(m_aDescription));
		m_Type = Type;
		m_Dysenthis = Dysenthis;
		m_InitialPrice = InitialPrice;
		m_Function = Function;
		m_aAttributes = std::move(aAttributes);
		CItemDescription::m_pData[m_ID] = *this;
	}

	ItemIdentifier GetID() const { return m_ID; }

	// main functions
	const char* GetName() const { return m_aName; }
	const char* GetDescription() const { return m_aDescription; }
	int GetInitialPrice() const { return m_InitialPrice; }
	int GetDysenthis() const { return m_Dysenthis; }
	ItemFunctional GetFunctional() const { return m_Function; }
	bool IsFunctional(ItemFunctional Functional) const { return m_Function == Functional; }
	ItemType GetType() const { return m_Type; }
	bool IsType(ItemType Type) const { return m_Type == Type; }
	class CRandomBox* GetRandomBox() { return m_RandomBox.empty() ? nullptr : &m_RandomBox; }

	bool IsEnchantable() const;
	bool IsEnchantMaxLevel(int Enchant) const;

	// equip modules types functions
	int GetInfoEnchantStats(AttributeIdentifier ID) const;
	int GetInfoEnchantStats(AttributeIdentifier ID, int Enchant) const;
	int GetEnchantPrice(int EnchantLevel) const;
	void StrFormatAttributes(class CPlayer* pPlayer, char* pBuffer, int Size, int Enchant) const;
	void StrFormatEnchantLevel(char* pBuffer, int Size, int Enchant) const;
};

#endif