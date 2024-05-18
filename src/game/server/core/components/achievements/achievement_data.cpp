/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "achievement_data.h"

void CAchievementInfo::InitCriteriaJson(const std::string& JsonData)
{
    Tools::Json::parseFromString(JsonData, [this](nlohmann::json& pJson)
    {
        for(auto& pAchievement : pJson)
        {
	        const int Value = pAchievement.value("value", 0);
            const int Value2 = pAchievement.value("value2", 0);

            switch (m_Type)
            {
                case ACHIEVEMENT_TYPE_CRAFT_ITEM:
                    m_Data.m_CraftItem.m_ItemID = Value;
                    m_Data.m_CraftItem.m_Value = Value2;
                    break;
                case ACHIEVEMENT_TYPE_DEFEAT_MOB:
                    m_Data.m_DefeatMob.m_ID = Value;
                    m_Data.m_DefeatMob.m_Value = Value2;
                    break;
                case ACHIEVEMENT_TYPE_DEFEAT_PVE:
                    m_Data.m_DefeatPVE.m_Value = Value;
                    break;
                case ACHIEVEMENT_TYPE_DEFEAT_PVP:
                    m_Data.m_DefeatPVP.m_Value = Value;
                    break;
                case ACHIEVEMENT_TYPE_ENCHANT_ITEM:
                    m_Data.m_EnchantItem.m_ItemID = Value;
                    break;
                case ACHIEVEMENT_TYPE_EQUIPPED:
                    m_Data.m_Equipped.m_ItemID = Value;
                    break;
                case ACHIEVEMENT_TYPE_GET_ITEM:
                    m_Data.m_GetItem.m_ItemID = Value;
                    break;
                case ACHIEVEMENT_TYPE_USE_ITEM:
                    m_Data.m_UseItem.m_ItemID = Value;
                    break;
                case ACHIEVEMENT_TYPE_UNLOCK_WORLD:
                    m_Data.m_UnlockWorld.m_WorldID = Value;
                    break;
                case ACHIEVEMENT_TYPE_LEVELING:
					m_Data.m_Leveling.m_Value = Value;
					break;
                default:
                    dbg_assert(false, "unknown achievement type");
                    break;
            }
        }
    });
}
