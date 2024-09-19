/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_BOTAI_HELPER_H
#define GAME_SERVER_BOTAI_HELPER_H

#include "../character.h"
#include "ai_bot.h"

class CEntityFunctionNurse;
class CCharacterBotAI : public CCharacter
{
	MACRO_ALLOC_POOL_ID()

	class CPlayerBot* m_pBotPlayer;
	class CAIController* m_pAI;

	// bot ai
	int m_MoveTick;
	int m_PrevDirection;
	vec2 m_PrevPos;
	vec2 m_WallPos;
	int m_EmotionsStyle;

	vec2 m_DieForce {};
	std::optional<int> m_ForcedActiveWeapon {};

	ska::unordered_set< int > m_aListDmgPlayers;

public:
	CCharacterBotAI(CGameWorld* pWorld);
	~CCharacterBotAI() override;

	CAIController* AI() const { return m_pAI; }
	void SetForcedWeapon(int WeaponID);
	void ClearForcedWeapon();

private:
	void InitBot();
	bool Spawn(class CPlayer *pPlayer, vec2 Pos) override;
	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;
	void GiveRandomEffects(int ClientID) override;
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon) override;
	void Die(int Killer, int Weapon) override;
	bool GiveWeapon(int Weapon, int GiveAmmo) override;

	void RewardPlayer(CPlayer *pPlayer) const;
	void HandleQuestMobReward(CPlayer* pPlayer) const;
	void HandleMobReward(CPlayer* pPlayer) const;

	/*
	 * Changing weapons randomly, only for those that have in equipment
	 */
	void SelectWeaponAtRandomInterval();
	void SelectEmoteAtRandomInterval(int EmotionStyle);

	void SetAim(vec2 Dir);

	bool SearchPlayersForDialogue();
	void ProcessBotBehavior();

	void ProcessNPC();
	void ProcessGuardianNPC();
	void ProcessMobs();
	void ProcessEidolons();
	void ProcessQuestMob();
	void ProcessQuestNPC();

	void HandleTuning() override;
	void BehaviorTick();

	void UpdateTarget(float Radius);

	CPlayer* SearchPlayerCondition(float Distance, const std::function<bool(CPlayer*)>& Condition) const;
	CPlayerBot* SearchPlayerBotCondition(float Distance, const std::function<bool(CPlayerBot*)>& Condition) const;

	void Move();
	void Fire();

	// Bots functions
	bool FunctionNurseNPC();
	bool BaseFunctionNPC();
};

#endif
