/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H
#include <game/server/entity.h>

#include "../core/tools/tiles_handler.h"

enum
{
	SAFEFLAG_HAMMER_HIT_DISABLED = 1 << 0,
	SAFEFLAG_COLLISION_DISABLED = 1 << 1,
	SAFEFLAG_HOOK_HIT_DISABLED = 1 << 2,
	SAFEFLAG_DAMAGE_DISABLED = 1 << 3
};

class CPlayerBot;
class CMultipleOrbite;
class CEntityFishingRod;

class CCharacter : public CEntity
{
	MACRO_ALLOC_POOL_ID()

	class CPlayer* m_pPlayer {};
	CTileHandler* m_pTilesHandler {};

	int m_LastWeapon {};
	int m_QueuedWeapon {};

	// ninja
	struct
	{
		vec2 m_ActivationDir;
		int m_CurrentMoveTime;
		int m_OldVelAmount;
	} m_Ninja {};

	// info for dead reckoning
	CCharacterCore m_SendCore {}; // core that we should send
	CCharacterCore m_ReckoningCore {}; // the dead reckoning core
	CEntityFishingRod* m_pFishingRod {};

	void HandleReload();
	void FireWeapon();
	void HandleWeaponSwitch();
	void DoWeaponSwitch();
	void HandleHookActions();
	bool HandleHammerActions(vec2 Direction, vec2 ProjStartPos);
	void HandleBuff(CTuningParams* TuningParams);
	void HandlePlayer();
	bool CanAccessWorld() const;

	bool FireHammer(vec2 Direction, vec2 ProjStartPos);
	bool FireGun(vec2 Direction, vec2 ProjStartPos);
	bool FireShotgun(vec2 Direction, vec2 ProjStartPos);
	bool FireGrenade(vec2 Direction, vec2 ProjStartPos);
	bool FireRifle(vec2 Direction, vec2 ProjStartPos);

	int GetTotalDamageByWeapon(int Weapon) const;

protected:
	int m_LastDamageByClient {};
	bool m_Alive {};
	int m_Health {};
	int m_Mana {};
	int m_ReckoningTick {};
	int m_LastNoAmmoSound {};
	int m_NumInputs {};
	int m_TriggeredEvents {};
	int m_LastAction {};
	int m_ReloadTimer {};
	int m_AttackTick {};
	int m_EmoteType {};
	int m_EmoteStop {};
	int m_SafeTickFlags {};
	vec2 m_NormalDoorHit {};
	std::string m_Zonename {};
	CMultipleOrbite* m_pMultipleOrbite {};

	void HandleWeapons();
	void HandleNinja();
	void HandleTiles();
	void HandleIndependentTuning();

	void HandleSafeFlags();
	bool StartConversation(CPlayerBot* pTarget) const;
	void HandleEventsDeath(int Killer, vec2 Force) const;

	void AutoUseHealingPotionIfNeeded() const;
	void AutoUseManaPotionIfNeeded() const;
	void TryUsePotion(std::optional<int> optItemID) const;

public:
	// these are non-heldback inputs
	CNetObj_PlayerInput m_Input {};
	CNetObj_PlayerInput m_LatestPrevInput {};
	CNetObj_PlayerInput m_LatestInput {};

	static constexpr int ms_PhysSize = 28;
	CCharacterCore m_Core {};

	int m_AmmoRegen {};
	vec2 m_OldPos {};
	vec2 m_OlderPos {};

	// constructors
	CCharacter(CGameWorld *pWorld);
	~CCharacter() override;

	CPlayer *GetPlayer() const { return m_pPlayer; }
	CTileHandler *GetTiles() const { return m_pTilesHandler; }

	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;
	void PostSnap() override;

	virtual bool Spawn(class CPlayer* pPlayer, vec2 Pos);
	virtual void GiveRandomEffects(int To);
	virtual bool TakeDamage(vec2 Force, int Damage, int FromCID, int Weapon);
	virtual void Die(int Killer, int Weapon);
	virtual void HandleTuning();

	void MovingDisable(bool State);

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetHook();
	void ResetInput();
	bool IsGrounded() const;
	bool IsCollisionFlag(int Flag) const;
	CPlayer* GetHookedPlayer() const;

	const char* GetZonename() const { return m_Zonename.c_str(); }
	void SetSafeFlags(int Flags = SAFEFLAG_DAMAGE_DISABLED | SAFEFLAG_HAMMER_HIT_DISABLED | SAFEFLAG_COLLISION_DISABLED | SAFEFLAG_HOOK_HIT_DISABLED) { m_SafeTickFlags = Flags; }
	virtual bool IsAllowedPVP(int FromID) const;
	bool IsAlive() const { return m_Alive; }
	void SetEmote(int Emote, int Sec, bool StartEmoticion);
	void SetWeapon(int Weapon);
	bool IncreaseHealth(int Amount);
	bool IncreaseMana(int Amount);
	bool TryUseMana(int Mana);
	int Mana() const { return m_Mana; }
	int Health() const { return m_Health; }
	void AddMultipleOrbite(bool Projectile, int Amount, int PickupType, int Subtype, int Orbitetype);
	void RemoveMultipleOrbite(bool Projectile, int Amount, int PickupType, int Subtype, int Orbitetype) const;
	virtual bool GiveWeapon(int Weapon, int Ammo);
	bool RemoveWeapon(int Weapon);
	void ChangePosition(vec2 NewPos);
	void UpdateEquippedStats(std::optional<int> UpdatedItemID = std::nullopt);
	void SetDoorHit(vec2 Start, vec2 End);
	void HandleDoorHit();
	void ResetDoorHit() { m_NormalDoorHit = vec2(0, 0); }
	vec2 GetMousePos() const { return m_Core.m_Pos + vec2(m_Core.m_Input.m_TargetX, m_Core.m_Input.m_TargetY); }
	CPlayer* GetLastAttacker() const;
};

#endif
