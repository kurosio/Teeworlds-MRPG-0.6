#include "mini_events_manager.h"

#include <game/server/gamecontext.h>

namespace
{
	constexpr std::array<MiniEventType, 7> gs_aMiniEvents = {
		MiniEventType::MiningDrop,
		MiniEventType::FarmerDrop,
		MiniEventType::FishingDrop,
		MiniEventType::MobDrop,
		MiniEventType::GoldGain,
		MiniEventType::ExpGain,
		MiniEventType::SkillPointDrop,
	};
}

void CMiniEventsManager::ScheduleNextRoll()
{
	const auto TickSpeed = Server()->TickSpeed();
	const auto MinInterval = maximum(1, g_Config.m_SvMiniEventsIntervalMinMinutes);
	const auto MaxInterval = maximum(MinInterval, g_Config.m_SvMiniEventsIntervalMaxMinutes);
	const int IntervalMinutes = MinInterval + rand() % (MaxInterval - MinInterval + 1);
	m_Data.Reset(Server()->Tick() + (TickSpeed * IntervalMinutes));
}

void CMiniEventsManager::OnInitWorld(const std::string&)
{
	ScheduleNextRoll();
}

void CMiniEventsManager::OnTick()
{
	if(!g_Config.m_SvMiniEventsEnabled)
		return;

	const auto Tick = Server()->Tick();
	if(m_Data.m_Type != MiniEventType::None && Tick >= m_Data.m_EndTick)
	{
		StopMiniEvent();
		return;
	}

	if(m_Data.m_NextRollTick > 0 && Tick >= m_Data.m_NextRollTick)
	{
		StartRandomMiniEvent();
	}
}

bool CMiniEventsManager::IsActive() const
{
	return m_Data.IsActive(Server()->Tick()) && g_Config.m_SvMiniEventsEnabled;
}

void CMiniEventsManager::StartRandomMiniEvent()
{
	const auto Tick = Server()->Tick();
	const auto TickSpeed = Server()->TickSpeed();
	const auto MinDuration = maximum(2, g_Config.m_SvMiniEventsDurationMinMinutes);
	const auto MaxDuration = maximum(MinDuration, g_Config.m_SvMiniEventsDurationMaxMinutes);
	const auto MinBonus = maximum(1, g_Config.m_SvMiniEventsBonusMinPercent);
	const auto MaxBonus = maximum(MinBonus, g_Config.m_SvMiniEventsBonusMaxPercent);
	const auto DurationMinutes = MinDuration + rand() % (MaxDuration - MinDuration + 1);

	m_Data.m_Type = gs_aMiniEvents[rand() % gs_aMiniEvents.size()];
	m_Data.m_BonusPercent = MinBonus + rand() % (MaxBonus - MinBonus + 1);
	m_Data.m_StartTick = Tick;
	m_Data.m_EndTick = Tick + (TickSpeed * 60 * DurationMinutes);
	m_Data.m_NextRollTick = 0;

	GS()->ChatWorld(GS()->GetWorldID(), "", mystd::aesthetic::wrapLinePillar(8).c_str());
	GS()->ChatWorld(GS()->GetWorldID(), "", "- Event Started!");
	GS()->ChatWorld(GS()->GetWorldID(), "", "- {} bonus: +{}%", GetMiniEventName(m_Data.m_Type), m_Data.m_BonusPercent);
	GS()->ChatWorld(GS()->GetWorldID(), "", "- Duration: {} min.", DurationMinutes);
	GS()->ChatWorld(GS()->GetWorldID(), "", "- Don't miss your chance!");
	GS()->ChatWorld(GS()->GetWorldID(), "", mystd::aesthetic::wrapLinePillar(8).c_str());

}

void CMiniEventsManager::StopMiniEvent()
{
	GS()->ChatWorld(GS()->GetWorldID(), "", "Event ended: {}.", GetMiniEventName(m_Data.m_Type));
	ScheduleNextRoll();
}

void CMiniEventsManager::FormatBroadcastLine(std::string& Result) const
{
	if(!IsActive())
		return;

	const auto RemainingTick = maximum(0, m_Data.m_EndTick - Server()->Tick());
	const auto RemainingSec = RemainingTick / Server()->TickSpeed();
	Result += "\n";
	Result += fmt_default("{} +{}% | {}s", GetMiniEventName(m_Data.m_Type), m_Data.m_BonusPercent, RemainingSec);
}

const char* CMiniEventsManager::GetMiniEventName(MiniEventType Type)
{
	switch(Type)
	{
		case MiniEventType::MiningDrop: return "Mining Drop";
		case MiniEventType::FarmerDrop: return "Farmer Drop";
		case MiniEventType::FishingDrop: return "Fishing Drop";
		case MiniEventType::MobDrop: return "Mob Drop";
		case MiniEventType::GoldGain: return "Gold Gain";
		case MiniEventType::ExpGain: return "Exp Gain";
		case MiniEventType::SkillPointDrop: return "SP Drop";
		default: return "Unknown";
	}
}
