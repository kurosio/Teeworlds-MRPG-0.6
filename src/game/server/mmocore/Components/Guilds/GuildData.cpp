/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildData.h"

#include <game/server/gamecontext.h>

CGS* CGuildData::GS() const
{
	/*
	CGuildData* pGuild;

	// members
	pGuild->GetMembers()->Join(2); // TODO: BY STATUS
	pGuild->GetMembers()->Kick(2); // TODO: BY STATUS
	for(auto& p : pGuild->GetMembers()->GetContainer())
	{
		p->GetAccountID();
		p->GetDeposit();
		p->GetRankID();
	}

	// rank's
	pGuild->GetRanks()->Add("Chucka");
	pGuild->GetRanks()->Get("Chucka");
	pGuild->GetRanks()->Remove("Chucka");
	for(auto& p : pGuild->GetRanks()->GetContainer())
	{
		p->GetName();
		p->ChangeAccess(ACCESS_NO);
		p->ChangeName("PornoHub")
	}

	// bank
	pGuild->GetBank()->Add(100, );
	pGuild->GetBank()->Take(100, );
	pGuild->GetBank()->Get();

	// history
	pGuild->GetHistory()->Add("%s changes access for rank '{STR}'", "Popa", "Co-leader");
	for(auto& p : pGuild->GetHistory()->GetLogs())
	{
		p.m_Log;
		p.m_Time;
	}

	// house's
	CGuildHouseData* pHouse = pGuild->GetHouse();
	for(auto& [DoorID, pDoor] : pHouse->GetDoors()->GetContainer())
	{
		pDoor->Close();
		pDoor->Open();
		pDoor->IsClosed();
		pDoor->GetName();
		pDoor->GetPos();
	}
	// TODO: ADD COMING
	*/

	return (CGS*)Instance::GetServer()->GameServer(m_pHouse != nullptr ? m_pHouse->GetWorldID() : MAIN_WORLD_ID);
}

CGuildData::~CGuildData()
{
	delete m_pHistory;
	delete m_pRanks;
	delete m_pBank;
}

void CGuildData::SetHouse(CGuildHouseData* pHouse)
{
	m_pHouse = pHouse;
}

void CGuildData::AddExperience(int Experience)
{
}
