/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SkillDataInfo.h"

std::map < int, CSkillDataInfo > CSkillDataInfo::ms_aSkillsData;

const char* CSkillDataInfo::GetControlEmoteStateName(int EmoticionID)
{
	switch(EmoticionID)
	{
		case EMOTICON_OOP: return "Emoticion Ooop";
		case EMOTICON_EXCLAMATION: return "Emoticion Exclamation";
		case EMOTICON_HEARTS: return "Emoticion Hearts";
		case EMOTICON_DROP: return "Emoticion Drop";
		case EMOTICON_DOTDOT: return "Emoticion ...";
		case EMOTICON_MUSIC: return "Emoticion Music";
		case EMOTICON_SORRY: return "Emoticion Sorry";
		case EMOTICON_GHOST: return "Emoticion Ghost";
		case EMOTICON_SUSHI: return "Emoticion Sushi";
		case EMOTICON_SPLATTEE: return "Emoticion Splatee";
		case EMOTICON_DEVILTEE: return "Emoticion Deviltee";
		case EMOTICON_ZOMG: return "Emoticion Zomg";
		case EMOTICON_ZZZ: return "Emoticion Zzz";
		case EMOTICON_WTF: return "Emoticion Wtf";
		case EMOTICON_EYES: return "Emoticion Eyes";
		case EMOTICON_QUESTION: return "Emoticion Question";
	}
	return "Not selected";
}