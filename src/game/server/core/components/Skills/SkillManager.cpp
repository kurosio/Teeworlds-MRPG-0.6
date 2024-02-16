/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "SkillManager.h"

#include <game/server/gamecontext.h>

void CSkillManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_skills_list");
	while (pRes->next())
	{
		std::string Name = pRes->getString("Name").c_str();
		std::string Description = pRes->getString("Description").c_str();
		std::string BoostName = pRes->getString("BoostName").c_str();
		int BoostValue = pRes->getInt("BoostValue");
		int PercentageCost = pRes->getInt("PercentageCost");
		int PriceSP = pRes->getInt("PriceSP");
		int MaxLevel = pRes->getInt("MaxLevel");
		bool Passive = pRes->getBoolean("Passive");
		SkillType Type = (SkillType)pRes->getInt("Type");

		SkillIdentifier ID = pRes->getInt("ID");
		CSkillDescription(ID).Init(Name, Description, BoostName, BoostValue, Type, PercentageCost, PriceSP, MaxLevel, Passive);
	}
}

void CSkillManager::OnInitAccount(CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_skills", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		int Level = pRes->getInt("Level");
		int SelectedEmoticion = pRes->getInt("UsedByEmoticon");

		SkillIdentifier ID = pRes->getInt("SkillID");
		CSkill(ID, ClientID).Init(Level, SelectedEmoticion);
	}
}

void CSkillManager::OnResetClient(int ClientID)
{
	CSkill::Data().erase(ClientID);
}

bool CSkillManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	if(Menulist == MENU_SKILLS_LEARN_LIST)
	{
		const int ClientID = pPlayer->GetCID();
		const char* pTypename[] = { "Improving", "Healing", "Attacking", "Defensive" };

		// information
		CVoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Skill master information");
		VInfo.Add("Here you can learn passive and active skills");
		VInfo.Add("You can bind active skill any button using the console");

		CVoteWrapper::AddItemValue(ClientID, itSkillPoint);

		// Skill types
		CVoteWrapper VTypes(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_STRICT_BOLD, "Skill types");
		VTypes.AddMenu(MENU_SKILLS_LEARN_LIST, SKILL_TYPE_TANK, pTypename[SKILL_TYPE_TANK]);
		VTypes.AddMenu(MENU_SKILLS_LEARN_LIST, SKILL_TYPE_DPS, pTypename[SKILL_TYPE_DPS]);
		VTypes.AddMenu(MENU_SKILLS_LEARN_LIST, SKILL_TYPE_HEALER, pTypename[SKILL_TYPE_HEALER]);
		VTypes.AddMenu(MENU_SKILLS_LEARN_LIST, SKILL_TYPE_IMPROVEMENTS, pTypename[SKILL_TYPE_IMPROVEMENTS]);
		VTypes.AddLine();

		if(pPlayer->m_VotesData.GetMenuTemporaryInteger() > 0)
		{
			int SelectedType = clamp(pPlayer->m_VotesData.GetMenuTemporaryInteger(), (int)SKILL_TYPE_IMPROVEMENTS, (int)NUM_SKILL_TYPES - 1);
			ShowSkillsByType(pPlayer, (SkillType)SelectedType);
		}

		return true;
	}

	return false;
}

void CSkillManager::ShowSkillsByType(CPlayer* pPlayer, SkillType Type) const
{
	for(const auto& [ID, Skill] : CSkillDescription::Data())
	{
		if(Skill.m_Type == Type)
			ShowDetailSkill(pPlayer, ID);
	}
}

void CSkillManager::ShowDetailSkill(CPlayer* pPlayer, SkillIdentifier ID) const
{
	const int ClientID = pPlayer->GetCID();
	CSkill* pSkill = pPlayer->GetSkill(ID);
	CSkillDescription* pInfo = pSkill->Info();

	const bool IsLearned = pSkill->IsLearned();
	const bool IsPassive = pInfo->IsPassive();
	const bool IsMaximumLevel = pSkill->GetLevel() >= pInfo->GetMaxLevel();

	CVoteWrapper VSkill(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "{STR} - {INT}SP {STR}", pInfo->GetName(), pInfo->GetPriceSP(), pSkill->GetStringLevelStatus().c_str());
	VSkill.Add("Description:");
	{
		VSkill.BeginDepthList();
		VSkill.Add(Instance::Localize(ClientID, pInfo->GetDescription()));
		VSkill.EndDepthList();
	}
	VSkill.AddLine();
	VSkill.Add("Main:");
	{
		VSkill.BeginDepthList();
		VSkill.Add("Level: {INT}/{INT}", pSkill->GetLevel(), pInfo->GetMaxLevel());
		VSkill.Add("{INT} {STR} (each level +{INT})", pSkill->GetBonus(), pInfo->GetBoostName(), pInfo->GetBoostDefault());
		VSkill.AddIf(!IsPassive, "Mana required {INT}%", pInfo->GetPercentageCost());
		VSkill.EndDepthList();
	}
	VSkill.AddLine();
	VSkill.AddIf(!IsPassive && IsLearned, "Usage:");
	{
		VSkill.BeginDepthList();
		VSkill.AddIf(!IsPassive && IsLearned, "F1 Bind: (bind 'key' say \"/useskill {INT}\")", ID);
		VSkill.AddIf(!IsPassive && IsLearned, "Used on {STR}", pSkill->GetSelectedEmoticonName());
		VSkill.EndDepthList();
	}
	VSkill.AddLine();
	VSkill.AddIfOption(!IsMaximumLevel, "SKILL_LEARN", ID, "Learn");
	VSkill.AddLine();
}

bool CSkillManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_SKILL_ZONE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_SKILLS_LEARN_LIST);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_SKILL_ZONE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}
	return false;
}

bool CSkillManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if (PPSTR(CMD, "SKILL_LEARN") == 0)
	{
		const int SkillID = VoteID;
		if (pPlayer->GetSkill(SkillID)->Upgrade())
			pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if (PPSTR(CMD, "SKILLCHANGEEMOTICION") == 0)
	{
		const int SkillID = VoteID;
		pPlayer->GetSkill(SkillID)->SelectNextControlEmote();
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}
	return false;
}

void CSkillManager::ParseEmoticionSkill(CPlayer *pPlayer, int EmoticionID)
{
	if(pPlayer && pPlayer->IsAuthed() && pPlayer->GetCharacter())
	{
		const int ClientID = pPlayer->GetCID();
		for(auto& [ID, Skill] : CSkill::Data()[ClientID])
		{
			if (Skill.m_SelectedEmoticion == EmoticionID)
				Skill.Use();
		}
	}
}
