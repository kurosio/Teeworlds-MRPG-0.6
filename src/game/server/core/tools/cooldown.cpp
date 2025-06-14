#include "cooldown.h"

#include <game/server/gamecontext.h>
#include <generated/protocol.h>

void CCooldown::Init(int ClientID)
{
	m_ClientID = ClientID;
}

void CCooldown::Start(int Ticks, std::string_view Name, CCooldownCallback fnCallback)
{
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || m_Active || Ticks <= 0)
		return;

	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	auto* pPlayer = pGS->GetPlayer(m_ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	m_Name = Name;
	m_Callback = std::move(fnCallback);
	m_Active = true;
	m_Interrupted = false;
	m_Pos = pPlayer->m_ViewPos;
	m_StartedTick = Ticks;
	m_Tick = Ticks;

	pGS->CreatePlayerSpawn(m_Pos, CmaskOne(m_ClientID));
	pPlayer->GetCharacter()->SetEmote(EMOTE_BLINK, (m_Tick / SERVER_TICK_SPEED), true);
}

void CCooldown::Reset()
{
	m_Tick = 0;
	m_StartedTick = 0;
	m_Callback = nullptr;
	m_Active = false;
	m_Pos = {};
	m_Interrupted = false;
	m_Name.clear();
}

void CCooldown::Tick()
{
	if(!m_Active)
		return;

	// check valid
	const auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	auto* pPlayer = pGS->GetPlayer(m_ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		Reset();
		return;
	}

	// interrupted
	if(m_Interrupted)
	{
		BroadcastCooldownInfo("< Interrupted >");
		Reset();
		return;
	}

	// update tick
	if(m_Tick-- <= 0)
	{
		BroadcastCooldownInfo();
		if(m_Callback)
			m_Callback();

		Reset();
		return;
	}

	// check player moving and send broadcast info
	auto* pServer = Instance::Server();
	if(pServer->Tick() % (pServer->TickSpeed() / 25) == 0)
	{
		if(HasPlayerMoved(pPlayer->GetCharacter()))
		{
			m_Interrupted = true;
			return;
		}
		BroadcastCooldownProgress(pServer);
	}
}

bool CCooldown::HasPlayerMoved(CCharacter* pChar) const
{
	return distance_squared(m_Pos, pChar->GetPos()) > squared(48.f);
}

void CCooldown::BroadcastCooldownInfo(const char* pReason) const
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	pGS->Broadcast(m_ClientID, BroadcastPriority::VeryImportant, 50, pReason ? pReason : "\0");
	pGS->CreatePlayerSpawn(m_Pos, CmaskOne(m_ClientID));
}

void CCooldown::BroadcastCooldownProgress(IServer* pServer) const
{
	// initialize variables
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
	const int TickSpeed = pServer->TickSpeed();
	const int Seconds = m_Tick / TickSpeed;
	const int Microseconds = (m_Tick % TickSpeed * 100) / TickSpeed;
	const float currentProgress = translate_to_percent(m_StartedTick, m_Tick);

	// send information
	char aTimeFormat[32];
	str_format(aTimeFormat, sizeof(aTimeFormat), "%d.%.2ds", Seconds, Microseconds);
	std::string progressBar = mystd::string::progressBar(100, static_cast<int>(currentProgress), 10, "\u25B0", "\u25B1");
	pGS->Broadcast(m_ClientID, BroadcastPriority::VeryImportant, 10, "{}\n< {} > {} - Action", m_Name, aTimeFormat, progressBar);
}