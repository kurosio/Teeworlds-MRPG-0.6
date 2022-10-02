/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_H
#define GAME_SERVER_ENTITIES_CHARACTER_H
#include <game/server/entity.h>

#include "../mmocore/TileHandle.h"

class CCharacter : public CEntity
{
	MACRO_ALLOC_POOL_ID()

	// player controlling this character
	class CPlayer *m_pPlayer;
	TileHandle *m_pHelper;

	bool m_Alive;
	int m_Event;
	int m_Mana;
	int m_LastWeapon;
	int m_QueuedWeapon;
	int m_AttackTick;
	int m_EmoteType;
	int m_EmoteStop;

	// last tick that the player took any action ie some input
	int m_LastAction;
	int m_LastNoAmmoSound;
	int m_NumInputs;
	int m_TriggeredEvents;

	// info for dead reckoning
	int m_ReckoningTick; // tick that we are performing dead reckoning From
	CCharacterCore m_SendCore; // core that we should send
	CCharacterCore m_ReckoningCore; // the dead reckoning core

	void FireWeapon();
	void HandleWeapons();
	void HandleWeaponSwitch();
	void DoWeaponSwitch();
	bool InteractiveHammer(vec2 Direction, vec2 ProjStartPos);
	//void InteractiveGun(vec2 Direction, vec2 ProjStartPos);
	//void InteractiveShotgun(vec2 Direction, vec2 ProjStartPos);
	//void InteractiveGrenade(vec2 Direction, vec2 ProjStartPos);
	//void InteractiveRifle(vec2 Direction, vec2 ProjStartPos);
	bool DecoInteractive();
	void HandleTuning();
	void HandleBuff(CTuningParams* TuningParams);
	void HandleAuthedPlayer();
	bool IsLockedWorld();

protected:
	int m_Health;
	int m_Armor;

	// these are non-heldback inputs
	CNetObj_PlayerInput m_Input;
	CNetObj_PlayerInput m_LatestPrevInput;
	CNetObj_PlayerInput m_LatestInput;

	void HandleTilesets();
	void HandleEvents();

public:
	//character's size
	static const int ms_PhysSize = 28;
	CCharacter(CGameWorld *pWorld);
	~CCharacter() override;

	CPlayer *GetPlayer() const { return m_pPlayer; }
	TileHandle *GetHelper() const { return m_pHelper; }

	virtual int GetSnapFullID() const;
	void Tick() override;
	void TickDefered() override;
	void Snap(int SnappingClient) override;
	void PostSnap() override;

	virtual bool Spawn(class CPlayer* pPlayer, vec2 Pos);
	virtual void GiveRandomEffects(int To);
	virtual bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon);
	virtual void Die(int Killer, int Weapon);

	void OnPredictedInput(CNetObj_PlayerInput *pNewInput);
	void OnDirectInput(CNetObj_PlayerInput *pNewInput);
	void ResetHook();
	void ResetInput();
	bool IsGrounded() const;

	bool IsAllowedPVP(int FromID) const;
	bool IsAlive() const { return m_Alive; }
	void SetEvent(int EventID) { m_Event = EventID; }
	void SetEmote(int Emote, int Sec);
	void SetWeapon(int W);
	bool IncreaseHealth(int Amount);
	bool IncreaseMana(int Amount);
	bool CheckFailMana(int Mana);
	int Mana() const { return m_Mana; }
	int Health() const { return m_Health; }

	virtual bool GiveWeapon(int Weapon, int GiveAmmo);
	bool RemoveWeapon(int Weapon);

	void CreateSnapProj(int SnapID, int Value, int TypeID, bool Dynamic, bool Projectile);
	void RemoveSnapProj(int Value, int SnapID, bool Effect = false);

	void ChangePosition(vec2 NewPos);
	void ResetDoorPos();
	void UpdateEquipingStats(int ItemID);

	// input
	vec2 GetMousePos() const { return m_Core.m_Pos + vec2(m_Core.m_Input.m_TargetX, m_Core.m_Input.m_TargetY); }
	int m_ActiveWeapon;
	int m_Jumped;

	// the player core for the physics
	CCharacterCore m_Core;

	// allow perm
	bool m_SkipDamage;
	int m_AmmoRegen;
	int m_ReloadTimer;

	vec2 m_OldPos;
	vec2 m_OlderPos;
	bool m_DoorHit;

private:
	bool StartConversation(CPlayer* pTarget);
};

#endif
