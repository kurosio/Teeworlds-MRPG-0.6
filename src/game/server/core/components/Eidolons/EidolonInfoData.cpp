/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "EidolonInfoData.h"

#include "game/server/core/components/Bots/BotData.h"
#include "game/server/core/components/inventory/item_info_data.h"

CEidolonInfoData::EidolonDescriptionList CEidolonInfoData::m_EidolonsInfoData =
{
	{
		itEidolonOtohime, 2,
		{
			"The shadow of the ocean. After discovering",
			"the land by accident, she follows the you",
			"of to explore a world that is completely new to her."
		}
	},
	{
		itEidolonMerrilee, 5,
		{
			"A sociable spirit, she loves to protect people.",
			"While it's a bit vain, she takes great pride",
			"in her beautiful hair, and uses a small amount",
			"of magic to keep it perfectly styled."
		}
	},
	{
		itEidolonDryad, 3,
		{
			"Dryads are green dryads with sea-green vines.",
			"They have violet-colored flowers for hands and beady eyes.",
			"Trained by scaffolding, to use some weapons, and boosts."
		}
	},
	{
		itEidolonPigQueen, 4,
		{
			"Perhaps the self-proclaimed queen of pigs.",
			"It has a delicate pink color, likes to eat everything green.",
			"Has a strong attack, but cannot use anything."
		}
	}
};

DataBotInfo* CEidolonInfoData::GetDataBot() const
{
	auto it = DataBotInfo::ms_aDataBot.find(m_DataBotID);
	return (it != DataBotInfo::ms_aDataBot.end()) ? &it->second : nullptr;
}

CItemDescription* CEidolonInfoData::GetItem() const
{
	auto it = CItemDescription::Data().find(m_ItemID);
	return (it != CItemDescription::Data().end()) ? &it->second : nullptr;
}