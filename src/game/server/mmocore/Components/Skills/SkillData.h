/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_DATA_H
#define GAME_SERVER_COMPONENT_SKILL_DATA_H

#include "SkillDataInfo.h"

class CSkill : public MultiworldIdentifiableStaticData< std::map< int, std::map < int, CSkill > > >
{
	friend class CSkillsCore;

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

	SkillIdentifier m_ID{};
	int m_ClientID{};
	int m_Level{};
	int m_SelectedEmoticion{};

public:
	CSkill() = default;
	CSkill(SkillIdentifier ID, int ClientID) : m_ID(ID), m_ClientID(ClientID) { }
	
	void Init(int Level, int SelectedEmoticion)
	{
		m_Level = Level;
		m_SelectedEmoticion = SelectedEmoticion;
		CSkill::m_pData[m_ClientID][m_ID] = *this;
	}

	void SetID(SkillIdentifier ID) { m_ID = ID; }
	SkillIdentifier GetID() const { return m_ID; }

	bool IsLearned() const { return m_Level > 0; }
	int GetLevel() const { return m_Level; }
	int GetBonus() const { return m_Level * Info()->GetBonusDefault(); }
	const char* GetSelectedEmoticonName() const { return Info()->GetEmoticonName(m_SelectedEmoticion); }

	void SelectNextControlEmote();
	bool Upgrade();
	bool Use();

	CSkillDescription* Info() const { return &CSkillDescription::Data()[m_ID]; }
};

#endif
