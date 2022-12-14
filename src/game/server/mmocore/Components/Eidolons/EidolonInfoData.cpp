/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "EidolonInfoData.h"

#include "game/server/mmocore/Components/Bots/BotData.h"
#include "game/server/mmocore/Components/Inventory/ItemInfoData.h"

CEidolonInfoData::EidolonDescriptionList CEidolonInfoData::m_EidolonsInfoData =
{
	{
		itEidolonOtohime, 45,
		{
			"The shadow of the ocean. After discovering",
			"the land by accident, she follows the you",
			"of to explore a world that is completely new to her."
		}
	},
	{
		itEidolonMerrilee, 46,
		{
			"A sociable spirit, she loves to protect people.",
			"While it's a bit vain, she takes great pride",
			"in her beautiful hair, and uses a small amount",
			"of magic to keep it perfectly styled."
		}
	},
	{
		itEidolonDryad, 58,
		{
			"Dryads are green dryads with sea-green vines.",
			"They have violet-colored flowers for hands and beady eyes."
		}
	}
};

DataBotInfo* CEidolonInfoData::GetDataBot() const
{
	if(DataBotInfo::ms_aDataBot.find(m_DataBotID) != DataBotInfo::ms_aDataBot.end())
		return &DataBotInfo::ms_aDataBot[m_DataBotID];
	return nullptr;
}

CItemDescription* CEidolonInfoData::GetItem() const
{
	if(CItemDescription::Data().find(m_ItemID) != CItemDescription::Data().end())
		return &CItemDescription::Data()[m_ItemID];
	return nullptr;
}
