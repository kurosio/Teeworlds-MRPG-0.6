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
	m_Data.Reset(Server()->Tick() + (TickSpeed * 60 * IntervalMinutes));
}

void CMiniEventsManager::ScheduleQuickRoll(int MinSeconds, int MaxSeconds)
{
	const auto TickSpeed = Server()->TickSpeed();
	const auto MinDelay = maximum(5, MinSeconds);
	const auto MaxDelay = maximum(MinDelay, MaxSeconds);
	const auto DelaySeconds = MinDelay + rand() % (MaxDelay - MinDelay + 1);
	m_Data.m_Type = MiniEventType::None;
	m_Data.m_BonusPercent = 0;
	m_Data.m_StartTick = 0;
	m_Data.m_EndTick = 0;
	m_Data.m_NextRollTick = Server()->Tick() + (TickSpeed * DelaySeconds);
}

void CMiniEventsManager::OnInitWorld(const std::string&)
{
	ScheduleNextRoll();
}

void CMiniEventsManager::OnTick()
{
	if(!g_Config.m_SvMiniEventsEnabled || GS()->HasWorldFlag(WORLD_FLAG_NO_MULTIPLIER))
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
	m_Data.m_ChainLevel = maximum(0, m_Data.m_ChainLevel);

	GS()->ChatWorld(GS()->GetWorldID(), "", mystd::aesthetic::wrapLinePillar(8).c_str());
	GS()->ChatWorld(GS()->GetWorldID(), "", "- Event Started! {}", m_Data.m_ChainLevel > 0 ? "(Chain Rush)" : "");
	GS()->ChatWorld(GS()->GetWorldID(), "", "- {} bonus: +{}%", GetMiniEventName(m_Data.m_Type), GetBonusPercent(m_Data.m_Type));
	if(m_Data.m_ChainLevel > 0)
	{
		GS()->ChatWorld(GS()->GetWorldID(), "", "- Chain level {} grants +{}%.", m_Data.m_ChainLevel,
			m_Data.m_ChainLevel * g_Config.m_SvMiniEventsChainBonusStepPercent);
	}
	GS()->ChatWorld(GS()->GetWorldID(), "", "- Duration: {} min.", DurationMinutes);
	GS()->ChatWorld(GS()->GetWorldID(), "", mystd::aesthetic::wrapLinePillar(8).c_str());

}

void CMiniEventsManager::StopMiniEvent()
{
	GS()->ChatWorld(GS()->GetWorldID(), "", "Event ended: {}.", GetMiniEventName(m_Data.m_Type));

	const auto MaxChainLevel = maximum(0, g_Config.m_SvMiniEventsChainMax);
	const auto ChainChance = clamp(g_Config.m_SvMiniEventsChainChancePercent, 0, 100);
	const auto CanContinueChain = m_Data.m_ChainLevel < MaxChainLevel;
	const auto WantsContinue = (rand() % 100) < ChainChance;
	if(CanContinueChain && WantsContinue)
	{
		++m_Data.m_ChainLevel;
		GS()->ChatWorld(GS()->GetWorldID(), "", mystd::aesthetic::wrapLinePillar(8).c_str());
		GS()->ChatWorld(GS()->GetWorldID(), "", "A chain anomaly appears!", m_Data.m_ChainLevel);
		GS()->ChatWorld(GS()->GetWorldID(), "", "Next mini-event in a few seconds (Lv.{})!", m_Data.m_ChainLevel);
		GS()->ChatWorld(GS()->GetWorldID(), "", mystd::aesthetic::wrapLinePillar(8).c_str());
		ScheduleQuickRoll(20, 80);
		return;
	}

	ScheduleNextRoll();
}

void CMiniEventsManager::FormatBroadcastLine(std::string& Result) const
{
	if(!IsActive())
		return;

	const auto RemainingTick = maximum(0, m_Data.m_EndTick - Server()->Tick());
	const auto RemainingSec = RemainingTick / Server()->TickSpeed();
	const auto BonusPercent = GetBonusPercent(m_Data.m_Type);
	Result += "\n";
	Result += fmt_default("{} +{}% | {}s", GetMiniEventName(m_Data.m_Type), BonusPercent, RemainingSec);
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

int CMiniEventsManager::GetBonusPercent(MiniEventType Type) const
{
	if(!IsActive() || m_Data.m_Type != Type)
		return 0;

	auto BonusPercent = m_Data.m_BonusPercent;
	BonusPercent += m_Data.m_ChainLevel * g_Config.m_SvMiniEventsChainBonusStepPercent;
	return BonusPercent;
}
