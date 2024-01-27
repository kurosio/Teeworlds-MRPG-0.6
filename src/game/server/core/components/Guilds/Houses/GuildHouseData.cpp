/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseData.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Guilds/GuildData.h>

CGS* CGuildHouseData::GS() const { return (CGS*)Instance::GetServer()->GameServer(m_WorldID); }

CGuildHouseData::~CGuildHouseData()
{
	delete m_pDecorations;
	delete m_pDoors;
}

void CGuildHouseData::InitProperties(std::string&& Properties)
{
	// Parse the JSON string
	Tools::Json::parseFromString(Properties, [this](nlohmann::json& pJson)
	{
		if(pJson.find("pos") != pJson.end())
		{
			auto pHousePosData = pJson["pos"];
			m_Position.x = (float)pHousePosData.value("x", 0);
			m_Position.y = (float)pHousePosData.value("y", 0);
			m_Radius = (float)pHousePosData.value("radius", 300);

			// Create a new instance of CGuildHouseDecorationManager and assign it to m_pDecorations
			// The CGuildHouseDecorationManager will handle all the decorations for the guild house.
			m_pDecorations = new CGuildHouseDecorationManager(this);
		}

		if(pJson.find("text_pos") != pJson.end())
		{
			auto pTextPosData = pJson["text_pos"];
			m_TextPosition.x = (float)pTextPosData.value("x", 0);
			m_TextPosition.y = (float)pTextPosData.value("y", 0);
		}

		if(pJson.find("doors") != pJson.end())
		{
			// Create a new instance of CGuildHouseDoorsController and assign it to m_pDoors
			// The CGuildHouseDecorationManager will handle all the doors for the guild house.
			m_pDoors = new CGuildHouseDoorsController(this);

			auto pDoorsData = pJson["doors"];
			for(const auto& pDoor : pDoorsData)
			{
				// Check if the door name is not empty
				std::string Doorname = pDoor.value("name", "");
				vec2 Position = vec2(pDoor.value("x", 0), pDoor.value("y", 0));
				m_pDoors->AddDoor(Doorname.c_str(), Position);
			}
		}
	});

	// Asserts
	dbg_assert(m_pDecorations != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoors != nullptr, "The house doors manager is null");
}

void CGuildHouseData::TextUpdate(int LifeTime)
{
	// Check if the last tick text update is greater than the current server tick
	if(m_LastTickTextUpdated > Server()->Tick())
		return;

	// Set the initial value of the variable "Name" to "HOUSE"
	std::string Name = "FREE GUILD HOUSE";

	// Check if the object has an owner
	if(IsPurchased())
	{
		// If it has an owner, update the value of "Name" to the player's name
		Name = m_pGuild->GetName();
	}

	// Create a text object with the given parameters
	if(GS()->CreateText(nullptr, false, m_TextPosition, {}, LifeTime - 5, Name.c_str()))
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
