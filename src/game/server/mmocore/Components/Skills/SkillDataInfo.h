/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_DATA_INFO_H
#define GAME_SERVER_COMPONENT_SKILL_DATA_INFO_H

enum SkillType
{
	SKILL_TYPE_IMPROVEMENTS,
	SKILL_TYPE_HEALER,
	SKILL_TYPE_DPS,
	SKILL_TYPE_TANK,
	NUM_SKILL_TYPES,
};

using SkillIdentifier = int;

class CSkillDataInfo : public MultiworldIdentifiableStaticData< std::map < SkillIdentifier, CSkillDataInfo > >
{
	friend class CSkillsCore;

	SkillIdentifier m_ID{};
	char m_aName[32]{};
	char m_aDesc[64]{};
	char m_aBonusName[64]{};
	int m_BonusDefault{};
	SkillType m_Type{};
	int m_ManaPercentageCost{};
	int m_PriceSP{};
	int m_MaxLevel{};
	bool m_Passive{};

public:
	CSkillDataInfo() = default;
	CSkillDataInfo(SkillIdentifier ID) : m_ID(ID) {}

	void Init(const std::string& Name, const std::string& Description, const std::string& BonusName, int BonusDefault, SkillType Type, int PercentageCost, int PriceSP, int MaxLevel, bool Passive)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aDesc, Description.c_str(), sizeof(m_aDesc));
		str_copy(m_aBonusName, BonusName.c_str(), sizeof(m_aBonusName));
		m_BonusDefault = BonusDefault;
		m_Type = Type;
		m_ManaPercentageCost = PercentageCost;
		m_PriceSP = PriceSP;
		m_MaxLevel = MaxLevel;
		m_Passive = Passive;
		CSkillDataInfo::m_pData[m_ID] = *this;
	}

	SkillIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_aName; }
	const char* GetDesc() const { return m_aDesc; }
	const char* GetBonusName() const { return m_aBonusName; }
	int GetBonusDefault() const { return m_BonusDefault; }
	SkillType GetType() const { return m_Type; }
	int GetManaPercentageCost() const { return m_ManaPercentageCost; }
	int GetPriceSP() const { return m_PriceSP; }
	int GetMaxLevel() const { return m_MaxLevel; }
	bool IsPassive() const { return m_Passive; }

	static const char* GetControlEmoteStateName(int EmoticionID);
};

#endif
