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
	if(m_Class == ClassGroup::DPS)
		return 0.f;
	return 0.f;
}

int CClassData::GetExtraDMG() const
{
	if(m_Class == ClassGroup::Tank)
		return 0.f;
	if(m_Class == ClassGroup::Healer)
		return 10.f;
	if(m_Class == ClassGroup::DPS)
		return 20.f;
	return 0;
}