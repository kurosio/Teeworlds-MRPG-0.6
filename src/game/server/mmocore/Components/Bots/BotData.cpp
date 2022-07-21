/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotData.h"

std::map< int, DataBotInfo > DataBotInfo::ms_aDataBot;
std::map< int, NpcBotInfo > NpcBotInfo::ms_aNpcBot;
std::map< int, QuestBotInfo > QuestBotInfo::ms_aQuestBot;
std::map< int, MobBotInfo > MobBotInfo::ms_aMobBot;

void DialogData::LoadFlags()
{
	m_Flag = TALKED_FLAG_FULL;

	// flag for selected who talked
	if(str_comp_nocase_num(m_aText, "[p]", 3) == 0)
	{
		m_Flag |= TALKED_FLAG_SAYS_PLAYER;
		mem_move(m_aText, m_aText + 3, sizeof(m_aText));
	}
	else
		m_Flag |= TALKED_FLAG_SAYS_BOT;

	// flag for selected render
	if(str_comp_nocase_num(m_aText, "[e]", 3) == 0)
	{
		m_Flag ^= TALKED_FLAG_FULL;
		mem_move(m_aText, m_aText + 3, sizeof(m_aText));
	}
	else if(str_comp_nocase_num(m_aText, "[e1]", 4) == 0)
	{
		m_Flag ^= TALKED_FLAG_PLAYER;
		mem_move(m_aText, m_aText + 4, sizeof(m_aText));
	}
	else if(str_comp_nocase_num(m_aText, "[e2]", 4) == 0)
	{
		m_Flag ^= TALKED_FLAG_BOT;
		mem_move(m_aText, m_aText + 4, sizeof(m_aText));
	}
}
