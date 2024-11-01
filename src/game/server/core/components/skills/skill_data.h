/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_DATA_H

class CEntityGroup;
class CCharacter;

// skill description
class CSkillDescription : public MultiworldIdentifiableData< std::map < int, CSkillDescription > >
{
	friend class CSkillManager;

	int m_ID {};
	char m_aName[32] {};
	char m_aDescription[64] {};
	char m_aBoostName[64] {};
	int m_BoostDefault {};
	ProfessionIdentifier m_ProfessionID{};
	int m_PercentageCost {};
	int m_PriceSP {};
	int m_MaxLevel {};
	bool m_Passive {};

public:
	// constructors
	CSkillDescription() = default;
	CSkillDescription(int ID) : m_ID(ID) {}

	// intialize skill description
	void Init(const std::string& Name, const std::string& Description, const std::string& BonusName, int BonusDefault, 
		ProfessionIdentifier ProfID, int PercentageCost, int PriceSP, int MaxLevel, bool Passive)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aDescription, Description.c_str(), sizeof(m_aDescription));
		str_copy(m_aBoostName, BonusName.c_str(), sizeof(m_aBoostName));
		m_BoostDefault = BonusDefault;
		m_ProfessionID = ProfID;
		m_PercentageCost = PercentageCost;
		m_PriceSP = PriceSP;
		m_MaxLevel = MaxLevel;
		m_Passive = Passive;
		CSkillDescription::m_pData[m_ID] = *this;
	}

	// getters and setters
	int GetID() const
	{
		return m_ID;
	}

	const char* GetName() const
	{
		return m_aName;
	}

	const char* GetDescription() const
	{
		return m_aDescription;
	}

	const char* GetBoostName() const
	{
		return m_aBoostName;
	}

	int GetBoostDefault() const
	{
		return m_BoostDefault;
	}

	ProfessionIdentifier GetProfessionID() const
	{
		return m_ProfessionID;
	}

	int GetPercentageCost() const
	{
		return m_PercentageCost;
	}

	int GetPriceSP() const
	{
		return m_PriceSP;
	}

	int GetMaxLevel() const
	{
		return m_MaxLevel;
	}

	bool IsPassive() const
	{
		return m_Passive;
	}
};

// skill data
class CSkill : public MultiworldIdentifiableData< std::map< int, std::deque < CSkill* > > >
{
	friend class CSkillManager;

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

	int m_ID{};
	int m_ClientID{};
	int m_Level{};
	int m_SelectedEmoticion{};
	std::array< std::weak_ptr<CEntityGroup>, NUM_SKILLS > m_apEntSkillPtrs {};

public:
	// constructors
	CSkill() = default;

	// create a new instance of CSkill
	static CSkill* CreateElement(int ClientID, const int& SkillID) noexcept
	{
		auto pData = new CSkill;
		pData->m_ID = SkillID;
		pData->m_ClientID = ClientID;
		return m_pData[ClientID].emplace_back(pData);
	}

	// initialize skill
	void Init(int Level, int SelectedEmoticion)
	{
		m_Level = Level;
		m_SelectedEmoticion = SelectedEmoticion;
	}

	// getters setters
	void SetID(int SkillID)
	{
		m_ID = SkillID;
	}

	int GetID() const
	{
		return m_ID;
	}

	bool IsLearned() const
	{
		return m_Level > 0;
	}

	int GetLevel() const
	{
		return m_Level;
	}

	int GetBonus() const
	{
		return m_Level * Info()->GetBoostDefault();
	}

	const char* GetSelectedEmoticonName() const
	{
		return GetEmoticonNameById(m_SelectedEmoticion);
	}

	CSkillDescription* Info() const
	{
		return &CSkillDescription::Data()[m_ID];
	}

	std::string GetStringLevelStatus() const;

	// functions
	void SelectNextControlEmote();
	bool Upgrade();
	bool Use();

private:
	enum
	{
		SKILL_USAGE_NONE,
		SKILL_USAGE_RESET,
		SKILL_USAGE_TOGGLE,
	};
	bool IsActivated(CCharacter* pChar, int Manacost, int SkillID, int SkillUsage = SKILL_USAGE_NONE) const;
};

#endif
