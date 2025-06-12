#include "quest_npc_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

CQuestNpcAI::CQuestNpcAI(QuestBotInfo* pQuestNpcInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter), m_pQuestNpcInfo(pQuestNpcInfo) {}

bool CQuestNpcAI::CanDamage(CPlayer* pFrom)
{
	return false;
}

void CQuestNpcAI::OnSpawn()
{
	m_EmotionStyle = EMOTE_BLINK;

	if(m_pQuestNpcInfo->m_HasAction)
	{
		EnableBotIndicator(POWERUP_ARMOR, 0);
	}
}

void CQuestNpcAI::Process()
{
	m_pCharacter->SetSafeFlags();

	// random direction target
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_pCharacter->m_Input.m_TargetY = (rand() % 9) - 4;
	}
	m_pCharacter->m_Input.m_TargetX = m_pCharacter->m_Input.m_Direction * 10 + 1;

	// behavior
	SearchPlayerCondition(128.f, [&](CPlayer* pCandidate)
	{
		const int CandidateCID = pCandidate->GetCID();

		if(m_pPlayer->IsActive() && m_pPlayer->IsActiveForClient(CandidateCID) != ESnappingPriority::None)
		{
			const vec2& CandidatePos = pCandidate->GetCharacter()->m_Core.m_Pos;
			const vec2& SelfPos = m_pCharacter->m_Core.m_Pos;

			pCandidate->GetCharacter()->SetSafeFlags();
			m_pCharacter->m_Input.m_TargetX = static_cast<int>(CandidatePos.x - SelfPos.x);
			m_pCharacter->m_Input.m_TargetY = static_cast<int>(CandidatePos.y - SelfPos.y);
			m_pCharacter->m_Input.m_Direction = 0;

			GS()->Broadcast(CandidateCID, BroadcastPriority::GameInformation, 10, "Begin dialogue: \"hammer hit\"");
		}
		return false;
	});
}

bool CQuestNpcAI::IsConversational()
{
	return m_pQuestNpcInfo->m_HasAction;
}
