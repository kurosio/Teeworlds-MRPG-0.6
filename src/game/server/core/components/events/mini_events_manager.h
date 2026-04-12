#ifndef GAME_SERVER_CORE_COMPONENTS_EVENTS_MINI_EVENTS_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_EVENTS_MINI_EVENTS_MANAGER_H

#include <game/server/core/mmo_component.h>

enum class MiniEventType : int
{
	None = 0,
	MiningDrop,
	FarmerDrop,
	FishingDrop,
	MobDrop,
	GoldGain,
	ExpGain,
	SkillPointDrop,
};

struct MiniEventData
{
	MiniEventType m_Type {MiniEventType::None};
	int m_BonusPercent {};
	int m_StartTick {};
	int m_EndTick {};
	int m_NextRollTick {};

	bool IsActive(int Tick) const
	{
		return m_Type != MiniEventType::None && Tick >= m_StartTick && Tick < m_EndTick;
	}

	void Reset(int NextRollTick)
	{
		m_Type = MiniEventType::None;
		m_BonusPercent = 0;
		m_StartTick = 0;
		m_EndTick = 0;
		m_NextRollTick = NextRollTick;
	}
};

class CMiniEventsManager : public MmoComponent
{
	MiniEventData m_Data{};

public:
	void OnPostInit() override;
	void OnTick() override;

	bool IsActive() const;

	template <typename T> requires std::is_integral_v<T>
	void ApplyBonus(MiniEventType Type, T* pValue, T* pBonusValue = nullptr) const
	{
		if(!IsActive() || m_Data.m_Type != Type || !pValue || *pValue <= 0)
			return;

		auto Result = maximum((T)1, (T)translate_to_percent_rest(*pValue, (float)m_Data.m_BonusPercent));
		*pValue += Result;

		if(pBonusValue)
			*pBonusValue += Result;
	}
	int GetBonusPercent(MiniEventType Type) const
	{
		if(IsActive() && m_Data.m_Type == Type)
			return m_Data.m_BonusPercent;
		return 0;
	}
	int GetBonusPercent() const { return GetBonusPercent(m_Data.m_Type); }
	void FormatBroadcastLine(std::string& Result) const;

private:
	void ScheduleNextRoll();
	void StartRandomMiniEvent();
	void StopMiniEvent();

	static const char* GetMiniEventName(MiniEventType Type);
};

#endif
