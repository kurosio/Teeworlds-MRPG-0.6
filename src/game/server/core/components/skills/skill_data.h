/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_DATA_H

#include "skill_tree.h"

class CEntityGroup;
class CCharacter;

// skill description
class CSkillDescription : public MultiworldIdentifiableData< std::map < int, CSkillDescription* > >
{
	friend class CSkillManager;

	int m_ID{};
	std::string m_Name{};
	std::string m_Description{};
	ProfessionIdentifier m_ProfessionID{};
	int m_ManaCostPct {};
	int m_PriceSP {};
	bool m_Passive {};

public:
	// constructors
	CSkillDescription() = default;

	// create a new instance of CSkill
	static CSkillDescription* CreateElement(int ID) noexcept
	{
		auto pData = new CSkillDescription;
		pData->m_ID = ID;
		return m_pData[ID] = std::move(pData);
	}

	// intialize skill description
	void Init(std::string_view Name, std::string_view Description, ProfessionIdentifier ProfID, int ManaCostPct, int PriceSP, bool Passive)
	{
		m_Name = Name;
		m_Description = Description;
		m_ProfessionID = ProfID;
		m_ManaCostPct = ManaCostPct;
		m_PriceSP = PriceSP;
		m_Passive = Passive;
	}

	// getters and setters
	int GetID() const { return m_ID; }
	const char* GetName() const { return m_Name.c_str(); }
	const char* GetDescription() const { return m_Description.c_str(); }
	ProfessionIdentifier GetProfessionID() const { return m_ProfessionID; }
	int GetManaCostPct() const { return m_ManaCostPct; }
	int GetPriceSP() const { return m_PriceSP; }
	bool IsPassive() const { return m_Passive; }
};

// skill data
class CSkill : public MultiworldIdentifiableData< std::map< int, std::deque < CSkill* > > >
{
	friend class CSkillManager;

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

	int m_ID{};
	int m_ClientID{};
	bool m_Learned {};
	int m_SelectedEmoticon{};
	std::weak_ptr<CEntityGroup> m_pEntSkillPtrs {};

	std::unordered_map<int, int> m_TreeSelections;
	void LoadTreeChoicesFromDB();
	int GetTreeNodePriceSP(int LevelIndex, int OptionIndex) const;

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
	void Init(bool Learned, int SelectedEmoticon)
	{
		m_Learned = Learned;
		m_SelectedEmoticon = SelectedEmoticon;
		LoadTreeChoicesFromDB();
	}

	// getters setters
	bool SetTreeOption(int LevelIndex, int OptionIndex);
	int GetTreeProgress() const { return (int)m_TreeSelections.size(); }
	int GetSelectedOption(int LevelIndex) const
	{
		auto it = m_TreeSelections.find(LevelIndex);
		return it == m_TreeSelections.end() ? 0 : it->second;
	}
	int GetMod(SkillMod Mod) const;
	int GetResetCostSP() const;

	int GetID() const { return m_ID; }
	bool IsLearned() const { return m_Learned; }
	const char* GetSelectedEmoticonName() const { return GetEmoticonNameById(m_SelectedEmoticon); }
	CSkillDescription* Info() const { return CSkillDescription::Data()[m_ID]; }
	std::string GetStringLevelStatus() const;

	// functions
	void SelectNextControlEmote();
	bool Upgrade();
	bool Use();
	bool ResetTree();

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
