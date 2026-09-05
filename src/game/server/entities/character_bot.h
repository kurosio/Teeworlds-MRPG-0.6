/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_BOT_H
#define GAME_SERVER_ENTITIES_CHARACTER_BOT_H

#include "character.h"
#include "ai_core/base_ai.h"

enum class AIState
{
	Idle,
	Chase,
	Combat,
	Retreat,
	Search,
	Stuck
};

class CCharacterBotAI : public CCharacter
{
	MACRO_ALLOC_POOL_ID()

		std::unique_ptr<CBaseAI> m_pAI{};
	CPlayerBot* m_pBotPlayer{};

	// FSM state tracking
	AIState m_AIState{ AIState::Idle };
	int m_StateTimer{};

	// live tracking
	int m_HesitationTicks{};
	int m_LookAroundTimer{};
	int m_BoredomTimer{};

	// movement / anti-stuck
	int m_MoveTick{};
	int m_StuckCount{};
	int m_PrevDirection{};
	vec2 m_LastPos{};

	// combat
	int m_StrafeDirection{};
	int m_LastStrafeChangeTick{};
	int m_IntervalChangeWeapon{};
	int m_ReactionTicks{};
	int m_LastDamageTick{};
	int m_LastSeenTargetTick{};
	std::optional<vec2> m_LastKnownTargetPos{};
	vec2 m_PrevTargetPos{};
	vec2 m_TargetVelocity{};

	// aim & weapons
	vec2 m_CurrentAimDir{ 0.0f, 1.0f };
	vec2 m_DieForce{};
	std::optional<int> m_ForcedActiveWeapon{};

	ska::unordered_map<int, int> m_aDamageByPlayer{};

public:
	CCharacterBotAI(CGameWorld* pWorld);

	CBaseAI* AI() const { return m_pAI.get(); }
	void SetForcedWeapon(int WeaponID);
	void ClearForcedWeapon();

	void SelectWeaponAtRandomInterval();
	void SelectEmoteAtRandomInterval();

	bool GiveWeapon(int Weapon, int GiveAmmo) override;
	bool IsAllowedPVP(int FromID) const override;

	void UpdateTarget(float Radius) const;
	void SetAim(vec2 Dir, bool Instant = false);

	ska::unordered_map<int, int>& GetListDmgPlayers() { return m_aDamageByPlayer; }

	void Move();
	void Fire();

private:
	bool Spawn(CPlayer* pPlayer, vec2 Pos) override;
	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;
	void GiveRandomEffects(int ClientID) override;
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int ForceFlag = -1) override;
	void Die(int Killer, int Weapon) override;
	void HandleTuning() override;
	void ProcessBot();

	// FSM modules
	void UpdateAIState(bool HasTarget, bool LineOfSight, float DistToTarget);
	void ChangeState(AIState NewState);
	void OnStateChanged(AIState OldState, AIState NewState);
	void SendBubbleEmoticon(int EmoticonID);

	// moving
	void ExecuteMovement(const vec2& WayPos, int ActiveWayPoints, const vec2& TargetPos);
	void UpdateJumping(const vec2& DirToWaypoint, int ActiveWayPoints);
	void UpdateHooking(const vec2& DirToWaypoint, int ActiveWayPoints, bool HasTarget, const vec2& TargetPos);
	void HandleAntiStuck();

	// fighting
	float GetOptimalDistance() const;
	vec2 CalculateLeadShotAim(const vec2& TargetPos) const;
	bool HasLineOfSight(const vec2& From, const vec2& To) const;
};

#endif