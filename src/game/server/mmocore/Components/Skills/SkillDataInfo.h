/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_DATA_INFO_H
#define GAME_SERVER_COMPONENT_SKILL_DATA_INFO_H

enum
{
	SKILL_TYPE_IMPROVEMENTS,
	SKILL_TYPE_HEALER,
	SKILL_TYPE_DPS,
	SKILL_TYPE_TANK,
	NUM_SKILL_TYPES,
};

class CSkillDataInfo
{
public:
	typedef std::map< int, CSkillDataInfo > SkillStores;

private:
	friend class CSkillsCore;

	char m_aName[32];
	char m_aDesc[64];
	char m_aBonusName[64];
	int m_BonusDefault;
	int m_Type;
	int m_ManaPercentageCost;
	int m_PriceSP;
	int m_MaxLevel;
	bool m_Passive;

public:
	const char* GetName() const { return m_aName; }
	const char* GetDesc() const { return m_aDesc; }
	const char* GetBonusName() const { return m_aBonusName; }
	int GetPriceSP() const { return m_PriceSP; }
	int GetManaPercentageCost() const { return m_ManaPercentageCost; }
	int GetBonusDefault() const { return m_BonusDefault; }
	int GetMaxLevel() const { return m_MaxLevel; }

	bool IsPassive() const { return m_Passive; }

	static const char* GetControlEmoteStateName(int EmoticionID);
	
	static SkillStores ms_aSkillsData;
};

#endif
