/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemInfoData.h"
#include <game/server/gamecontext.h>

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
			if(Type == AttributeGroup::DamageType)
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
			s_vTotalPotionByItemIDList[m_ID] = Potion;
			m_PotionContext = Potion;
		}

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
		for(auto& p : pJson["random_box"])
		{
			const auto ItemID = p.value("item_id", -1);
			const auto Value = p.value("value", 1);
			const auto Chance = p.value("chance", 100.0f);
			m_RandomBox.Add(ItemID, Value, Chance);
		}
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