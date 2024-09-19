/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_BOTAI_CONTROLLER_HELPER_H
#define GAME_SERVER_BOTAI_CONTROLLER_HELPER_H

class CCharacterBotAI;

enum class TARGET_TYPE
{
	EMPTY,
	ACTIVE,
	LOST
};

// target system
class CTargetSystem
{
	int m_TargetID { -1 };
	bool m_IsCollised {};
	int m_Aggression {};
	TARGET_TYPE m_Type {};
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
			m_Type = TARGET_TYPE::EMPTY;
		}
	}

	void Tick()
	{
		if(m_pCharacter)
		{
			if(m_Type == TARGET_TYPE::LOST && m_Aggression)
			{
				m_Aggression--;
				if(!m_Aggression)
					Reset();
			}
		}
	}
	TARGET_TYPE GetType() const { return m_Type; }
	int GetCID() const { return m_TargetID; }
	int GetAggresion() const { return m_Aggression; }
	void Set(int ClientID, int Aggression)
	{
		if(m_pCharacter && ClientID >= 0 && ClientID < MAX_CLIENTS)
		{
			m_TargetID = ClientID;
			m_Aggression = Aggression;
			m_Type = TARGET_TYPE::ACTIVE;
		}
	}

	bool SetType(TARGET_TYPE TargetType)
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

class CAIController
{
public:
	CAIController(CCharacterBotAI* pCharacter);
	~CAIController(){}

	CCharacterBotAI* GetCharacter() const { return m_pCharacter; };
	CTargetSystem* GetTarget() { return &m_Target; }

private:
	CCharacterBotAI* m_pCharacter;
	CTargetSystem m_Target;
};

#endif
