/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemInfoData.h"

#include <game/server/gamecontext.h>

std::map < int, CItemDataInfo > CItemDataInfo::ms_aItemsInfo;

int CItemDataInfo::GetInfoEnchantStats(AttributeIdentifier ID) const
{
	for (const auto& Att : m_aAttribute)
	{
		AttributeIdentifier SearchID = Att.GetID();
		if(SearchID >= AttributeIdentifier::SpreadShotgun && SearchID < AttributeIdentifier::ATTRIBUTES_NUM && SearchID == ID)
			return Att.GetValue();
	}
	return 0;
}

int CItemDataInfo::GetInfoEnchantStats(AttributeIdentifier ID, int Enchant) const
{
	const int StatSize = GetInfoEnchantStats(ID);
	if(StatSize <= 0)
		return 0;

	const int PercentEnchant = translate_to_percent_rest(StatSize, PERCENT_OF_ENCHANT);
	int EnchantStat = StatSize + PercentEnchant * (1 + Enchant);

	// the case when with percent will back 0
	if(PercentEnchant <= 0)
		EnchantStat += Enchant;

	return EnchantStat;
}

int CItemDataInfo::GetEnchantPrice(int EnchantLevel) const
{
	int FinishedPrice = 0;
	for (const auto& Att : m_aAttribute)
	{
		if(Att.HasValue())
		{
			int UpgradePrice;
			AttributeType Type = Att.Info()->GetType();

			// strength stats
			if(Type == AttributeType::Hardtype)
				UpgradePrice = max(20, Att.Info()->GetUpgradePrice()) * 15;

			// weapon and job stats
			else if(Type == AttributeType::Job || Type == AttributeType::Weapon || Att.GetID() == AttributeIdentifier::LuckyDropItem)
				UpgradePrice = max(40, Att.Info()->GetUpgradePrice()) * 15;

			// other stats
			else
				UpgradePrice = max(5, Att.Info()->GetUpgradePrice()) * 15;

			const int PercentEnchant = max(1, translate_to_percent_rest(Att.GetValue(), PERCENT_OF_ENCHANT));
			FinishedPrice += UpgradePrice * (PercentEnchant * (1 + EnchantLevel));
		}
	}
	return FinishedPrice;
}

bool CItemDataInfo::IsEnchantable() const
{
	return std::any_of(std::begin(m_aAttribute), std::end(m_aAttribute), [](const CAttribute &p){return p.GetValue() > 0; });
}

bool CItemDataInfo::IsEnchantMaxLevel(int Enchant) const
{
	for (const auto& Att : m_aAttribute)
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

void CItemDataInfo::StrFormatAttributes(CPlayer* pPlayer, char* pBuffer, int Size, int Enchant) const
{
	dynamic_string Buffer;
	for (const auto& Att : m_aAttribute)
	{
		if(Att.HasValue())
		{
			const int BonusValue = GetInfoEnchantStats(Att.GetID(), Enchant);
			pPlayer->GS()->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}+{VAL} ", Att.Info()->GetName(), BonusValue);
		}
	}
	str_copy(pBuffer, Buffer.buffer(), Size);
	Buffer.clear();
}

void CItemDataInfo::StrFormatEnchantLevel(char* pBuffer, int Size, int Enchant) const
{
	if(Enchant > 0)
	{
		str_format(pBuffer, Size, "[%s]", IsEnchantMaxLevel(Enchant) ? "Max" : std::string("+" + std::to_string(Enchant)).c_str());
		return;
	}
	str_copy(pBuffer, "\0", Size);
}