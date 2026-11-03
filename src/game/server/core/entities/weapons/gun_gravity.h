#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_GUN_GRAVITY_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_GUN_GRAVITY_H

#include <game/server/entity.h>

class CEntityGravityGun : public CEntity
{
	CCharacter* m_pOwner;
	CEntity* m_pTarget;
	int m_EndTick;
	int m_NextImpactTick;
	int m_PhysicsTick { -1 };
	int m_PulseStartTick;
	float m_PulseStrength { 1.f };
	vec2 m_VelocityBeforeMove {};

	CEntityGravityGun(CCharacter* pOwner, CEntity* pTarget);
	static CEntityGravityGun* FindOwnerCapture(const CEntity* pOwner);
	static CEntityGravityGun* FindTargetCapture(const CEntity* pTarget);
	static CEntity* FindTarget(CCharacter* pOwner);
	bool IsCaptureValid() const;
	int RemainingSeconds() const;
	void Release();
	void DamageOnImpact();
	void SnapCaptureField(int SnappingClient);

public:
	enum class StopScope
	{
		AnyParticipant,
		OwnerOnly,
	};

	static bool IsEquipped(const CCharacter* pCharacter);
	static void Fire(CCharacter* pOwner);
	static int SecondsLeft(const CCharacter* pOwner);
	static void Stop(CEntity* pEntity, StopScope Scope = StopScope::AnyParticipant);
	static bool BeforeMove(CEntity* pEntity);
	static void AfterMove(CCharacter* pCharacter);

	void Tick() override;
	void Reset() override { Release(); }
	void Snap(int SnappingClient) override;
};

#endif
