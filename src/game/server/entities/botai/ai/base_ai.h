#ifndef GAME_SERVER_ENTITIES_AI_BASE_AI_H
#define GAME_SERVER_ENTITIES_AI_BASE_AI_H

class CGS;
class CPlayer;
class CPlayerBot;
class CCharacterBotAI;

enum class TargetType
{
	EMPTY,
	ACTIVE,
	LOST
};

// target system
class CTargetAI
{
	int m_TargetID { -1 };
	bool m_IsCollised {};
	int m_Aggression {};
	TargetType m_Type {};
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
			m_Aggression = 0;
			m_IsCollised = false;
			m_Type = TargetType::EMPTY;
		}
	}

	void Tick()
	{
		if(m_pCharacter)
		{
			if(m_Type == TargetType::LOST && m_Aggression)
			{
				m_Aggression--;
				if(!m_Aggression)
					Reset();
			}
		}
	}
	TargetType GetType() const { return m_Type; }
	int GetCID() const { return m_TargetID; }
	int GetAggresion() const { return m_Aggression; }
	void Set(int ClientID, int Aggression)
	{
		if(m_pCharacter && ClientID >= 0 && ClientID < MAX_CLIENTS)
		{
			m_TargetID = ClientID;
			m_Aggression = Aggression;
			m_Type = TargetType::ACTIVE;
		}
	}

	bool SetType(TargetType TargetType)
	{
		if(m_pCharacter && m_Type != TargetType)
		{
			m_Type = TargetType;
			return true;
		}

		return false;
	}

	bool IsEmpty() const { return m_TargetID <= -1; }
	bool IsCollised() const { return m_IsCollised; }
	void UpdateCollised(bool Collised) { m_IsCollised = Collised; }
};

class CBaseAI
{
public:
	CBaseAI(CPlayerBot* pPlayer, CCharacterBotAI* pCharacter);
	virtual ~CBaseAI() {}

	virtual bool IsConversational() { return false; }

	virtual void OnSpawn() {}
	virtual void OnTakeDamage(int Dmg, int From, int Weapon) {}
	virtual void OnDie(int Killer, int Weapon) {}
	virtual void OnRewardPlayer(CPlayer* pForPlayer, vec2 Force) const {}
	virtual void OnHandleTunning(CTuningParams* pTuning) {}
	virtual void OnGiveRandomEffect(int ClientID) {}
	virtual void OnTargetRules(float Radius) {}
	virtual void OnSnapDDNetCharacter(int SnappingClient, CNetObj_DDNetCharacter* pDDNetCharacter) {};
	virtual void Process() = 0;

	CTargetAI* GetTarget() { return &m_Target; }

protected:
	int m_ClientID;
	vec2 m_SpawnPoint;
	CPlayerBot* m_pPlayer;
	CCharacterBotAI* m_pCharacter;
	CTargetAI m_Target;

	IServer* Server() const;
	CGS* GS() const;

	void SelectEmoteAtRandomInterval(int EmotionStyle) const;

	CPlayer* SearchPlayerCondition(float Distance, const std::function<bool(CPlayer*)>& Condition);
	CPlayerBot* SearchPlayerBotCondition(float Distance, const std::function<bool(CPlayerBot*)>& Condition);
};

#endif
