/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/core/components/mails/mail_wrapper.h>
#include <game/server/gamecontext.h>

#include "achievement_data.h"

CGS* CAchievement::GS() const { return (CGS*)Instance::GameServerPlayer(m_ClientID); }
CPlayer* CAchievement::GetPlayer() const { return GS()->GetPlayer(m_ClientID); }

void CAchievementInfo::InitData(const std::string& RewardData)
{
	// parse the reward data
	Utils::Json::parseFromString(RewardData, [this](nlohmann::json& pJson)
	{
		m_RewardData = std::move(pJson);
	});

	// initialize achievement group
	m_Group = ACHIEVEMENT_GROUP_GENERAL;
	switch(m_Type)
	{
		case ACHIEVEMENT_DEFEAT_PVP:
		case ACHIEVEMENT_DEFEAT_PVE:
		case ACHIEVEMENT_DEFEAT_MOB:
		case ACHIEVEMENT_DEATH:
		case ACHIEVEMENT_TOTAL_DAMAGE:
			m_Group = ACHIEVEMENT_GROUP_BATTLE;
			break;
		case ACHIEVEMENT_EQUIP:
		case ACHIEVEMENT_RECEIVE_ITEM:
		case ACHIEVEMENT_HAVE_ITEM:
		case ACHIEVEMENT_CRAFT_ITEM:
			m_Group = ACHIEVEMENT_GROUP_ITEMS;
			break;
		default: 
			m_Group = ACHIEVEMENT_GROUP_GENERAL;
			break;
	}
}

bool CAchievementInfo::CheckAchievement(int Misc, const CAchievement* pAchievement) const
{
	if(m_Criteria != Misc && m_Criteria > 0)
		return false;

	switch(m_Type)
	{
		case ACHIEVEMENT_EQUIP:
		case ACHIEVEMENT_UNLOCK_WORLD:
			return pAchievement->m_Progress > 0;
		case ACHIEVEMENT_CRAFT_ITEM:
		case ACHIEVEMENT_DEFEAT_MOB:
		case ACHIEVEMENT_RECEIVE_ITEM:
		case ACHIEVEMENT_HAVE_ITEM:
		case ACHIEVEMENT_DEFEAT_PVE:
		case ACHIEVEMENT_DEFEAT_PVP:
		case ACHIEVEMENT_DEATH:
		case ACHIEVEMENT_TOTAL_DAMAGE:
		case ACHIEVEMENT_LEVELING:
			return pAchievement->m_Progress >= m_Required;
		default:
			 dbg_assert(false, "unknown achievement type");
			 return false;
	}
}

bool CAchievement::UpdateProgress(int Misc, int Value, int ProgressType)
{
	if(m_Completed || !GetPlayer())
		return false;

	// update the achievement progress
	CPlayer* pPlayer = GetPlayer();
	switch(ProgressType)
	{
		case PROGRESS_SET:    m_Progress = Value; break;
		case PROGRESS_REMOVE: m_Progress -= Value; break;
		case PROGRESS_ADD:    m_Progress += Value; break;
		default:              return false;
	}
	m_Progress = clamp(m_Progress, 0, m_pInfo->GetRequired());

	// check if the achievement is completed
	if(m_pInfo->CheckAchievement(Misc, this))
	{
		m_Completed = true;
		RewardPlayer(pPlayer);
		GS()->CreateHammerHit(pPlayer->m_ViewPos);
		GS()->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);
		GS()->Chat(m_ClientID, "'{}' has completed the achievement '{}'!", Server()->ClientName(m_ClientID), m_pInfo->GetName());
	}
	else
	{
		NotifyPlayerProgress(pPlayer);
	}

	pPlayer->Account()->SetAchieventProgress(m_pInfo->GetID(), m_Progress, m_Completed);
	return true;
}

void CAchievement::RewardPlayer(CPlayer* pPlayer) const
{
	const auto& JsonData = m_pInfo->GetRewardData();
	if(JsonData.empty()) return;

	int Exp = JsonData.value("exp", 0);
	if(Exp > 0)
	{
		pPlayer->Account()->AddExperience(Exp);
		GS()->Chat(m_ClientID, "You received {} exp!", Exp);
	}

	for(const CItemsContainer Items = CItem::FromArrayJSON(JsonData, "items"); auto& Item : Items)
		pPlayer->GetItem(Item)->Add(Item.GetValue());

	if(int AchievementPoints = m_pInfo->GetAchievementPoint(); AchievementPoints > 0)
	{
		auto* pPlayerItem = pPlayer->GetItem(itAchievementPoint);
		pPlayerItem->Add(AchievementPoints);
		GS()->Chat(m_ClientID, "You received {}({}) achievement points!", AchievementPoints, pPlayerItem->GetValue());
	}
}

void CAchievement::NotifyPlayerProgress(CPlayer* pPlayer)
{
	int Percent = translate_to_percent(m_pInfo->GetRequired(), m_Progress);
	if(Percent > 80 && !m_NotifiedSoonComplete)
	{
		m_NotifiedSoonComplete = true;
		GS()->Chat(m_ClientID, "Achievement '{}' will be soon completed!", m_pInfo->GetName());
	}
}
