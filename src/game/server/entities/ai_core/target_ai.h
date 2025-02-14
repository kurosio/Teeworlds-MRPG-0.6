#ifndef GAME_SERVER_ENTITIES_AI_CORE_TARGET_AI_H
#define GAME_SERVER_ENTITIES_AI_CORE_TARGET_AI_H

class CGS;
class CPlayer;
class CPlayerBot;
class CCharacterBotAI;

enum class TargetType
{
	Empty,
	Active,
	Lost
};

// target system
class CTargetAI
{
	int m_TargetID { -1 };
	TargetType m_Type { TargetType::Empty };
	bool m_IsCollised {};
	int m_Aggression {};
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
			m_Type = TargetType::Empty;
		}
	}

	void Tick()
	{
		if(m_pCharacter)
		{
			// is lost decrease agression
			if(m_Type == TargetType::Lost && m_Aggression)
			{
				m_Aggression--;
				if(!m_Aggression)
					Reset();
			}
		}
	}

	TargetType GetType() const
	{
		return m_Type;
	}

	int GetCID() const
	{
		return m_TargetID;
	}

	int GetAggression() const
	{
		return m_Aggression;
	}

	void Set(int ClientID, int Aggression)
	{
		if(m_pCharacter && ClientID >= 0 && ClientID < MAX_CLIENTS)
		{
			m_TargetID = ClientID;
			m_Aggression = Aggression;
			m_Type = TargetType::Active;
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

	bool IsEmpty() const
	{
		return m_TargetID <= -1;
	}

	bool IsCollided() const
	{
		return m_IsCollised;
	}

	void UpdateCollised(bool Collised)
	{
		m_IsCollised = Collised;
	}
};

#endif
