/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_ITEM_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_ITEM_DATA_H

#include "item_info_data.h"

class CItem;
using CItemsContainer = std::deque<CItem>;

// item
class CItem
{
protected:
	ItemIdentifier m_ID {};
	int m_Value {};
	int m_Enchant {};
	int m_Durability {};
	int m_Settings {};

public:
	CItem() = default;
	CItem(ItemIdentifier ID, int Value = 0, int Enchant = 0, int Durability = 100, int Settings = 0) :
		m_ID(ID), m_Value(Value), m_Enchant(Enchant), m_Durability(Durability), m_Settings(Settings)
	{
	}

	// getters
	ItemIdentifier GetID() const { return m_ID; }
	int GetValue() const { return m_Value; }
	int GetEnchant() const { return m_Enchant; }
	int GetDurability() const { return m_Durability; }
	int GetSettings() const { return m_Settings; }
	bool IsValid() const { return CItemDescription::Data().contains(m_ID) && m_Value > 0; }
	CItemDescription* Info() const { return &CItemDescription::Data()[m_ID]; }
	std::string GetStringEnchantLevel() const { return Info()->GetStringEnchantLevel(m_Enchant); }

	// setters
	void SetID(ItemIdentifier ID) { m_ID = ID; }

	virtual bool SetValue(int Value)
	{
		m_Value = Value;
		return true;
	}
	virtual bool SetEnchant(int Enchant)
	{
		m_Enchant = Enchant;
		return true;
	}
	virtual bool SetDurability(int Durability)
	{
		m_Durability = Durability;
		return true;
	}
	virtual bool SetSettings(int Settings)
	{
		m_Settings = Settings;
		return true;
	}
};



// player item
class CPlayerItem : public CItem, public MultiworldIdentifiableData<std::map<int, std::map<int, CPlayerItem>>>
{
	friend class CInventoryManager;
	int m_ClientID {};

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CPlayerItem() = default;
	CPlayerItem(ItemIdentifier ID, int ClientID) : CItem(ID), m_ClientID(ClientID) {}
	CPlayerItem(ItemIdentifier ID, int ClientID, int Value, int Enchant, int Durability, int Settings) :
		CItem(ID, Value, Enchant, Durability, Settings), m_ClientID(ClientID)
	{
	}

	void Init(int Value, int Enchant, int Durability, int Settings)
	{
		m_Value = Value;
		m_Enchant = Enchant;
		m_Durability = Durability;
		m_Settings = Settings;
		CPlayerItem::m_pData[m_ClientID][m_ID] = *this;
	}

	// getters
	int GetTotalAttributesLevel() const { return Info()->GetTotalAttributesLevel(m_Enchant); }
	std::optional<float> GetEnchantAttributeChance(AttributeIdentifier ID) const { return Info()->GetEnchantAttributeChance(ID, m_Enchant); }
	int GetEnchantAttributeValue(AttributeIdentifier ID) const { return Info()->GetEnchantAttributeValue(ID, m_Enchant); }
	int GetEnchantPrice() const { return Info()->GetEnchantPrice(m_Enchant); }
	bool IsEquipped() const;
	bool IsEnchantMaxLevel() const { return Info()->IsEnchantMaxLevel(m_Enchant); }
	bool HasItem() const { return m_Value > 0; }
	std::string GetStringAttributesInfo(CPlayer* pPlayer) const { return Info()->GetStringAttributesInfo(pPlayer, m_Enchant); }

	// main functions
	bool Add(int Value, int StartSettings = 0, int StartEnchant = 0, bool Message = true);
	bool Remove(int Value);
	bool Equip();
	bool UnEquip();
	bool Use(int Value);
	bool Drop(int Value);
	bool Save();

	// override functions
	bool SetValue(int Value) override;
	bool SetEnchant(int Enchant) override;
	bool SetDurability(int Durability) override;
	bool SetSettings(int Settings) override;

private:
	bool ShouldAutoEquip() const;
};



// JSON ADL
inline void to_json(nlohmann::json& j, const CItem& data)
{
	j = nlohmann::json::object({
		{"id", data.GetID()},
		{"value", data.GetValue()}
		});

	// optional fields
	if(data.GetEnchant() != 0)
		j["enchant"] = data.GetEnchant();
	if(data.GetDurability() != 0)
		j["durability"] = data.GetDurability();
}

inline void from_json(const nlohmann::json& j, CItem& data)
{
	if(!j.is_object())
	{
		throw nlohmann::json::type_error::create(302, fmt_default("type must be object, but is {}", std::string(j.type_name())));
	}

	data.SetID(j.at("id").get<ItemIdentifier>());
	data.SetValue(j.at("value").get<int>());

	// optional fields
	data.SetEnchant(j.value("enchant", 0));
	data.SetDurability(j.value("durability", 0));
}

// JSON ADL for CItemsContainer=
inline void to_json(nlohmann::json& j, const CItemsContainer& container)
{
	j = nlohmann::json::array();
	for(const CItem& item : container)
		j.push_back(item);
}

inline void from_json(const nlohmann::json& j, CItemsContainer& container)
{
	if(!j.is_array())
		throw nlohmann::json::type_error::create(302, "type must be array to deserialize into CItemsContainer");

	container.clear();
	for(const auto& element_json : j)
		container.push_back(element_json.get<CItem>());
}

#endif