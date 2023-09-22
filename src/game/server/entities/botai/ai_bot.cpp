#include "ai_bot.h"

CAIController::CAIController(CCharacterBotAI* pCharacter) : m_pCharacter(pCharacter)
{
	m_Target.Reset();
	m_Target.Init(m_pCharacter);
}