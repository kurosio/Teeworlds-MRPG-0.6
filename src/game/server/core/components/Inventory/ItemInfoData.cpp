/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemInfoData.h"

#include <game/server/gamecontext.h>

#include <game/server/core/scenarios/scenario_universal.h>

int CItemDescription::GetInfoEnchantStats(AttributeIdentifier ID) const
{
	for (const auto& Att : m_aAttributes)
	{
		AttributeIdentifier SearchID = Att.GetID();
		if(SearchID != ID)
			continue;

		if(SearchID < AttributeIdentifier::DMG || SearchID >= AttributeIdentifier::ATTRIBUTES_NUM)
			continue;

		return Att.GetValue();
	}

	return 0;
}

int CItemDescription::GetInfoEnchantStats(AttributeIdentifier ID, int Enchant) const
{
	const int StatSize = GetInfoEnchantStats(ID);
	if(StatSize <= 0)
		return 0;

	const int PercentEnchant = translate_to_percent_rest(StatSize, PERCENT_OF_ENCHANT);
	int EnchantStat = StatSize + (PercentEnchant * Enchant);

	// the case when with percent will back 0
	if(PercentEnchant <= 0)
		EnchantStat += Enchant;

	return EnchantStat;
}

int CItemDescription::GetEnchantPrice(int EnchantLevel) const
{
	int FinishedPrice = 0;
	for (const auto& Att : m_aAttributes)
	{
		if(Att.HasValue())
		{
			int UpgradePrice;
			AttributeGroup Type = Att.Info()->GetGroup();

			// strength stats
			if(Type == AttributeGroup::Hardtype)
				UpgradePrice = maximum(80, Att.Info()->GetUpgradePrice()) * 55;

			// weapon and job stats
			else if(Type == AttributeGroup::Job || Type == AttributeGroup::Weapon || Att.GetID() == AttributeIdentifier::LuckyDropItem)
				UpgradePrice = maximum(100, Att.Info()->GetUpgradePrice()) * 55;

			// other stats
			else
				UpgradePrice = maximum(5, Att.Info()->GetUpgradePrice()) * 55;

			const int PercentEnchant = maximum(1, translate_to_percent_rest(Att.GetValue(), PERCENT_OF_ENCHANT));
			FinishedPrice += UpgradePrice * (PercentEnchant * (1 + EnchantLevel));
		}
	}
	return FinishedPrice;
}

void CItemDescription::InitData(const DBSet& GroupSet, const DBSet& TypeSet)
{
	// initialize group
	if(GroupSet.hasSet("Quest"))
		m_Group = ItemGroup::Quest;
	else if(GroupSet.hasSet("Usable"))
		m_Group = ItemGroup::Usable;
	else if(GroupSet.hasSet("Resource"))
		m_Group = ItemGroup::Resource;
	else if(GroupSet.hasSet("Other"))
		m_Group = ItemGroup::Other;
	else if(GroupSet.hasSet("Settings"))
		m_Group = ItemGroup::Settings;
	else if(GroupSet.hasSet("Equipment"))
		m_Group = ItemGroup::Equipment;
	else if(GroupSet.hasSet("Decoration"))
		m_Group = ItemGroup::Decoration;
	else if(GroupSet.hasSet("Potion"))
		m_Group = ItemGroup::Potion;
	else if(GroupSet.hasSet("Currency"))
		m_Group = ItemGroup::Currency;

	// initialize type
	if(TypeSet.hasSet("Default"))
		m_Type = ItemType::NoFunctional;
	else if(TypeSet.hasSet("Equip hammer"))
		m_Type = ItemType::EquipHammer;
	else if(TypeSet.hasSet("Equip gun"))
		m_Type = ItemType::EquipGun;
	else if(TypeSet.hasSet("Equip shotgun"))
		m_Type = ItemType::EquipShotgun;
	else if(TypeSet.hasSet("Equip grenade"))
		m_Type = ItemType::EquipGrenade;
	else if(TypeSet.hasSet("Equip rifle"))
		m_Type = ItemType::EquipLaser;
	else if(TypeSet.hasSet("Equip pickaxe"))
		m_Type = ItemType::EquipPickaxe;
	else if(TypeSet.hasSet("Equip rake"))
		m_Type = ItemType::EquipRake;
	else if(TypeSet.hasSet("Equip armor"))
		m_Type = ItemType::EquipArmor;
	else if(TypeSet.hasSet("Equip eidolon"))
		m_Type = ItemType::EquipEidolon;
	else if(TypeSet.hasSet("Equip title"))
		m_Type = ItemType::EquipTitle;
	else if(TypeSet.hasSet("Equip potion HP"))
		m_Type = ItemType::EquipPotionHeal;
	else if(TypeSet.hasSet("Equip potion MP"))
		m_Type = ItemType::EquipPotionMana;
	else if(TypeSet.hasSet("Single use x1"))
		m_Type = ItemType::UseSingle;
	else if(TypeSet.hasSet("Multiple use x99"))
		m_Type = ItemType::UseMultiple;
	else if(TypeSet.hasSet("Resource harvestable"))
		m_Type = ItemType::ResourceHarvestable;
	else if(TypeSet.hasSet("Resource mineable"))
		m_Type = ItemType::ResourceMineable;

	mystd::json::parse(m_Data, [this](nlohmann::json& pJson)
	{
		// try to initialize harversing
		if(const auto& pHarvestingJson = pJson["harvesting"]; !pHarvestingJson.is_null())
		{
			HarvestingContext Harvesting;
			Harvesting.Level = pHarvestingJson.value("level", 1);
			Harvesting.Health = pHarvestingJson.value("health", 100);
			m_HarvestingContext = Harvesting;
		}

		// try to initialize potion
		if(const auto& pPotionJson = pJson["potion"]; !pPotionJson.is_null())
		{
			PotionContext Potion;
			Potion.Effect = pPotionJson.value("effect", "");
			Potion.Value = pPotionJson.value("value", 0);
			Potion.Lifetime = pPotionJson.value("lifetime", 0);
			s_vTotalPotionByItemIDList[m_ID] = Potion;
			m_PotionContext = Potion;
		}

		// try to initialize random box
		for(auto& p : pJson["random_box"])
		{
			const auto ItemID = p.value("item_id", -1);
			const auto Value = p.value("value", 1);
			const auto Chance = p.value("chance", 100.0f);
			m_RandomBox.Add(ItemID, Value, Chance);
		}
	});
}

void CItemDescription::StartItemScenario(CPlayer* pPlayer, ItemScenarioEvent Event) const
{
	mystd::json::parse(m_Data, [&pPlayer, Event, this](nlohmann::json& pJson)
	{
		int ScenarioID = SCENARIO_UNIVERSAL;
		const char* pElem;
		switch(Event)
		{
			case ItemScenarioEvent::OnEventGot: 
				ScenarioID = SCENARIO_ON_ITEM_GOT;
				pElem = "on_event_got"; 
				break;
			case ItemScenarioEvent::OnEventLost:
				ScenarioID = SCENARIO_ON_ITEM_LOST;
				pElem = "on_event_lost"; 
				break;
			case ItemScenarioEvent::OnEventEquip:
				ScenarioID = SCENARIO_ON_ITEM_EQUIP;
				pElem = "on_event_equip"; 
				break;
			default: 
				ScenarioID = SCENARIO_ON_ITEM_UNEQUIP;
				pElem = "on_event_unequip"; 
				break;
		}

		// start scenario
		const auto& scenarioJsonData = pJson[pElem];
		pPlayer->Scenarios().Start(std::make_unique<CUniversalScenario>(ScenarioID, scenarioJsonData));
	});
}

bool CItemDescription::IsStackable() const
{
	return !(IsEnchantable() || IsGroup(ItemGroup::Settings) || IsGroup(ItemGroup::Equipment));
}

bool CItemDescription::IsEnchantable() const
{
	return !m_aAttributes.empty();
}

bool CItemDescription::IsEnchantMaxLevel(int Enchant) const
{
	for (const auto& Att : m_aAttributes)
	{
		if(Att.HasValue())
		{
			const int EnchantMax = Att.GetValue() + translate_to_percent_rest(Att.GetValue(), PERCENT_MAXIMUM_ENCHANT);
			if(GetInfoEnchantStats(Att.GetID(), Enchant) > EnchantMax)
				return true;
		}
	}
	return false;
}

bool CItemDescription::HasAttributes() const { return !m_aAttributes.empty(); }

std::string CItemDescription::GetStringAttributesInfo(CPlayer* pPlayer, int Enchant) const
{
	std::string strAttributes {};
	for(const auto& Att : m_aAttributes)
	{
		if(Att.HasValue())
		{
			const int BonusValue = GetInfoEnchantStats(Att.GetID(), Enchant);
			strAttributes += fmt_localize(pPlayer->GetCID(), "{}+{$} ", Att.Info()->GetName(), BonusValue);
		}
	}
	return strAttributes.empty() ? "unattributed" : strAttributes;
}

std::string CItemDescription::GetStringEnchantLevel(int Enchant) const
{
	if(Enchant > 0)
		return "[" + (IsEnchantMaxLevel(Enchant) ? "Max" : std::string("+" + std::to_string(Enchant))) + "]";
	return "\0";
}