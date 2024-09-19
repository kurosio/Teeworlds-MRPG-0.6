#include "quest_mob.h"

#include <game/server/entities/botai/character_bot_ai.h>
#include <game/server/entity_manager.h>
#include <game/server/core/components/quests/quest_manager.h>
#include <game/server/gamecontext.h>

CQuestMobAI::CQuestMobAI(CQuestBotMobInfo* pQuestMobInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter), m_pQuestMobInfo(pQuestMobInfo) {}

void CQuestMobAI::OnRewardPlayer(CPlayer* pPlayer, vec2 Force) const
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pPlayer->GetBotID();

	if(m_pQuestMobInfo->m_ActiveForClient[ClientID])
	{
		m_pQuestMobInfo->m_CompleteClient[ClientID] = true;
	}

	GS()->Core()->QuestManager()->TryAppendDefeatProgress(pPlayer, BotID);
}

void CQuestMobAI::OnTargetRules(float Radius)
{
	const auto* pTarget = GS()->GetPlayer(m_Target.GetCID(), false, true);
	const auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidate)
	{
		const bool DamageDisabled = pCandidate->GetCharacter()->m_Core.m_DamageDisabled;
		const bool IsActiveForClient = m_pQuestMobInfo->m_ActiveForClient[pCandidate->GetCID()];

		if(pTarget)
		{
			const int CurrentTotalAttHP = pTarget->GetTotalAttributeValue(AttributeIdentifier::HP);
			const int CandidateTotalAttHP = pCandidate->GetTotalAttributeValue(AttributeIdentifier::HP);
			return !DamageDisabled && IsActiveForClient && CurrentTotalAttHP < CandidateTotalAttHP;
		}

		return !DamageDisabled && IsActiveForClient;
	});

	if(pPlayer)
	{
		m_Target.Set(pPlayer->GetCID(), 100);
	}
}

void CQuestMobAI::Process()
{
	const float DistanceBetweenSpawnpoint = distance(m_SpawnPoint, m_pCharacter->GetPos());
	if(DistanceBetweenSpawnpoint > 800.0f)
	{
		m_pCharacter->ChangePosition(m_SpawnPoint);
	}
	else if(DistanceBetweenSpawnpoint > 400.0f && !m_Target.IsEmpty())
	{
		m_Target.Reset();
	}
	else
	{
		m_pCharacter->UpdateTarget(800.0f);
	}
	m_pCharacter->ResetInput();


	bool MobMove = true;
	if(const auto* pTargetChar = GS()->GetPlayerChar(m_Target.GetCID()))
	{
		m_pPlayer->m_TargetPos = pTargetChar->GetPos();
		m_pCharacter->Fire();
	}
	else
	{
		if(DistanceBetweenSpawnpoint < 128.0f)
		{
			m_pPlayer->m_TargetPos = {};
			m_pCharacter->m_Input.m_Direction = 0;
			MobMove = false;
		}
		else
		{
			m_pPlayer->m_TargetPos = m_SpawnPoint;
		}
	}

	if(MobMove)
	{
		m_pCharacter->SelectWeaponAtRandomInterval();
		m_pCharacter->Move();
	}

	SelectEmoteAtRandomInterval(0); // TODO
}

void CQuestMobAI::OnSnapDDNetCharacter(int SnappingClient, CNetObj_DDNetCharacter* pDDNetCharacter)
{
	if(!m_pQuestMobInfo->m_ActiveForClient[SnappingClient])
	{
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_SOLO;
		pDDNetCharacter->m_Flags |= CHARACTERFLAG_COLLISION_DISABLED;
	}
}