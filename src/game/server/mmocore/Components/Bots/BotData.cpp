/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotData.h"

std::map< int, DataBotInfo > DataBotInfo::ms_aDataBot;
std::map< int, NpcBotInfo > NpcBotInfo::ms_aNpcBot;
std::map< int, QuestBotInfo > QuestBotInfo::ms_aQuestBot;
std::map< int, MobBotInfo > MobBotInfo::ms_aMobBot;

void CDialog::Init(int BotID, std::string DialogueData, int Emote, bool ActionStep)
{
	auto VariantInit = [BotID](VariantText* pVariant, std::string Text)
	{
		int64_t size = -1;

		const char* pBot = str_find_nocase(Text.c_str(), "[bot_");
		if(int SearchBotID = 0; pBot != nullptr && sscanf(pBot, "[bot_%d]", &SearchBotID) && DataBotInfo::IsDataBotValid(SearchBotID))
		{
			char aBufSearch[16];
			str_format(aBufSearch, sizeof(aBufSearch), "[bot_%d]", SearchBotID);
			Text.erase(0, str_length(aBufSearch));

			pVariant->Init(DataBotInfo::ms_aDataBot[SearchBotID].m_aNameBot);
			pVariant->m_Flag |= TALKED_FLAG_SAYS_BOT;
		}
		else if(str_find_nocase(Text.c_str(), "[p]") != nullptr)
		{
			Text.erase(0, size + 4);

			pVariant->Init("" /* dynamic data */);
			pVariant->m_Flag |= TALKED_FLAG_SAYS_PLAYER;
		}
		else if(str_find_nocase(Text.c_str(), "[eidolon]") != nullptr)
		{
			Text.erase(0, size + 10);

			pVariant->Init("" /* dynamic data */);
			pVariant->m_Flag |= TALKED_FLAG_SAYS_EIDOLON;
		}
		else
		{
			pVariant->Init(DataBotInfo::ms_aDataBot[BotID].m_aNameBot);
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

void MobBotInfo::InitBuffDebuff(int Seconds, int Range, float Chance, std::string& buffSets)
{
	if(!buffSets.empty())
	{
		size_t start;
		size_t end = 0;
		std::string delim = ",";

		while((start = buffSets.find_first_not_of(delim, end)) != std::string::npos)
		{
			end = buffSets.find(delim, start);
			m_Effects.push_back({ Chance, buffSets.substr(start, end - start), { Seconds, Range } });
		}
	}
}
