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
	void ApplyBonus(MiniEventType Type, int& Value) const;
	void FormatBroadcastLine(std::string& Result) const;

private:
	void ScheduleNextRoll();
	void StartRandomMiniEvent();
	void StopMiniEvent();

	static const char* GetMiniEventName(MiniEventType Type);
};

#endif
