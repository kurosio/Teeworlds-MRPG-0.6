/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ITEM_DATA_H
#define GAME_SERVER_COMPONENT_ITEM_DATA_H
#include "ItemInfoData.h"

class CItemData
{
	class CGS* m_pGS;
	class CPlayer* m_pPlayer;
	class CGS* GS() const { return m_pGS; }

public:
	int m_ItemID;
	int m_Value;
	int m_Settings;
	int m_Enchant;
	int m_Durability;

	CItemDataInfo& Info() const { return CItemDataInfo::ms_aItemsInfo[m_ItemID]; }
	int GetID() const { return m_ItemID; }

	// main functions
	void SetItemOwner(CPlayer* pPlayer);
	bool SetEnchant(int Enchant);
	bool SetSettings(int Settings);
	bool SetDurability(int Durability);

	bool Add(int Value, int Settings = 0, int Enchant = 0, bool Message = true);
	bool Remove(int Value, int Settings = 0);
	bool Equip();
	bool Use(int Value);
	bool Drop(int Value);

	// equip modules types functions
	int GetEnchantStats(int AttributeID) const { return Info().GetInfoEnchantStats(AttributeID, m_Enchant); }
	int GetEnchantPrice() const { return Info().GetEnchantPrice(m_Enchant); }

	bool IsEquipped() const { return m_Value > 0 && m_Settings > 0 && (Info().m_Type == ItemType::TYPE_POTION || Info().m_Type == ItemType::TYPE_SETTINGS || Info().m_Type == ItemType::TYPE_MODULE || Info().m_Type == ItemType::TYPE_EQUIP); }
	bool IsEnchantMaxLevel() const { return Info().IsEnchantMaxLevel(m_Enchant); }

	void FormatEnchantLevel(char* pBuffer, int Size) const { Info().FormatEnchantLevel(pBuffer, Size, m_Enchant); }
	void FormatAttributes(CPlayer* pPlayer, char* pBuffer, int Size) const { Info().FormatAttributes(pPlayer, pBuffer, Size, m_Enchant); }

private:
	bool Save() const;

public:
	static std::map < int, std::map < int, CItemData > > ms_aItems;
};

#endif