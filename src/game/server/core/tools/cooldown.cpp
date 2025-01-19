#include "cooldown.h"

#include <game/server/gamecontext.h>
#include <generated/protocol.h>

void CCooldown::Start(int Tick, std::string Name, CCooldownCallback fnCallback)
{
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || m_Active)
		return;

	auto* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	auto* pPlayer = pGS->GetPlayer(m_ClientID, true, true);
	if(!pPlayer)
		return;

	m_Name = std::move(Name);
	m_Callback = std::move(fnCallback);
	m_Active = true;
	m_Interrupted = false;
	m_Pos = pPlayer->m_ViewPos;
	m_StartedTick = Tick;
	m_Tick = Tick;

	pGS->CreatePlayerSpawn(m_Pos, CmaskOne(m_ClientID));
	pPlayer->GetCharacter()->SetEmote(EMOTE_BLINK, m_Tick, true);
}

void CCooldown::Reset()
{
	m_Tick = 0;
	m_StartedTick = 0;
	m_Callback = nullptr;
	m_Active = false;
	m_Pos = {};
	m_Interrupted = false;
}

void CCooldown::Tick()
{
	if(!m_Active)
		return;

	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	auto* pPlayer = pGS->GetPlayer(m_ClientID, true, true);
	if(!pPlayer)
	{
		Reset();
		return;
	}

	if(m_Tick <= 0)
	{
		EndCooldown();
		
		if(m_Callback)
		{
			m_Callback();
		}
		
		return;
	}

	if(m_Interrupted)
	{
		EndCooldown("< Interrupted >");
		return;
	}

	auto* pServer = Instance::Server();
	if(pServer->Tick() % (pServer->TickSpeed() / 25) == 0)
	{
		if(HasPlayerMoved(pPlayer))
		{
			m_Interrupted = true;
			return;
		}

		BroadcastCooldown(pServer);
	}

	m_Tick--;
}


void CCooldown::EndCooldown(const char* pReason)
{
	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);

	m_Active = false;
	pGS->Broadcast(m_ClientID, BroadcastPriority::VeryImportant, 50, pReason);
	pGS->CreatePlayerSpawn(m_Pos, CmaskOne(m_ClientID));
}

bool CCooldown::HasPlayerMoved(CPlayer* pPlayer) const
{
	return distance(m_Pos, pPlayer->m_ViewPos) > 48.f;
}

void CCooldown::BroadcastCooldown(IServer* pServer) const
{
	const int seconds = m_Tick / pServer->TickSpeed();
	const int microseconds = m_Tick % pServer->TickSpeed();

	char timeFormat[32];
	str_format(timeFormat, sizeof(timeFormat), "%d.%.2ds", seconds, microseconds);

	const float currentProgress = (float)translate_to_percent(m_StartedTick, m_Tick);
	std::string progressBar = mystd::string::progressBar(100, static_cast<int>(currentProgress), 10, "\u25B0", "\u25B1");

	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	pGS->Broadcast(m_ClientID, BroadcastPriority::VeryImportant, 10, "{}\n< {} > {} - Action", m_Name, timeFormat, progressBar);
}