/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/server.h>

#include <game/server/core/components/mails/mail_wrapper.h>
#include <game/server/gamecontext.h>

#include "achievement_data.h"

void CAchievementInfo::InitData(const std::string& CriteriaData, const std::string& RewardData)
{
    // check if criteria valid
    dbg_assert(CriteriaData.length() > 0, "empty json data");

    // parse the criteria data
    Tools::Json::parseFromString(CriteriaData, [this](nlohmann::json& pJson)
    {
	    m_Misc = pJson.value("value1", 0);
        m_MiscRequired = pJson.value("value2", 0);
    });

    // parse the reward data
    Tools::Json::parseFromString(RewardData, [this](nlohmann::json& pJson)
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
    m_Progress = clamp(m_Progress, 0, m_pInfo->GetMiscRequired());

    // check if the achievement is completed
    if(m_pInfo->CheckAchievement(Misc, this))
    {
        // mark as completed
	    m_Completed = true;

        // reward the player
        const auto& JsonData = m_pInfo->GetRewardData();
        if(!JsonData.empty())
        {
            // experience
            int Exp = JsonData.value("exp", 0);
            if(Exp > 0)
            {
                pPlayer->Account()->AddExperience(Exp);
                GS()->Chat(m_ClientID, "You received {} exp!", Exp);
            }

            // items
            CItemsContainer Items = CItem::FromArrayJSON(JsonData, "items");
            for(auto& Item : Items)
            {
                CPlayerItem* pPlayerItem = pPlayer->GetItem(Item);
                if(Item.Info()->IsEnchantable() && pPlayerItem->HasItem())
                {
                    MailWrapper Mail("System", pPlayer->Account()->GetID(), "Achievement");
                    Mail.AddDescLine("Some awards cannot be earned.");
                    Mail.AttachItem(Item);
                    Mail.Send();
                }
                else
                    pPlayer->GetItem(Item)->Add(Item.GetValue());
            }
        }

        // send msg
        GS()->CreateHammerHit(pPlayer->m_ViewPos);
        GS()->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);
        GS()->Chat(m_ClientID, "'{}' has completed the achievement '{}'!", Server()->ClientName(m_ClientID), m_pInfo->GetName());
    }
    else
    {
	    int Percent = translate_to_percent(m_pInfo->GetMiscRequired(), m_Progress);
        if(Percent > 80 && !m_NotifiedSoonComplete)
        {
            m_NotifiedSoonComplete = true;
            GS()->Chat(m_ClientID, "Achievement '{}' will be soon completed!", m_pInfo->GetName());
        }
    }

    // update the achievement progress
    pPlayer->Account()->SetAchieventProgress(m_pInfo->GetID(), m_Progress, m_Completed);
    return true;
}