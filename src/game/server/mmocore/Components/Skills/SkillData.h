/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_DATA_H
#define GAME_SERVER_COMPONENT_SKILL_DATA_H
#include "SkillDataInfo.h"

class CSkillData
{
	friend class CSkillsCore;

	int m_Level;

	class CGS* m_pGS;
	class CPlayer* m_pPlayer;
	CGS* GS() const { return m_pGS; }

public:
	int m_SkillID;
	int m_SelectedEmoticion;

	int GetID() const { return m_SkillID; }
	int GetBonus() const { return m_Level * Info()->GetBonusDefault(); }
	bool IsLearned() const { return m_Level > 0; }
	int GetLevel() const { return m_Level; }
	const char* GetControlEmoteStateName() const { return Info()->GetControlEmoteStateName(m_SelectedEmoticion); }

	void SelectNextControlEmote();
	bool Upgrade();
	bool Use();

	void SetSkillOwner(CPlayer* pPlayer);
	CSkillDataInfo* Info() const { return &CSkillDataInfo::ms_aSkillsData[m_SkillID]; };

	static std::map< int, std::map < int, CSkillData > > ms_aSkills;
};

#endif
