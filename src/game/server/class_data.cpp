/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "class_data.h"

void CClassData::SetProfessionID(Professions ProfID)
{
	m_ProfessionID = ProfID;
}

float CClassData::GetExtraHP() const
{
	if(m_ProfessionID == Professions::Tank)
		return 30.f;
	if(m_ProfessionID == Professions::Healer)
		return 15.f;
	if(m_ProfessionID == Professions::Dps)
		return 5.f;
	return 0.f;
}

float CClassData::GetExtraMP() const
{
	if(m_ProfessionID == Professions::Tank)
		return 5.f;
	if(m_ProfessionID == Professions::Healer)
		return 30.f;
	if(m_ProfessionID == Professions::Dps)
		return 15.f;
	return 0;
}

float CClassData::GetExtraDMG() const
{
	if(m_ProfessionID == Professions::Tank)
		return 10.f;
	if(m_ProfessionID == Professions::Healer)
		return 5.f;
	if(m_ProfessionID == Professions::Dps)
		return 30.f;
	return 0;
}

void CClassData::SetProfessionSkin(CTeeInfo& TeeInfo, bool HasCustomizer) const
{
	if(HasCustomizer)
		return;

	if(m_ProfessionID == Professions::Tank)
	{
		str_copy(TeeInfo.m_aSkinName, "red_panda", sizeof(TeeInfo.m_aSkinName));
	}
	else if(m_ProfessionID == Professions::Healer)
	{
		str_copy(TeeInfo.m_aSkinName, "Empieza", sizeof(TeeInfo.m_aSkinName));
	}
	else if(m_ProfessionID == Professions::Dps)
	{
		str_copy(TeeInfo.m_aSkinName, "flokes", sizeof(TeeInfo.m_aSkinName));
	}
	TeeInfo.m_UseCustomColor = 0;
}