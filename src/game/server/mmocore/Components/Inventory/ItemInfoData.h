/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H
#define GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H

// define for quick fine-tuning of enchantment
#define PERCENT_OF_ENCHANT 8
#define PERCENT_MAXIMUM_ENCHANT 40

#include "AttributeData.h"

class CItemDescription : public MultiworldIdentifiableStaticData < std::map< int, CItemDescription > >
{
public:
	using ContainerAttributes = std::deque< CAttribute >;
private:
	friend class CInventoryCore;

	int m_ItemID{};
	char m_aName[32]{};
	char m_aDesc[64]{};
	ItemType m_Type{};
	int m_Dysenthis{};
	int m_InitialPrice{};
	ItemFunctional m_Function{};
	ContainerAttributes m_aAttributes{};

public:
	CItemDescription() = default;
	CItemDescription(int ID) : m_ItemID(ID) {}

	void Init(const std::string& Name, const std::string& Description, ItemType Type, int Dysenthis, int InitialPrice, ItemFunctional Function, ContainerAttributes aAttributes)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aDesc, Description.c_str(), sizeof(m_aDesc));
		m_Type = Type;
		m_Dysenthis = Dysenthis;
		m_InitialPrice = InitialPrice;
		m_Function = Function;
		m_aAttributes = std::move(aAttributes);
		CItemDescription::m_pData[m_ItemID] = *this;
	}

	// main functions
	const char* GetName() const { return m_aName; }
	const char* GetDesc() const { return m_aDesc; }

	// equip modules types functions
	int GetInfoEnchantStats(AttributeIdentifier ID) const;
	int GetInfoEnchantStats(AttributeIdentifier ID, int Enchant) const;
	int GetEnchantPrice(int EnchantLevel) const;

	int GetID() const { return m_ItemID; }
	int GetInitialPrice() const { return m_InitialPrice; }
	int GetDysenthis() const { return m_Dysenthis; }

	ItemFunctional GetFunctional() const { return m_Function; }
	bool IsFunctional(ItemFunctional Functional) const { return m_Function == Functional; }

	ItemType GetType() const { return m_Type; }
	bool IsType(ItemType Type) const { return m_Type == Type; }

	bool IsEnchantable() const;
	bool IsEnchantMaxLevel(int Enchant) const;

	void StrFormatAttributes(class CPlayer* pPlayer, char* pBuffer, int Size, int Enchant) const;
	void StrFormatEnchantLevel(char* pBuffer, int Size, int Enchant) const;
};

#endif