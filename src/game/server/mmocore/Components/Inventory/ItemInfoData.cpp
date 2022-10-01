/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemInfoData.h"

#include <game/server/gamecontext.h>

std::map < int, CItemDataInfo > CItemDataInfo::ms_aItemsInfo;

int CItemDataInfo::GetInfoEnchantStats(Attribute ID) const
{
	for(int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		if(m_aAttribute[i] >= Attribute::SpreadShotgun && m_aAttribute[i] < Attribute::ATTRIBUTES_NUM && m_aAttribute[i] == ID)
			return m_aAttributeValue[i];
	}
	return 0;
}

int CItemDataInfo::GetInfoEnchantStats(Attribute ID, int Enchant) const
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
	for(int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		if(CGS::ms_aAttributesInfo.find(m_aAttribute[i]) == CGS::ms_aAttributesInfo.end())
			continue;

		int UpgradePrice;
		const Attribute Attribute = m_aAttribute[i];
		const AttributeType Type = CGS::ms_aAttributesInfo[Attribute].GetType();

		// strength stats
		if(Type == AttributeType::Hardtype)
			UpgradePrice = max(20, CGS::ms_aAttributesInfo[Attribute].GetUpgradePrice()) * 15;

		// weapon and job stats
		else if(Type == AttributeType::Job || Type == AttributeType::Weapon || Attribute == Attribute::LuckyDropItem)
			UpgradePrice = max(40, CGS::ms_aAttributesInfo[Attribute].GetUpgradePrice()) * 15;

		// other stats
		else
			UpgradePrice = max(5, CGS::ms_aAttributesInfo[Attribute].GetUpgradePrice()) * 15;

		const int PercentEnchant = max(1, translate_to_percent_rest(m_aAttributeValue[i], PERCENT_OF_ENCHANT));
		FinishedPrice += UpgradePrice * (PercentEnchant * (1 + EnchantLevel));
	}
	return FinishedPrice;
}

bool CItemDataInfo::IsEnchantable() const
{
	for(int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		if(CGS::ms_aAttributesInfo.find(m_aAttribute[i]) != CGS::ms_aAttributesInfo.end() && m_aAttributeValue[i] > 0)
			return true;
	}
	return false;
}

bool CItemDataInfo::IsEnchantMaxLevel(int Enchant) const
{
	for(int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		if(CGS::ms_aAttributesInfo.find(m_aAttribute[i]) != CGS::ms_aAttributesInfo.end() && m_aAttributeValue[i] > 0)
		{
			const int EnchantMax = m_aAttributeValue[i] + translate_to_percent_rest(m_aAttributeValue[i], PERCENT_MAXIMUM_ENCHANT);
			if(GetInfoEnchantStats(m_aAttribute[i], Enchant) > EnchantMax)
				return true;
		}
	}
	return false;
}

void CItemDataInfo::FormatAttributes(CPlayer* pPlayer, char* pBuffer, int Size, int Enchant) const
{
	dynamic_string Buffer;
	for(int i = 0; i < STATS_MAX_FOR_ITEM; i++)
	{
		if(CGS::ms_aAttributesInfo.find(m_aAttribute[i]) != CGS::ms_aAttributesInfo.end() && m_aAttributeValue[i] > 0)
		{
			const int BonusValue = GetInfoEnchantStats(m_aAttribute[i], Enchant);
			pPlayer->GS()->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}+{VAL} ", pPlayer->GS()->GetAttributeInfo(m_aAttribute[i])->GetName(), BonusValue);
		}
	}
	str_copy(pBuffer, Buffer.buffer(), Size);
	Buffer.clear();
}

void CItemDataInfo::FormatEnchantLevel(char* pBuffer, int Size, int Enchant) const
{
	if(Enchant > 0)
	{
		str_format(pBuffer, Size, "[%s]", IsEnchantMaxLevel(Enchant) ? "Max" : std::string("+" + std::to_string(Enchant)).c_str());
		return;
	}
	str_copy(pBuffer, "\0", Size);
}