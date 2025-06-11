/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "item_info_data.h"
#include <game/server/gamecontext.h>

int CItemDescription::GetEnchantAttributeValue(AttributeIdentifier ID) const
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

int CItemDescription::GetEnchantAttributeValue(AttributeIdentifier ID, int Enchant) const
{
	const int StatSize = GetEnchantAttributeValue(ID);
	if(StatSize <= 0)
		return 0;

	const int PercentEnchant = translate_to_percent_rest(StatSize, PERCENT_OF_ENCHANT);
	int EnchantStat = StatSize + (PercentEnchant * Enchant);

	// the case when with percent will back 0
	if(PercentEnchant <= 0)
		EnchantStat += Enchant;

	return EnchantStat;
}

std::optional<float> CItemDescription::GetEnchantAttributeChance(AttributeIdentifier ID, int Enchant) const
{
	int totalValue = GetEnchantAttributeValue(ID, Enchant);
	if(!totalValue)
		return std::nullopt;

	return CAttributeDescription::CalculateChance(ID, totalValue);
}

int CItemDescription::GetEnchantPrice(int EnchantLevel) const
{
	int FinishedPrice = 0;
	for (const auto& Att : m_aAttributes)
	{
		if(Att.HasValue())
		{
			const float UpgradePriceInfluence = powf((float)maximum(1, Att.Info()->GetUpgradePrice() + 1), (float)(g_Config.m_SvEnchantUpgradeInfluence) / 100.f);
			const float LevelInfluence = powf((float)maximum(1, EnchantLevel + 1), (float)(g_Config.m_SvEnchantIncreaseInfluence) / 100.f);
			const float BaseAttributeCost = ((float)g_Config.m_SvEnchantPriceFactor * UpgradePriceInfluence);
			FinishedPrice += round_to_int(BaseAttributeCost * Att.GetValue() * LevelInfluence);
		}
	}
	return FinishedPrice;
}

void CItemDescription::InitData(const DBSet& GroupSet, const DBSet& TypeSet)
{
	// initialize group and type
	const auto Group = GetItemGroupFromDBSet(GroupSet);
	const auto Type = GetItemTypeFromDBSet(TypeSet);
	if(Group == ItemGroup::Unknown || Type == ItemType::Unknown)
	{
		dbg_assert(false, "item group or type initialization error by (dbset initilization)");
		return;
	}

	m_Group = Group;
	m_Type = Type;

	// load data context for item
	mystd::json::parse(m_Data, [this](nlohmann::json& pJson)
	{
		// try to initialize potion
		if(const auto& pPotionJson = pJson["potion"]; !pPotionJson.is_null())
		{
			PotionContext Potion;
			Potion.Effect = pPotionJson.value("effect", "");
			Potion.Value = pPotionJson.value("value", 0);
			Potion.Lifetime = pPotionJson.value("lifetime", 0);
			Potion.Recasttime = pPotionJson.value("recast", POTION_RECAST_DEFAULT_TIME);
			s_vTotalPotionByItemIDList[m_ID] = Potion;
			m_PotionContext = Potion;
		}

		// try to initialize apply bonuses item
		if(const auto& pBonusJson = pJson["bonus"]; !pBonusJson.is_null())
		{
			BonusesContext Bonus;
			Bonus.Amount = pBonusJson.value("amount", 0.f);
			Bonus.DurationDays = pBonusJson.value("duration_days", 0);
			Bonus.DurationHours = pBonusJson.value("duration_hours", 0);
			Bonus.DurationMinutes = pBonusJson.value("duration_minutes", 0);
			Bonus.Type = pBonusJson.value("type", 1);
			m_BonusContext = Bonus;
		}

		// try to initialize random box
		if(const auto& pRandomBoxJson = pJson["random_box"]; !pRandomBoxJson.is_null())
		{
			for(auto& p : pRandomBoxJson)
			{
				const auto ItemID = p.value("item_id", -1);
				const auto Value = p.value("value", 1);
				const auto Chance = p.value("chance", 100.0f);
				m_RandomBox.Add(ItemID, Value, Chance);
			}
			m_RandomBox.NormalizeChances();
		}
	});
}

void CItemDescription::InitUniqueName(const std::string& Name)
{
	// prefix
	std::string_view Prefix {};
	switch(m_Type)
	{
		case ItemType::EquipArmorTank:
		case ItemType::EquipHelmetTank:
			Prefix = "T";
			break;

		case ItemType::EquipArmorDPS:
		case ItemType::EquipHelmetDPS:
			Prefix = "D";
			break;

		case ItemType::EquipArmorHealer:
		case ItemType::EquipHelmetHealer:
			Prefix = "H";
			break;

		case ItemType::EquipHammer:
		case ItemType::EquipGun:
		case ItemType::EquipShotgun:
		case ItemType::EquipGrenade:
		case ItemType::EquipLaser:
			Prefix = "W";
			break;

		case ItemType::EquipPickaxe:
		case ItemType::EquipFishrod:
		case ItemType::EquipGloves:
		case ItemType::EquipRake:
			Prefix = "J";
			break;

		default: break;
	}

	if(m_Group == ItemGroup::Equipment && m_Type == ItemType::Default)
		Prefix = "M";

	// prepare result
	std::string Result {};
	if(Prefix.empty())
		Result = Name;
	else
		Result = fmt_default("{}: {}", Prefix, Name);
	str_copy(m_aName, Result.c_str(), sizeof(m_aName));
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
			if(GetEnchantAttributeValue(Att.GetID(), Enchant) > EnchantMax)
				return true;
		}
	}
	return false;
}

bool CItemDescription::HasAttributes() const
{
	return !m_aAttributes.empty();
}

int CItemDescription::GetTotalAttributesLevel(int Enchant) const
{
	if(m_aAttributes.empty())
		return 0;

	int Total = 0;
	for(auto& Attribute : m_aAttributes)
	{
		const int Value = GetEnchantAttributeValue(Attribute.GetID(), Enchant);
		Total += (Value * Attribute.Info()->GetUpgradePrice());
	}

	return Total;
}

std::string CItemDescription::GetStringAttributesInfo(CPlayer* pPlayer, int Enchant) const
{
	std::string strAttributes {};
	for(const auto& Att : m_aAttributes)
	{
		if(Att.HasValue())
		{
			if(auto chanceOpt = GetEnchantAttributeChance(Att.GetID(), Enchant))
			{
				strAttributes += fmt_localize(pPlayer->GetCID(), "{}+{~.2}% ", Att.Info()->GetName(), (*chanceOpt));
			}
			else
			{
				const int BonusValue = GetEnchantAttributeValue(Att.GetID(), Enchant);
				strAttributes += fmt_localize(pPlayer->GetCID(), "{}+{$} ", Att.Info()->GetName(), BonusValue);
			}
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