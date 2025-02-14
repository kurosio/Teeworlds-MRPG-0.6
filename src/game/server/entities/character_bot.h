/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CHARACTER_BOT_H
#define GAME_SERVER_ENTITIES_CHARACTER_BOT_H

#include "character.h"
#include "ai_core/base_ai.h"

class CCharacterBotAI : public CCharacter
{
	MACRO_ALLOC_POOL_ID()

	std::unique_ptr<CBaseAI> m_pAI{};
	CPlayerBot* m_pBotPlayer{};

	int m_MoveTick{};
	int m_PrevDirection{};
	vec2 m_PrevPos{};
	vec2 m_DieForce {};
	std::optional<int> m_ForcedActiveWeapon {};
	ska::unordered_set< int > m_aListDmgPlayers{};

public:
	CCharacterBotAI(CGameWorld* pWorld);

	CBaseAI* AI() const { return m_pAI.get(); }
	void SetForcedWeapon(int WeaponID);
	void ClearForcedWeapon();

private:
	bool Spawn(CPlayer *pPlayer, vec2 Pos) override;
	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;
	void GiveRandomEffects(int ClientID) override;
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon) override;
	void Die(int Killer, int Weapon) override;
	void HandleTuning() override;
	void ProcessBot();

public:
	void SelectWeaponAtRandomInterval();
	void SelectEmoteAtRandomInterval();

	bool GiveWeapon(int Weapon, int GiveAmmo) override;

	bool IsAllowedPVP(int FromID) const override;
	void UpdateTarget(float Radius) const;
	void SetAim(vec2 Dir);

	ska::unordered_set<int>& GetListDmgPlayers() { return m_aListDmgPlayers; }

	void Move();
	void Fire();
};

#endif
