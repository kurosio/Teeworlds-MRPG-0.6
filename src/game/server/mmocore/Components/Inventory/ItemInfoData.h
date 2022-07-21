/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H
#define GAME_SERVER_COMPONENT_ITEM_DATA_INFO_H

// define for quick fine-tuning of enchantment
#define PERCENT_OF_ENCHANT 8
#define PERCENT_MAXIMUM_ENCHANT 40

class CItemDataInfo : public CItemDataInformation
{
public:
	// main functions
	const char* GetName() const { return m_aName; }
	const char* GetDesc() const { return m_aDesc; }
	const char* GetIcon() const { return m_aIcon; }

	// equip modules types functions
	int GetInfoEnchantStats(int AttributeID) const;
	int GetInfoEnchantStats(int AttributeID, int Enchant) const;
	int GetEnchantPrice(int EnchantLevel) const;

	bool IsEnchantable() const;
	bool IsEnchantMaxLevel(int Enchant) const;

	void FormatAttributes(class CPlayer* pPlayer, char* pBuffer, int Size, int Enchant) const;
	void FormatEnchantLevel(char* pBuffer, int Size, int Enchant) const;

	static std::map< int, CItemDataInfo > ms_aItemsInfo;
};

#endif