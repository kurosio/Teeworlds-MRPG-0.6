#include "base_ai.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>

#include "game/server/entities/botai/character_bot_ai.h"

CBaseAI::CBaseAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: m_pPlayer(pPlayer), m_pCharacter(pCharacter)
{
	m_ClientID = pPlayer->GetCID();
	m_Target.Reset();
	m_Target.Init(m_pCharacter);
	m_SpawnPoint = m_pCharacter->GetPos();
}

IServer* CBaseAI::Server() const
{
	return Instance::Server();
}

CGS* CBaseAI::GS() const
{
	return m_pPlayer->GS();
}

void CBaseAI::SelectEmoteAtRandomInterval(int EmotionStyle) const
{
	if(EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	const int emoteInterval = Server()->TickSpeed() * 3 + rand() % 10;
	if(Server()->Tick() % emoteInterval == 0)
	{
		const int duration = 1 + rand() % 2;
		m_pCharacter->SetEmote(EMOTE_BLINK, duration, true);
	}
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