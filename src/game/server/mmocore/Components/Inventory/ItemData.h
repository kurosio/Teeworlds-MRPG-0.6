/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ITEM_DATA_H
#define GAME_SERVER_COMPONENT_ITEM_DATA_H

#include "ItemInfoData.h"

class CItem
{
protected:
	int m_ItemID;
	int m_Value;
	int m_Enchant;
	int m_Durability;
	int m_Settings;

public:
	CItem() = default;
	CItem(int ID, int Value, int Enchant, int Durability, int Settings) : m_ItemID(ID), m_Value(Value), m_Enchant(Enchant), m_Durability(Durability),m_Settings(Settings) {}

	// getters
	int GetID() const { return m_ItemID; }
	int GetValue() const { return m_Value; }
	int GetEnchant() const{return m_Enchant;}
	int GetDurability() const{return m_Durability;}
	int GetSettings() const{ return m_Settings; }

	// virtual functions
	virtual void SetID(int ID) { m_ItemID = ID; }
	virtual bool SetValue(int Value) { m_Value = Value; return true; }
	virtual bool SetEnchant(int Enchant) { m_Enchant = Enchant; return true; }
	virtual bool SetDurability(int Durability){ m_Durability = Durability; return true; }
	virtual bool SetSettings(int Settings) { m_Settings = Settings; return true; }

	CItemDataInfo* Info() const { return &CItemDataInfo::ms_aItemsInfo[m_ItemID]; }
};

class CPlayerItem : public CItem, public MultiworldIdentifiableStaticData< std::map < int, std::map < int, CPlayerItem > > >
{
	friend class CInventoryCore;
	class IServer* m_pServer;
	int m_ClientID;

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CPlayerItem() = default;
	CPlayerItem(class IServer* pServer, int ID, int ClientID) : m_pServer(pServer), m_ClientID(ClientID) { m_ItemID = ID; }
	
	void Init(int Value, int Enchant, int Durability, int Settings)
	{
		m_Value = Value;
		m_Enchant = Enchant;
		m_Durability = Durability;
		m_Settings = Settings;
		CPlayerItem::m_pData[m_ClientID][m_ItemID] = *this;
	}
	
	static bool IsValidItem(int ID) { return m_pData.find(ID) != m_pData.end(); }

	// equip modules types functions
	int GetEnchantStats(AttributeIdentifier ID) const { return Info()->GetInfoEnchantStats(ID, m_Enchant); }
	int GetEnchantPrice() const { return Info()->GetEnchantPrice(m_Enchant); }
	bool IsEquipped() const
	{
		return m_Value > 0 && m_Settings > 0 && (Info()->IsType(ItemType::TYPE_POTION) || Info()->IsType(ItemType::TYPE_SETTINGS) 
			|| Info()->IsType(ItemType::TYPE_MODULE) || Info()->IsType(ItemType::TYPE_EQUIP));
	}
	bool IsEnchantMaxLevel() const { return Info()->IsEnchantMaxLevel(m_Enchant); }
	bool HasItem() const { return m_Value > 0; }

	// main functions
	bool Add(int Value, int Settings = 0, int Enchant = 0, bool Message = true);
	bool Remove(int Value, int Settings = 0);
	bool Equip();
	bool Use(int Value);
	bool Drop(int Value);
	void StrFormatEnchantLevel(char* pBuffer, int Size) const { Info()->StrFormatEnchantLevel(pBuffer, Size, m_Enchant); }
	void StrFormatAttributes(CPlayer* pPlayer, char* pBuffer, int Size) const { Info()->StrFormatAttributes(pPlayer, pBuffer, Size, m_Enchant); }

	// override functions
	void SetID(int ID) override { /* disabled for player item data */}
	bool SetValue(int Value) override;
	bool SetEnchant(int Enchant) override;
	bool SetDurability(int Durability) override;
	bool SetSettings(int Settings) override;
private:
	bool Save() const;
};

#endif