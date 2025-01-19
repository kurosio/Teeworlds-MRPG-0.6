/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_CLASS_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_CLASS_DATA_H

class ClassData
{
	int m_ClientID{};
	ProfessionIdentifier m_ProfessionID{};

public:
	ClassData()
	{
		m_ProfessionID = ProfessionIdentifier::None;
	}

	void Init(int ClientID, ProfessionIdentifier ProfID)
	{
		m_ClientID = ClientID;
		SetProfessionID(ProfID);
	}

	void SetProfessionID(ProfessionIdentifier ProfID);

	bool HasProfession() const
	{
		return m_ProfessionID != ProfessionIdentifier::None;
	}

	ProfessionIdentifier GetProfessionID() const
	{
		return m_ProfessionID;
	}

	bool IsProfession(ProfessionIdentifier ProfID) const
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