#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_PROFESSION_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_PROFESSION_H

class CGS;
class CPlayer;

enum EProfessionType
{
	PROFESSION_TYPE_WAR = 0,
	PROFESSION_TYPE_OTHER = 1,
};

class CProfession
{
	int m_Level {};
	uint64_t m_Experience {};
	int m_UpgradePoint {};
	int m_ClientID {};
	Professions m_ProfessionID {};
	int m_ProfessionType {};

protected:
	CGS* GS() const;
	CPlayer* GetPlayer() const;
	std::unordered_map<AttributeIdentifier, int> m_Attributes;

public:
	CProfession(Professions Profession, int ProfessionType);

	void Init(int ClientID, const std::optional<std::string>& jsonData);
	void Save();

	void AddExperience(uint64_t Experience);
	bool Upgrade(AttributeIdentifier ID, int Units, int PricePerUnit);

	int GetLevel() const
	{
		return m_Level;
	}

	uint64_t GetExperience() const
	{
		return m_Experience;
	}

	uint64_t GetExpForNextLevel() const
	{
		return computeExperience(m_Level);
	}

	int GetUpgradePoint() const
	{
		return m_UpgradePoint;
	}

	Professions GetProfessionID() const
	{
		return m_ProfessionID;
	}

	int GetProfessionType() const
	{
		return m_ProfessionType;
	}

	const std::unordered_map<AttributeIdentifier, int>& GetAttributes() const
	{
		return m_Attributes;
	}

	int GetAttributeValue(AttributeIdentifier ID) const
	{
		return m_Attributes.contains(ID) ? m_Attributes.at(ID) : 0;
	}

	bool HasAttributes() const
	{
		return !m_Attributes.empty();
	}

private:
	std::string GetPreparedJsonString() const;
};

class CTankProfession : public CProfession
{
public:
	CTankProfession() : CProfession(Professions::Tank, PROFESSION_TYPE_WAR)
	{
		m_Attributes[AttributeIdentifier::HP] = 0;
		m_Attributes[AttributeIdentifier::Lucky] = 0;
	}
};

class CDPSProfession : public CProfession
{
public:
	CDPSProfession() : CProfession(Professions::Dps, PROFESSION_TYPE_WAR)
	{
		m_Attributes[AttributeIdentifier::Crit] = 0;
		m_Attributes[AttributeIdentifier::CritDMG] = 0;
		m_Attributes[AttributeIdentifier::AttackSPD] = 0;
	}
};

class CHealerProfession : public CProfession
{
public:
	CHealerProfession() : CProfession(Professions::Healer, PROFESSION_TYPE_WAR)
	{
		m_Attributes[AttributeIdentifier::MP] = 0;
		m_Attributes[AttributeIdentifier::Vampirism] = 0;
	}
};

class CFarmerProfession : public CProfession
{
public:
	CFarmerProfession() : CProfession(Professions::Farmer, PROFESSION_TYPE_OTHER)
	{
		m_Attributes[AttributeIdentifier::Extraction] = 1;
	}
};

class CMinerProfession : public CProfession
{
public:
	CMinerProfession() : CProfession(Professions::Miner, PROFESSION_TYPE_OTHER)
	{
		m_Attributes[AttributeIdentifier::Efficiency] = 1;
	}
};

#endif