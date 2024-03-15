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
	ska::unordered_map< int, vec2 > m_aPath;
	ska::unordered_set< int > m_aListDmgPlayers;

public:
	CCharacterBotAI(CGameWorld* pWorld);
	~CCharacterBotAI() override;

	CAIController* AI() const { return m_pAI; }

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

	void RewardPlayer(CPlayer *pPlayer, vec2 ForceDies) const;

	/*
	 * Changing weapons randomly, only for those that have in equipment
	 */
	void ChangeWeapons();


	void EmotesAction(int EmotionStyle);
	void SetAim(vec2 Dir);

	bool SearchPlayersForDialogue();
	void HandleBot();
	void EngineNPC();
	void EngineMobs();
	void EngineEidolons();
	void EngineQuestMob();
	void EngineQuestNPC();
	void HandleTuning() override;
	void BehaviorTick();

	CPlayer *SearchPlayer(float Distance) const;
    CPlayer *SearchTankPlayer(float Distance);
	CPlayerBot* SearchMob(float Distance) const;

	void Move();
	void Fire();

	// Bots functions
	bool FunctionNurseNPC();
	bool FunctionGuardianNPC();
	bool BaseFunctionNPC();
};

#endif
