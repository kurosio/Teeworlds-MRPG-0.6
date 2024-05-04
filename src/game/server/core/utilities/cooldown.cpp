#include "cooldown.h"

#include <game/server/gamecontext.h>
#include <generated/protocol.h>

#include <utility>

void CCooldown::Start(int Time, std::string Name, CCooldownCallback Callback)
{
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || m_IsCooldownActive)
		return;

	CGS* pGS = (CGS*)(Instance::GameServerPlayer(m_ClientID));
	if(CPlayer* pPlayer = pGS->GetPlayer(m_ClientID, true, true))
	{
		m_StartPos = pPlayer->m_ViewPos;
		m_StartTimer = Time;
		m_Timer = Time;
		m_Callback = std::move(Callback);
		m_IsCooldownActive = true;
		m_Interrupted = false;
		m_Name = std::move(Name);

		pGS->CreatePlayerSpawn(m_StartPos, CmaskOne(m_ClientID));
		pPlayer->GetCharacter()->SetEmote(EMOTE_BLINK, m_Timer, true);
	}
}

void CCooldown::Reset()
{
	m_Timer = 0;
	m_StartTimer = 0;
	m_Callback = nullptr;
	m_IsCooldownActive = false;
	m_StartPos = {};
	m_Interrupted = false;
}

void CCooldown::Handler()
{
	// Check if the cooldown is active
	if(!m_IsCooldownActive)
	{
		return;
	}

	// Get server instance and player data
	CGS* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	CPlayer* pPlayer = pGS->GetPlayer(m_ClientID, true, true);
	if(!pPlayer)
	{
		Reset();
		return;
	}

	// If timer is not set, end the cooldown and execute the callback function
	if(!m_Timer)
	{
		m_IsCooldownActive = false;
		pGS->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 50, "\0");
		pGS->CreatePlayerSpawn(m_StartPos, CmaskOne(m_ClientID));
		m_Callback();
		return;
	}

	// If interrupted, end the cooldown and broadcast interruption message
	if(m_Interrupted)
	{
		m_IsCooldownActive = false;
		pGS->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 50, "< Interrupted >");
		return;
	}

	// Check if a certain amount of ticks has passed
	IServer* pServer = Instance::Server();
	if(pServer->Tick() % (pServer->TickSpeed() / 25) == 0)
	{
		// Check if player has moved too far from the starting position
		if(distance(m_StartPos, pPlayer->m_ViewPos) > 48.f)
		{
			m_Interrupted = true;
			return;
		}

		// Format the time remaining in SS:MSMS format
		char aTimeformat[32];
		int Seconds = m_Timer / pServer->TickSpeed();
		int Microseconds = (m_Timer - Seconds * pServer->TickSpeed());
		str_format(aTimeformat, sizeof(aTimeformat), "%d.%.2ds", Seconds, Microseconds);

		// Format a progress bar to show the remaining time as a percentage
		float Current = translate_to_percent(m_StartTimer, m_Timer);
		std::string ProgressBar = Tools::String::progressBar(100, (int)Current, 10, "\u25B0", "\u25B1");

		// Broadcast the time remaining and progress bar
		pGS->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 10, "{}\n< {} > {} - Action", m_Name.c_str(), aTimeformat, ProgressBar.c_str());
	}

	// Decrease the timer
	m_Timer--;
}
