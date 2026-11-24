#include "cooldown.h"

#include <game/server/gamecontext.h>
#include <generated/protocol.h>

void CCooldown::Init(int ClientID)
{
	// Reset any previous state before assigning a new owner
	Reset();
	m_ClientID = ClientID;
}

bool CCooldown::IsClientIDValid() const
{
	return m_ClientID >= 0 && m_ClientID < MAX_PLAYERS;
}

CGS* CCooldown::GetGameServer() const
{
	if (!IsClientIDValid())
		return nullptr;

	return static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
}

void CCooldown::Start(int DurationTicks, std::string_view Name, CCooldownCallback fnCallback)
{
	if (!IsClientIDValid() || m_Active || DurationTicks <= 0)
		return;

	auto* pGS = GetGameServer();
	if (!pGS)
		return;

	auto* pPlayer = pGS->GetPlayer(m_ClientID);
	auto* pChar = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if (!pPlayer || !pChar)
		return;

	m_Name.assign(Name);
	m_Callback = std::move(fnCallback);
	m_Active = true;
	m_StartPos = pChar->GetPos();
	m_StartedTick = DurationTicks;
	m_Tick = DurationTicks;

	pGS->CreatePlayerSpawn(m_StartPos, CmaskOne(m_ClientID));

	// Round up to at least one second so short cooldowns still show an emote
	const int EmoteDurationSeconds = std::max(1, (DurationTicks + SERVER_TICK_SPEED - 1) / SERVER_TICK_SPEED);
	pChar->SetEmote(EMOTE_BLINK, EmoteDurationSeconds, true);
}

void CCooldown::Reset()
{
	m_Name.clear();
	m_StartPos = {};
	m_Tick = 0;
	m_StartedTick = 0;
	m_Callback = nullptr;
	m_Active = false;
}

void CCooldown::Finish()
{
	// Broadcast completion before clearing visual state
	BroadcastCooldownInfo();

	// Move callback out before reset so the callback may safely start a new cooldown
	auto Callback = std::move(m_Callback);
	Reset();

	if (Callback)
		Callback();
}

void CCooldown::Interrupt(const char* pReason)
{
	BroadcastCooldownInfo(pReason);
	Reset();
}

void CCooldown::Tick()
{
	if (!m_Active)
		return;

	auto* pServer = Instance::Server();
	auto* pGS = GetGameServer();
	if (!pServer || !pGS)
	{
		Reset();
		return;
	}

	auto* pPlayer = pGS->GetPlayer(m_ClientID);
	auto* pChar = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if (!pPlayer || !pChar)
	{
		Reset();
		return;
	}

	// Finish the cooldown as soon as no ticks remain
	if (m_Tick <= 0)
	{
		Finish();
		return;
	}

	// Match the original timing behavior: decrease first, then update UI
	--m_Tick;

	// Update progress and interruption checks at a fixed rate
	const int UpdateInterval = std::max(1, pServer->TickSpeed() / kProgressUpdatesPerSecond);
	if (pServer->Tick() % UpdateInterval != 0)
		return;

	if (HasPlayerMoved(pChar))
	{
		Interrupt("< Interrupted >");
		return;
	}

	BroadcastCooldownProgress(pServer);
}

bool CCooldown::HasPlayerMoved(const CCharacter* pChar) const
{
	if (!pChar)
		return false;

	return distance_squared(m_StartPos, pChar->GetPos()) > squared(kInterruptDistance);
}

void CCooldown::BroadcastCooldownInfo(const char* pMessage) const
{
	auto* pGS = GetGameServer();
	if (!pGS)
		return;

	pGS->Broadcast(m_ClientID, BroadcastPriority::VeryImportant, 50, pMessage);
	pGS->CreatePlayerSpawn(m_StartPos, CmaskOne(m_ClientID));
}

void CCooldown::BroadcastCooldownProgress(IServer* pServer) const
{
	if (!pServer)
		return;

	auto* pGS = GetGameServer();
	if (!pGS)
		return;

	const int TickSpeed = std::max(1, pServer->TickSpeed());
	const int Seconds = m_Tick / TickSpeed;
	const int Hundredths = ((m_Tick % TickSpeed) * 100) / TickSpeed;

	const int ProgressPercent = std::clamp(
		static_cast<int>(translate_to_percent(m_StartedTick, m_Tick)), 0, 100);

	char aTimeFormat[32];
	str_format(aTimeFormat, sizeof(aTimeFormat), "%d.%.2ds", Seconds, Hundredths);
	const std::string ProgressBar = mystd::string::progressBar(100, ProgressPercent, kProgressBarSegments, "\u25B0", "\u25B1");
	pGS->Broadcast(m_ClientID, BroadcastPriority::VeryImportant, 10, "{}\n< {~} > {~} - Action", m_Name, aTimeFormat, ProgressBar);
}