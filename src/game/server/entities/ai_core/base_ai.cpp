#include "base_ai.h"

#include "../ai_entities/indicator.h"

#include <engine/server.h>
#include <game/server/entities/character_bot.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>

CBaseAI::CBaseAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: m_pPlayer(pPlayer), m_pCharacter(pCharacter)
{
	m_ClientID = pPlayer->GetCID();
	m_Target.Reset();
	m_Target.Init(m_pCharacter);
	m_SpawnPoint = m_pCharacter->m_Core.m_Pos;
	m_EmotionStyle = EMOTE_NORMAL;
}

void CBaseAI::EnableBotIndicator(int Type, int SubType)
{
	if(!m_pEntBotIndicator)
		m_pEntBotIndicator = new CEntityBotIndicator(&GS()->m_World, m_ClientID, Type, SubType);
}

void CBaseAI::DisableBotIndicator()
{
	if(!m_pEntBotIndicator)
		return;

	delete m_pEntBotIndicator;
	m_pEntBotIndicator = nullptr;
}

IServer* CBaseAI::Server() const
{
	return Instance::Server();
}

CGS* CBaseAI::GS() const
{
	return m_pPlayer->GS();
}

CPlayer* CBaseAI::SearchPlayerCondition(float Distance, const std::function<bool(CPlayer*)>& Condition)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pCandidatePlayer = GS()->GetPlayer(i);

		if(!pCandidatePlayer || !pCandidatePlayer->GetCharacter())
			continue;

		if(!GS()->IsPlayerInWorld(i))
			continue;

		const vec2& PlayerPos = m_pCharacter->m_Core.m_Pos;
		const vec2& CandidatePos = pCandidatePlayer->GetCharacter()->m_Core.m_Pos;
		if(distance(PlayerPos, CandidatePos) > Distance)
			continue;

		const bool IntersectedWithInvisibleLine = GS()->Collision()->IntersectLineWithInvisible(CandidatePos, PlayerPos, nullptr, nullptr);
		m_Target.UpdateCollised(IntersectedWithInvisibleLine);
		if(GS()->Collision()->IntersectLineWithInvisible(CandidatePos, PlayerPos, nullptr, nullptr))
			continue;

		if(!Condition(pCandidatePlayer))
			continue;

		return pCandidatePlayer;
	}

	return nullptr;
}

CPlayerBot* CBaseAI::SearchPlayerBotCondition(float Distance, const std::function<bool(CPlayerBot*)>& Condition)
{
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if(m_pPlayer->GetCID() == i)
			continue;

		CPlayerBot* pCandidatePlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));

		if(!pCandidatePlayer || !pCandidatePlayer->GetCharacter())
			continue;

		if(pCandidatePlayer->IsDisabledBotDamage())
			continue;

		const vec2& PlayerPos = m_pCharacter->m_Core.m_Pos;
		const vec2& CandidatePos = pCandidatePlayer->GetCharacter()->m_Core.m_Pos;
		if(distance(PlayerPos, CandidatePos) > Distance)
			continue;

		const bool IntersectedWithInvisibleLine = GS()->Collision()->IntersectLineWithInvisible(CandidatePos, PlayerPos, nullptr, nullptr);
		m_Target.UpdateCollised(IntersectedWithInvisibleLine);
		if(GS()->Collision()->IntersectLineWithInvisible(CandidatePos, PlayerPos, nullptr, nullptr))
			continue;

		if(!Condition(pCandidatePlayer))
			continue;

		return pCandidatePlayer;
	}

	return nullptr;
}