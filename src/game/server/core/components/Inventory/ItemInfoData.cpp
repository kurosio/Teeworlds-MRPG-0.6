/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemInfoData.h"

#include <game/server/gamecontext.h>

int CItemDescription::GetInfoEnchantStats(AttributeIdentifier ID) const
{
	for (const auto& Att : m_aAttributes)
	{
		AttributeIdentifier SearchID = Att.GetID();
		if(SearchID >= AttributeIdentifier::SpreadShotgun && SearchID < AttributeIdentifier::ATTRIBUTES_NUM && SearchID == ID)
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

void CItemDescription::RunEvent(CPlayer* pPlayer, int EventID) const
{
	Utils::Json::parseFromString(m_Data, [&pPlayer, EventID, this](nlohmann::json& pJson)
	{
		const char* pElem;
		switch(EventID)
		{
			case OnEventGot: pElem = "on_event_got"; break;
			case OnEventLost: pElem = "on_event_lost"; break;
			case OnEventEquip: pElem = "on_event_equip"; break;
			default: pElem = "on_event_unequip"; break;
		}

		CGS* pGS = pPlayer->GS();
		const auto& pEventObjectJson = pJson[pElem];
		const int ClientID = pPlayer->GetCID();

		// messages array
		if(pEventObjectJson.contains("messages"))
		{
			const auto& pChatArrayJson = pEventObjectJson["messages"];

			std::string Text;
			for(const auto& p : pChatArrayJson)
			{
				Text = p.value("text", "\0");

				// broadcast type
				if(p.contains("broadcast") && p.value("broadcast", false))
					pGS->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 300, Text.c_str());
				else // chat type
					pGS->Chat(ClientID, Text.c_str());
			}
		}
	});
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

void CItemDescription::StrFormatAttributes(CPlayer* pPlayer, char* pBuffer, int Size, int Enchant) const
{
	std::string strAttrbutes{};
	for (const auto& Att : m_aAttributes)
	{
		if(Att.HasValue())
		{
			const int BonusValue = GetInfoEnchantStats(Att.GetID(), Enchant);
			strAttrbutes += fmt_handle_def(pPlayer->GetCID(), "{}+{}", Att.Info()->GetName(), BonusValue);
		}
	}
	str_copy(pBuffer, strAttrbutes.c_str(), Size);
}

std::string CItemDescription::StringEnchantLevel(int Enchant) const
{
	if(Enchant > 0)
		return "[" + (IsEnchantMaxLevel(Enchant) ? "Max" : std::string("+" + std::to_string(Enchant))) + "]";
	return "\0";
}