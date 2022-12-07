/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_BOTAI_HELPER_H
#define GAME_SERVER_BOTAI_HELPER_H
#include "../character.h"

enum class TARGET_TYPE
{
	EMPTY,
	ACTIVE,
	LOST
};

class CEntityFunctionNurse;
class CCharacterBotAI : public CCharacter
{
	MACRO_ALLOC_POOL_ID()

	class CPlayerBot* m_pBotPlayer;

	// target system
	class CTargetSystem
	{
		int m_TargetID{-1};
		bool m_TargetCollised{};
		int m_TargetAggression{};
		TARGET_TYPE m_TargetType {};
		CCharacterBotAI* m_pCharacter {};

	public:
		void Init(CCharacterBotAI* pCharacter)
		{
			m_pCharacter = pCharacter;
		}

		void Reset()
		{
			if(m_pCharacter)
			{
				m_TargetID = -1;
				m_TargetAggression = 0;
				m_TargetCollised = false;
				m_TargetType = TARGET_TYPE::EMPTY;
			}
		}

		void Tick()
		{
			if(m_pCharacter)
			{
				if(m_TargetType == TARGET_TYPE::LOST && m_TargetAggression)
				{
					m_TargetAggression--;
					if(!m_TargetAggression)
						Reset();
				}
			}
		}
		int GetCID() const { return m_TargetID; }
		int GetAggresion() const { return m_TargetAggression; }
		void Set(int ClientID, int Aggression)
		{
			if(m_pCharacter && ClientID >= 0 && ClientID < MAX_CLIENTS)
			{
				m_TargetID = ClientID;
				m_TargetAggression = Aggression;
				m_TargetType = TARGET_TYPE::ACTIVE;
			}
		}

		void SetType(TARGET_TYPE TargetType)
		{
			if(m_pCharacter)
			{
				m_TargetType = TargetType;
			}
		}

		bool IsEmpty() const { return m_TargetID <= -1; }
		bool IsCollised() const { return m_TargetCollised; }
		void UpdateCollised(bool Collised) { m_TargetCollised = Collised; }
	} m_Target;


	// bot ai
	bool m_UseHookDissabled;
	int m_MoveTick;
	int m_PrevDirection;
	vec2 m_PrevPos;
	vec2 m_WallPos;
	int m_EmotionsStyle;
	std::deque< int > m_aListDmgPlayers;

public:
	CCharacterBotAI(CGameWorld* pWorld);
	~CCharacterBotAI() override;

	CTargetSystem* GetTarget() { return &m_Target; }

private:
	bool Spawn(class CPlayer *pPlayer, vec2 Pos) override;
	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;
	void GiveRandomEffects(int ClientID) override;
	bool TakeDamage(vec2 Force, int Dmg, int From, int Weapon) override;
	void Die(int Killer, int Weapon) override;
	bool GiveWeapon(int Weapon, int GiveAmmo) override;
	int GetSnapFullID() const override;

	void RewardPlayer(CPlayer *pPlayer, vec2 ForceDies) const;

	/*
	 * Changing weapons randomly, only for those that have in equipment
	 */
	void ChangeWeapons();


	void EmotesAction(int EmotionStyle);
	void SetAim(vec2 Dir);

	bool SearchTalkedPlayer();
	void HandleBot();
	void EngineNPC();
	void EngineMobs();
	void EngineEidolons();
	void EngineQuestMob();
	void HandleTuning() override;
	void BehaviorTick();

	CPlayer *SearchPlayer(float Distance) const;
    CPlayer *SearchTankPlayer(float Distance);
	CPlayerBot* SearchMob(float Distance) const;

	void Move();
	void Fire();

	// Bots functions
	bool FunctionNurseNPC();
	bool BaseFunctionNPC();
};

#endif
