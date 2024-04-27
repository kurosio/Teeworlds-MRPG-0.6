/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "class_data.h"

void CClassData::Init(ClassGroup Class)
{
	m_Class = Class;
}

float CClassData::GetExtraHP() const
{
	if(m_Class == ClassGroup::Tank)
		return 30.f;
	if(m_Class == ClassGroup::Healer)
		return 15.f;
	if(m_Class == ClassGroup::Dps)
		return 5.f;
	return 0.f;
}

float CClassData::GetExtraMP() const
{
	if(m_Class == ClassGroup::Tank)
		return 5.f;
	if(m_Class == ClassGroup::Healer)
		return 30.f;
	if(m_Class == ClassGroup::Dps)
		return 15.f;
	return 0;
}

void CClassData::SetClassSkin(CTeeInfo& TeeInfo, bool HasCustomizer) const
{
	if(HasCustomizer)
		return;

	if(m_Class == ClassGroup::Tank)
		str_copy(TeeInfo.m_aSkinName, "red_panda", sizeof(TeeInfo.m_aSkinName));
	else if(m_Class == ClassGroup::Healer)
		str_copy(TeeInfo.m_aSkinName, "Empieza", sizeof(TeeInfo.m_aSkinName));
	else if(m_Class == ClassGroup::Dps)
		str_copy(TeeInfo.m_aSkinName, "flokes", sizeof(TeeInfo.m_aSkinName));
	TeeInfo.m_UseCustomColor = 0;
}

float CClassData::GetExtraDMG() const
{
	if(m_Class == ClassGroup::Tank)
		return 10.f;
	if(m_Class == ClassGroup::Healer)
		return 5.f;
	if(m_Class == ClassGroup::Dps)
		return 30.f;
	return 0;
}