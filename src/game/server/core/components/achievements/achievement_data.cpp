/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/server.h>
#include <game/server/gamecontext.h>

#include "achievement_data.h"

void CAchievementInfo::InitCriteriaJson(const std::string& JsonData)
{
    // check if the json data is empty
    dbg_assert(JsonData.length() > 0, "empty json data");

    // parse the json data
    Tools::Json::parseFromString(JsonData, [this](nlohmann::json& pJson)
    {
	    m_Misc = pJson.value("value1", 0);
        m_MiscRequired = pJson.value("value2", 0);
    });
}

bool CAchievementInfo::CheckAchievement(int Value, const CAchievement* pAchievement) const
{
    switch (m_Type)
    {
        case ACHIEVEMENT_EQUIP:
        case ACHIEVEMENT_UNLOCK_WORLD:
            return m_Misc == Value && pAchievement->m_Progress > 0;
        case ACHIEVEMENT_CRAFT_ITEM:
        case ACHIEVEMENT_DEFEAT_MOB:
        case ACHIEVEMENT_RECEIVE_ITEM:
    	case ACHIEVEMENT_HAVE_ITEM:
            return m_Misc == Value && pAchievement->m_Progress >= m_MiscRequired;
        case ACHIEVEMENT_DEFEAT_PVE:
        case ACHIEVEMENT_DEFEAT_PVP:
		case ACHIEVEMENT_DEATH:
    	case ACHIEVEMENT_TOTAL_DAMAGE:
        case ACHIEVEMENT_LEVELING:
            return pAchievement->m_Progress >= m_MiscRequired;
        default:
            dbg_assert(false, "unknown achievement type");
            break;
    }

    return false;
}

CGS* CAchievement::GS() const { return (CGS*)Instance::GameServerPlayer(m_ClientID); }
CPlayer* CAchievement::GetPlayer() const { return GS()->GetPlayer(m_ClientID); }

bool CAchievement::UpdateProgress(int Misc, int Value, int ProgressType)
{
	// check if the achievement is completed
	if(m_Completed)
		return false;

	// check valid player
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

    // update the achievement progress
    if(ProgressType == PROGRESS_SET)
		m_Progress = Value;
	else if(ProgressType == PROGRESS_REMOVE)
		m_Progress -= Value;
	else if(ProgressType == PROGRESS_ADD)
		m_Progress += Value;

    // check if the achievement is completed
    if(m_pInfo->CheckAchievement(Misc, this))
    {
	    m_Completed = true;
		GS()->Chat(m_ClientID, "Achievement test: '{}'", m_pInfo->GetName());
	}

    // update the achievement progress
    pPlayer->Account()->SetAchieventProgress(m_pInfo->GetID(), m_Progress, m_Completed);
    return true;
}