/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotData.h"

std::map< int, DataBotInfo > DataBotInfo::ms_aDataBot;
std::map< int, NpcBotInfo > NpcBotInfo::ms_aNpcBot;
std::map< int, QuestBotInfo > QuestBotInfo::ms_aQuestBot;
std::map< int, MobBotInfo > MobBotInfo::ms_aMobBot;

void CDialog::Init(std::string DialogueData, int Emote, bool ActionStep)
{
	auto VariantInit = [](VariantText* pVariant, std::string Text)
	{
		int64_t size = -1;
		if(size = Text.find("[p]"); size != -1)
		{
			pVariant->m_Flag |= TALKED_FLAG_SAYS_PLAYER;
			Text.erase(0, size + 3);
		}
		else
		{
			pVariant->m_Flag |= TALKED_FLAG_SAYS_BOT;
		}

		pVariant->m_Text = Text;
	};

	size_t pos = 0;
	std::string delimiter = "{OR}";
	while((pos = DialogueData.find(delimiter)) != std::string::npos)
	{
		CDialog::VariantText Variant;
		VariantInit(&Variant, DialogueData.substr(0, pos));
		m_aVariantText.push_back(Variant);
		DialogueData.erase(0, pos + delimiter.length());
	}

	if(!DialogueData.empty() || IsEmptyDialog())
	{
		CDialog::VariantText Variant;
		VariantInit(&Variant, DialogueData);
		m_aVariantText.push_back(Variant);
	}

	m_Emote = Emote;
	m_ActionStep = ActionStep;
}

CDialog::VariantText* CDialog::GetVariant()
{
	if(m_aVariantText.size() > 1)
	{
		int random = random_int() % (int)m_aVariantText.size();
		return &m_aVariantText[random];
	}

	return &m_aVariantText.front();
}
