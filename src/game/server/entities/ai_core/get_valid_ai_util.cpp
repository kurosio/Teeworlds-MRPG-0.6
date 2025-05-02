#include "get_valid_ai_util.h"

#include "eidolon_ai.h"
#include "mob_ai.h"
#include "npc_ai.h"
#include "quest_mob_ai.h"
#include "quest_npc_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/playerbot.h>

template <typename TargetAI>
TargetAI* GetValidAI(CPlayerBot* pPlayerBot)
{
    if(!pPlayerBot)
        return nullptr;

    auto* pCharBot = dynamic_cast<CCharacterBotAI*>(pPlayerBot->GetCharacter());
    if(!pCharBot)
        return nullptr;

    CBaseAI* pBaseAI = pCharBot->AI();
    if(!pBaseAI)
        return nullptr;

    return (dynamic_cast<TargetAI*>(pBaseAI));
}

template CEidolonAI* GetValidAI<CEidolonAI>(CPlayerBot* pPlayerBot);
template CMobAI* GetValidAI<CMobAI>(CPlayerBot* pPlayerBot);
template CNpcAI* GetValidAI<CNpcAI>(CPlayerBot* pPlayerBot);
template CQuestMobAI* GetValidAI<CQuestMobAI>(CPlayerBot* pPlayerBot);
template CQuestNpcAI* GetValidAI<CQuestNpcAI>(CPlayerBot* pPlayerBot);