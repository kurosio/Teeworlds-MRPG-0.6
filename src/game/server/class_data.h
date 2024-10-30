/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_CLASS_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_CLASS_DATA_H

class CClassData
{
	int m_ClientID{};
	Professions m_ProfessionID{};

public:
	CClassData()
	{
		m_ProfessionID = Professions::None;
	}

	void Init(int ClientID, Professions ProfID)
	{
		m_ClientID = ClientID;
		SetProfessionID(ProfID);
	}

	void SetProfessionID(Professions ProfID);

	bool HasProfession() const
	{
		return m_ProfessionID != Professions::None;
	}

	Professions GetProfessionID() const
	{
		return m_ProfessionID;
	}

	bool IsProfession(Professions ProfID) const
	{
		return m_ProfessionID == ProfID;
	}

	const char* GetName() const
	{
		return GetProfessionName(m_ProfessionID);
	};

	float GetExtraHP() const;
	float GetExtraDMG() const;
	float GetExtraMP() const;

	void UpdateProfessionSkin() const;
};

#endif