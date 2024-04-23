/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_CLASS_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_CLASS_DATA_H

class CClassData
{
	ClassGroup m_Class {};

public:
	CClassData() {};

	void Init(ClassGroup Class);

	ClassGroup GetGroup() const { return m_Class; }
	float GetExtraHP() const;
	float GetExtraDMG() const;
	float GetExtraMP() const;
};

#endif