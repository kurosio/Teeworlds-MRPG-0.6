#include "npc_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/gamecontext.h>

//#include <game/server/entities/botai/nurse_heart.h>

CNpcAI::CNpcAI(NpcBotInfo* pNpcInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter), m_pNpcInfo(pNpcInfo) {}

void CNpcAI::OnSpawn()
{
	const int Function = m_pNpcInfo->m_Function;

	if(Function == FUNCTION_NPC_GIVE_QUEST)
	{
		m_pCharacter->AddMultipleOrbite(3, POWERUP_ARMOR, 0);
	}
	else if(Function == FUNCTION_NPC_NURSE)
	{
		//new CNurseHeart(&GS()->m_World, m_ClientID);
	}
	else if(Function == FUNCTION_NPC_GUARDIAN)
	{
		m_pCharacter->AddMultipleOrbite(2, POWERUP_NINJA, POWERUP_WEAPON);
	}
}

void CNpcAI::OnTakeDamage(int Dmg, int From, int Weapon)
{
	const auto* pFrom = GS()->GetPlayer(From);
	const int Function = m_pNpcInfo->m_Function;

	if(!pFrom->IsBot())
	{
		pFrom->Account()->IncreaseCrimeScore(1 + rand() % 8);
		m_pCharacter->SetEmote(EMOTE_ANGRY, 1, true);

		if(Function == FUNCTION_NPC_GUARDIAN && pFrom->Account()->IsCrimeScoreMaxedOut())
		{
			m_Target.Set(From, 200);
		}
	}
}

void CNpcAI::OnHandleTunning(CTuningParams* pTuning)
{
	const int Function = m_pNpcInfo->m_Function;

	// walk effect
	if(Function != FUNCTION_NPC_GUARDIAN || (Function == FUNCTION_NPC_GUARDIAN && m_Target.IsEmpty()))
	{
		pTuning->m_GroundControlSpeed = 5.0f;
		pTuning->m_GroundControlAccel = 1.0f;
	}
}

void CNpcAI::OnTargetRules(float Radius)
{
	const int Function = m_pNpcInfo->m_Function;

	if(Function == FUNCTION_NPC_GUARDIAN)
	{
		const auto* pTarget = GS()->GetPlayer(m_Target.GetCID(), false, true);
		const auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidate)
		{
			const bool DamageDisabled = pCandidate->GetCharacter()->m_Core.m_DamageDisabled;

			if(pTarget)
			{
				const int CurrentTotalAttHP = pTarget->GetTotalAttributeValue(AttributeIdentifier::HP);
				const int CandidateTotalAttHP = pCandidate->GetTotalAttributeValue(AttributeIdentifier::HP);
				return !DamageDisabled && (CurrentTotalAttHP < CandidateTotalAttHP);
			}

			const bool AgressionFactor = GS()->IsWorldType(WorldType::Dungeon) || rand() % 30 == 0;
			return !DamageDisabled && AgressionFactor;
		});

		if(pPlayer)
		{
			m_Target.Set(pPlayer->GetCID(), 100);
		}
	}
}

void CNpcAI::ProcessGuardianNPC() const
{
	const float DistanceBetweenSpawnpoint = distance(m_SpawnPoint, m_pCharacter->GetPos());
	if(DistanceBetweenSpawnpoint > 800.0f && m_Target.IsEmpty())
	{
		m_pCharacter->ChangePosition(m_SpawnPoint);
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
		if(DistanceBetweenSpawnpoint < 256.0f)
		{
			if(Server()->Tick() % Server()->TickSpeed() == 0)
			{
				m_pCharacter->m_Input.m_TargetY = rand() % 4 - rand() % 8;
			}

			m_pCharacter->m_Input.m_TargetX = (m_pCharacter->m_Input.m_Direction * 10 + 1);
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
		m_pCharacter->Move();
	}

	SelectEmoteAtRandomInterval(0); // TODO
}

void CNpcAI::ProcessDefaultNPC()
{
	const float collisionWidth = GS()->Collision()->GetWidth() * 32.0f;
	const float npcPosX = m_pCharacter->GetPos().x + m_pCharacter->m_Input.m_Direction * 45.0f;

	m_pCharacter->m_Input.m_Direction = (npcPosX < 0) ? 1 : (npcPosX >= collisionWidth) ? -1 : m_pCharacter->m_Input.m_Direction;
	m_pCharacter->m_LatestPrevInput = m_pCharacter->m_LatestInput;
	m_pCharacter->m_LatestInput = m_pCharacter->m_Input;

	// behavior
	bool HasPlayerNearby = false;
	SearchPlayerCondition(128.f, [&](CPlayer* pCandidate)
	{
		const int CandidateCID = pCandidate->GetCID();

		if(m_pPlayer->IsActive() && m_pPlayer->IsActiveForClient(CandidateCID))
		{
			const vec2& CandidatePos = pCandidate->GetCharacter()->m_Core.m_Pos;
			const vec2& SelfPos = m_pCharacter->m_Core.m_Pos;

			pCandidate->GetCharacter()->SetSafeFlags();
			m_pCharacter->m_Input.m_TargetX = static_cast<int>(CandidatePos.x - SelfPos.x);
			m_pCharacter->m_Input.m_TargetY = static_cast<int>(CandidatePos.y - SelfPos.y);
			m_pCharacter->m_Input.m_Direction = 0;
			HasPlayerNearby = true;

			GS()->Broadcast(CandidateCID, BroadcastPriority::GAME_INFORMATION, 10, "Begin dialogue: \"hammer hit\"");
		}
		return false;
	});

	// random direction target
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_pCharacter->m_Input.m_TargetY = (rand() % 9) - 4;
	}
	m_pCharacter->m_Input.m_TargetX = m_pCharacter->m_Input.m_Direction * 10 + 1;

	// random direction moving
	if(!HasPlayerNearby && !m_pNpcInfo->m_Static && rand() % 50 == 0)
	{
		m_pCharacter->m_Input.m_Direction = -1 + rand() % 3;
	}

	// emote actions
	SelectEmoteAtRandomInterval(0); // TODO
}

void CNpcAI::Process()
{
	// has functional guardian
	if(m_pNpcInfo->m_Function == FUNCTION_NPC_GUARDIAN)
	{
		ProcessGuardianNPC();
		return;
	}

	// has functional nurse
	if(m_pNpcInfo->m_Function == FUNCTION_NPC_NURSE)
	{
		//SetSafeFlags(SAFEFLAG_COLLISION_DISABLED);
		//ProcessNPC();
		return;
	}

	m_pCharacter->SetSafeFlags();
	ProcessDefaultNPC();
}

bool CNpcAI::IsConversational()
{
	return m_pNpcInfo->m_Function != FUNCTION_NPC_GUARDIAN;
}