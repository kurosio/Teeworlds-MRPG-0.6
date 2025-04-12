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
	int m_ClientID {};
	int m_Level {};
	uint64_t m_Experience {};
	int m_UpgradePoint {};
	ProfessionIdentifier m_ProfessionID {};
	int m_ProfessionType {};

protected:
	CGS* GS() const;
	CPlayer* GetPlayer() const;
	std::unordered_map<AttributeIdentifier, int> m_Attributes {};
	std::unordered_map<AttributeIdentifier, float> m_ExtraBoost {};
	CTeeInfo m_ProfessionSkin {};

public:
	CProfession(ProfessionIdentifier ProfID, int ProfessionType);

	void Init(int ClientID, const std::optional<std::string>& jsonData);
	void Save();

	void AddExperience(uint64_t Experience);
	bool Upgrade(AttributeIdentifier ID, int Units, int PricePerUnit);

	int GetLevel() const { return m_Level; }
	uint64_t GetExperience() const { return m_Experience; }
	uint64_t GetExpForNextLevel() const { return computeExperience(m_Level); }
	int GetUpgradePoint() const { return m_UpgradePoint; }
	ProfessionIdentifier GetProfessionID() const { return m_ProfessionID; }
	bool IsProfessionType(int ProfessionType) const { return m_ProfessionType == ProfessionType; }
	const std::unordered_map<AttributeIdentifier, int>& GetAttributes() const { return m_Attributes; }
	int GetAttributeValue(AttributeIdentifier ID) const { return m_Attributes.contains(ID) ? m_Attributes.at(ID) : 0; }
	float GetExtraBoostAttribute(AttributeIdentifier ID) const { return m_ExtraBoost.contains(ID) ? m_ExtraBoost.at(ID) : .0f; }
	const CTeeInfo& GetTeeInfo() const { return m_ProfessionSkin; }

private:
	std::string GetPreparedJsonString() const;
};

/*
 * Tank profession
 */
class CTankProfession : public CProfession
{
public:
	CTankProfession() : CProfession(ProfessionIdentifier::Tank, PROFESSION_TYPE_WAR)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::HP] = 0;
		m_Attributes[AttributeIdentifier::Lucky] = 0;

		// extra boost for profession
		m_ExtraBoost[AttributeIdentifier::HP] = 30.f;
		m_ExtraBoost[AttributeIdentifier::MP] = 5.f;
		m_ExtraBoost[AttributeIdentifier::DMG] = 10.f;

		// profession skin
		m_ProfessionSkin.m_UseCustomColor = 0;
		str_copy(m_ProfessionSkin.m_aSkinName, "red_panda", sizeof(m_ProfessionSkin.m_aSkinName));
	}
};

/*
 * DPS profession
 */
class CDPSProfession : public CProfession
{
public:
	CDPSProfession() : CProfession(ProfessionIdentifier::Dps, PROFESSION_TYPE_WAR)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::Crit] = 0;
		m_Attributes[AttributeIdentifier::AttackSPD] = 0;

		// extra boost for profession
		m_ExtraBoost[AttributeIdentifier::HP] = 5.f;
		m_ExtraBoost[AttributeIdentifier::MP] = 15.f;
		m_ExtraBoost[AttributeIdentifier::DMG] = 30.f;

		// profession skin
		m_ProfessionSkin.m_UseCustomColor = 0;
		str_copy(m_ProfessionSkin.m_aSkinName, "flokes", sizeof(m_ProfessionSkin.m_aSkinName));
	}
};

/*
 * Healer profession
 */
class CHealerProfession : public CProfession
{
public:
	CHealerProfession()
		: CProfession(ProfessionIdentifier::Healer, PROFESSION_TYPE_WAR)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::MP] = 0;
		m_Attributes[AttributeIdentifier::Vampirism] = 0;

		// extra boost for profession
		m_ExtraBoost[AttributeIdentifier::HP] = 15.f;
		m_ExtraBoost[AttributeIdentifier::MP] = 30.f;
		m_ExtraBoost[AttributeIdentifier::DMG] = 5.f;

		// profession skin
		m_ProfessionSkin.m_UseCustomColor = 0;
		str_copy(m_ProfessionSkin.m_aSkinName, "Empieza", sizeof(m_ProfessionSkin.m_aSkinName));
	}
};

/*
 * Farmer profession
 */
class CFarmerProfession : public CProfession
{
public:
	CFarmerProfession()
		: CProfession(ProfessionIdentifier::Farmer, PROFESSION_TYPE_OTHER)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::Extraction] = 1;
	}
};

/*
 * Miner profession
 */
class CMinerProfession : public CProfession
{
public:
	CMinerProfession()
		: CProfession(ProfessionIdentifier::Miner, PROFESSION_TYPE_OTHER)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::Efficiency] = 1;
	}
};

/*
 * Fisherman profession
 */
class CFishermanProfession : public CProfession
{
public:
	CFishermanProfession()
		: CProfession(ProfessionIdentifier::Fisherman, PROFESSION_TYPE_OTHER)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::Patience] = 1;
	}
};

/*
 * Loader profession
 */
class CLoaderProfession : public CProfession
{
public:
	CLoaderProfession()
		: CProfession(ProfessionIdentifier::Loader, PROFESSION_TYPE_OTHER)
	{
		// availables for upgrades
		m_Attributes[AttributeIdentifier::ProductCapacity] = 1;
	}
};

#endif