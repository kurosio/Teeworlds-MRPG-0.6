#include "npc_ai.h"

#include <game/server/entities/character_bot.h>
#include <game/server/gamecontext.h>

CNpcAI::CNpcAI(NpcBotInfo* pNpcInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter), m_pNpcInfo(pNpcInfo) {}

bool CNpcAI::CanDamage(CPlayer* pFrom)
{
	if(m_pNpcInfo->m_Function == FUNCTION_NPC_GUARDIAN)
	{
		if(pFrom->IsBot())
		{
			auto* pFromBot = dynamic_cast<CPlayerBot*>(pFrom);
			return pFromBot->GetBotType() == TYPE_BOT_MOB;
		}

		return pFrom->Account()->IsCrimeMaxedOut();
	}
	return false;
}

void CNpcAI::OnSpawn()
{
	const int Function = m_pNpcInfo->m_Function;

	if(Function == FUNCTION_NPC_GIVE_QUEST)
	{
		EnableBotIndicator(POWERUP_HEALTH, 0);
	}
	else if(Function == FUNCTION_NPC_NURSE)
	{
		//new CEntityNurseHeart(&GS()->m_World, m_ClientID);
	}
	else if(Function == FUNCTION_NPC_GUARDIAN)
	{
		EnableBotIndicator(POWERUP_NINJA, POWERUP_WEAPON);
	}
}

void CNpcAI::OnTakeDamage(int Dmg, int From, int Weapon)
{
	const auto* pFrom = GS()->GetPlayer(From);
	const int Function = m_pNpcInfo->m_Function;

	if(!pFrom->IsBot())
	{
		pFrom->Account()->IncreaseCrime(1 + rand() % 8);
		m_pCharacter->SetEmote(EMOTE_ANGRY, 1, true);

		if(Function == FUNCTION_NPC_GUARDIAN && pFrom->Account()->IsCrimeMaxedOut())
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
		auto* pPlayer = SearchPlayerCondition(Radius, [](const CPlayer* pCandidate)
		{
			const bool IsCrimaScoreMax = pCandidate->Account()->IsCrimeMaxedOut();
			const bool DamageDisabled = pCandidate->GetCharacter()->m_Core.m_DamageDisabled;

			return !DamageDisabled && IsCrimaScoreMax;
		});

		if(!pPlayer)
		{
			pPlayer = SearchPlayerBotCondition(Radius, [this](CPlayerBot* pCandidate)
			{
				return m_pCharacter->IsAllowedPVP(pCandidate->GetCID());
			});
		}

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

	if(const auto* pTargetChar = GS()->GetPlayerChar(m_Target.GetCID()))
	{
		m_pPlayer->m_TargetPos = pTargetChar->GetPos();
		m_pCharacter->SelectWeaponAtRandomInterval();
		m_pCharacter->Fire();
		m_pCharacter->Move();
		return;
	}

	if(DistanceBetweenSpawnpoint < 256.0f)
	{
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_pCharacter->m_Input.m_TargetY = rand() % 4 - rand() % 8;
		}

		m_pCharacter->m_Input.m_TargetX = (m_pCharacter->m_Input.m_Direction * 10 + 1);
		m_pCharacter->m_Input.m_Direction = 0;
	}
	else
	{
		m_pPlayer->m_TargetPos = m_SpawnPoint;
		m_pCharacter->Move();
	}
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
	SearchPlayerCondition(128.f, [this, &HasPlayerNearby](CPlayer* pCandidate)
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

			GS()->Broadcast(CandidateCID, BroadcastPriority::GameInformation, 10, "Begin dialogue: \"hammer hit\"");
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
		return;
	}

	m_pCharacter->SetSafeFlags();
	ProcessDefaultNPC();
}

bool CNpcAI::IsConversational()
{
	return m_pNpcInfo->m_Function != FUNCTION_NPC_GUARDIAN;
}