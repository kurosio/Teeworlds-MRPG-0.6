#include "cooldown.h"
#include <game/server/gamecontext.h>
#include <generated/protocol.h>

void CCooldown::Start(int Time, std::string Name, CCooldownCallback fnCallback)
{
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || m_IsCooldownActive)
		return;

	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	if(const CPlayer* pPlayer = pGS->GetPlayer(m_ClientID, true, true))
	{
		m_StartPos = pPlayer->m_ViewPos;
		m_StartMousePos = pPlayer->GetCharacter()->GetMousePos();
		m_StartTimer = Time;
		m_Timer = Time;
		m_Callback = std::move(fnCallback);
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
	if(!m_IsCooldownActive)
		return;

	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	CPlayer* pPlayer = pGS->GetPlayer(m_ClientID, true, true);
	if(!pPlayer)
	{
		Reset();
		return;
	}

	if(m_Timer <= 0)
	{
		EndCooldown();
		if(m_Callback) m_Callback();
		return;
	}

	if(m_Interrupted)
	{
		EndCooldown("< Interrupted >");
		return;
	}

	IServer* pServer = Instance::Server();
	if(pServer->Tick() % (pServer->TickSpeed() / 25) == 0)
	{
		if(HasPlayerMoved(pPlayer) || HasMouseMoved(pPlayer))
		{
			m_Interrupted = true;
			return;
		}

		BroadcastCooldown(pServer);
	}

	m_Timer--;
}


void CCooldown::EndCooldown(const char* pMessage)
{
	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);

	m_IsCooldownActive = false;
	pGS->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 50, pMessage);
	pGS->CreatePlayerSpawn(m_StartPos, CmaskOne(m_ClientID));
}

bool CCooldown::HasPlayerMoved(CPlayer* pPlayer) const
{
	return distance(m_StartPos, pPlayer->m_ViewPos) > 48.f;
}

bool CCooldown::HasMouseMoved(CPlayer* pPlayer) const
{
	return pPlayer->GetCharacter() ? distance(m_StartMousePos, pPlayer->GetCharacter()->GetMousePos()) > 48.f : true;
}

void CCooldown::BroadcastCooldown(IServer* pServer) const
{
	const int seconds = m_Timer / pServer->TickSpeed();
	const int microseconds = m_Timer % pServer->TickSpeed();

	char timeFormat[32];
	str_format(timeFormat, sizeof(timeFormat), "%d.%.2ds", seconds, microseconds);

	const float currentProgress = (float)translate_to_percent(m_StartTimer, m_Timer);
	std::string progressBar = Utils::String::progressBar(100, static_cast<int>(currentProgress), 10, "\u25B0", "\u25B1");

	const auto pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
	pGS->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 10, "%s\n< %s > %s - Action", m_Name.c_str(), timeFormat, progressBar.c_str());
}