/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseData.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Guilds/GuildData.h>

CGS* CGuildHouseData::GS() const { return (CGS*)Instance::GetServer()->GameServer(m_WorldID); }

CGuildHouseData::~CGuildHouseData()
{
	delete m_pDecorations;
	delete m_pDoors;
}

void CGuildHouseData::TextUpdate(int LifeTime)
{
	// Check if the last tick text update is greater than the current server tick
	if(m_LastTickTextUpdated > Server()->Tick())
		return;

	// Set the initial value of the variable "Name" to "HOUSE"
	std::string Name = "GUILD HOUSE";

	// Check if the object has an owner
	if(IsPurchased())
	{
		// If it has an owner, update the value of "Name" to the player's name
		Name = m_pGuild->GetName();
	}

	// Create a text object with the given parameters
	if(GS()->CreateText(nullptr, false, m_TextPos, {}, LifeTime - 5, Name.c_str()))
	{
		// Update the value of "m_LastTickTextUpdated" to the current server tick plus the lifetime of the text object
		m_LastTickTextUpdated = Server()->Tick() + LifeTime;
	}
}

void CGuildHouseData::UpdateGuild(CGuildData* pGuild)
{
	m_pGuild = pGuild;
	if(m_pGuild)
	{
		m_pGuild->m_pHouse = this;
	}
}
