/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITY_H
#define GAME_SERVER_ENTITY_H

#include "alloc.h"
#include "gameworld.h"

enum class ESnappingPriority
{
	None = 0,
	Lower,
	High,
};

class CPlayer;
class CCharacter;

class CEntity
{
	MACRO_ALLOC_HEAP()

private:
	friend class CGameWorld;
	CGameWorld* m_pGameWorld {};
	CEntity* m_pPrevTypeEntity {};
	CEntity* m_pNextTypeEntity {};
	int m_ID {};
	int m_ObjType {};
	bool m_MarkedForDestroy {};
	bool m_HasPlayersInView {};
	std::unordered_map<int, std::vector<int>> m_vGroupIds {};

protected:
	vec2 m_Pos {};
	vec2 m_PosTo {};
	int m_ClientID {};
	float m_Radius {};
	ESnappingPriority m_NextCheckSnappingPriority {};

	int GetID() const { return m_ID; }
	std::vector<int>* FindSnappingGroupIds(int GroupID)
	{
		auto it = m_vGroupIds.find(GroupID);
		return it != m_vGroupIds.end() ? &it->second : nullptr;
	}
	void AddSnappingGroupIds(int GroupID, int NumIds);
	void RemoveSnappingGroupIds(int GroupID);

public:
	CEntity(CGameWorld* pGameWorld, int Objtype, vec2 Pos, int Radius = 0, int ClientID = -1);
	virtual ~CEntity();

	CGameWorld* GameWorld() const { return m_pGameWorld; }
	class CGS* GS() const { return m_pGameWorld->GS(); }
	class IServer* Server() const { return m_pGameWorld->Server(); }

	CEntity* TypeNext() const { return m_pNextTypeEntity; }
	CEntity* TypePrev() const { return m_pPrevTypeEntity; }
	const vec2& GetPos() const { return m_Pos; }
	const vec2& GetPosTo() const { return m_PosTo; }
	float GetRadius() const { return m_Radius; }
	bool IsMarkedForDestroy() const { return m_MarkedForDestroy; }
	bool HasPlayersInView() const { return m_HasPlayersInView; }
	int GetClientID() const { return m_ClientID; }

	CPlayer* GetOwner() const;
	CCharacter* GetOwnerChar() const;

	void MarkForDestroy() { m_MarkedForDestroy = true; }
	void SetPos(vec2 Pos) { m_Pos = Pos; }
	void SetPosTo(vec2 Pos) { m_PosTo = Pos; }
	void SetClientID(int ClientID) { m_ClientID = ClientID; }

	virtual void Destroy() { delete this; }
	virtual void Reset()  {}
	virtual void Tick()  {}
	virtual void TickDeferred()  {}
	virtual void Snap(int SnappingClient) {}
	virtual void PostSnap() {}

	int NetworkClippedByPriority(int SnappingClient, ESnappingPriority Priority);
	int NetworkClipped(int SnappingClient);
	int NetworkClipped(int SnappingClient, vec2 CheckPos);
	int NetworkClipped(int SnappingClient, vec2 CheckPos, float Radius);

public:
	bool IsValidSnappingState(int SnappingClient) const;
	bool GameLayerClipped(vec2 CheckPos) const;
};

class CFlashingTick
{
	int m_Timer { TIMER_RESET };
	bool m_Flashing {};
	constexpr static int FLASH_THRESHOLD = 150;
	constexpr static int TIMER_RESET = 5;

public:
	CFlashingTick() = default;
	constexpr bool IsFlashing() const { return m_Flashing; }

	void Tick(const int& lifeSpan)
	{
		if(lifeSpan < FLASH_THRESHOLD)
		{
			if(--m_Timer <= 0)
			{
				m_Flashing = !m_Flashing;
				m_Timer = TIMER_RESET;
			}
		}
		else if(m_Flashing || m_Timer != TIMER_RESET)
		{
			m_Flashing = false;
			m_Timer = TIMER_RESET;
		}
	}
};

template <typename T>
class Interpolation
{
	bool m_Started {};
	bool m_Active {};
	bool m_Reversed {};
	T m_InitialValue {};
	T m_TargetValue {};
	int m_DurationTicks {};
	int m_ElapsedTicks {};
	int m_StartTick {};
	std::function<T(float)> m_InterpolationFunction;

public:
	Interpolation() = default;

	void Init(T initialValue, T targetValue, int durationTicks, std::function<T(float)> interpolationFunction)
	{
		m_InitialValue = initialValue;
		m_TargetValue = targetValue;
		m_DurationTicks = durationTicks;
		m_InterpolationFunction = interpolationFunction;
	}

	void Start(int currentTick)
	{
		if(m_Active)
			return;

		m_Started = true;
		m_Active = true;
		m_Reversed = false;
		m_StartTick = currentTick;
		m_ElapsedTicks = 0;
	}

	void Reverse(int currentTick)
	{
		if(m_Active)
			return;

		m_Started = true;
		m_Active = true;
		m_Reversed = !m_Reversed;
		std::swap(m_InitialValue, m_TargetValue);
		m_StartTick = currentTick;
		m_ElapsedTicks = 0;
	}

	void Stop()
	{
		m_Active = false;
		m_Started = false;
	}

	bool IsStarted() const { return m_Started; }
	bool IsActive() const { return m_Active; }
	int GetDurationTicks() const { return m_DurationTicks; }

	T GetCurrentValue(int currentTick)
	{
		if(!m_Active)
		{
			return m_Reversed ? m_InitialValue : m_TargetValue;
		}

		m_ElapsedTicks = currentTick - m_StartTick;
		if(m_ElapsedTicks >= m_DurationTicks)
		{
			m_Active = false;
			return m_TargetValue;
		}

		float t = static_cast<float>(m_ElapsedTicks) / m_DurationTicks;
		if(m_Reversed)
			t = 1.0f - t;

		return m_InterpolationFunction(t);
	}
};

#endif