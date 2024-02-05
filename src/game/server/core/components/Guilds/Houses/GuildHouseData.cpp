/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseData.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Guilds/GuildData.h>

CGS* CGuildHouseData::GS() const { return (CGS*)Instance::GetServer()->GameServer(m_WorldID); }

CGuildHouseData::~CGuildHouseData()
{
	delete m_pPlantzones;
	delete m_pDecorations;
	delete m_pDoors;
}

void CGuildHouseData::InitProperties(std::string&& Plantzones, std::string&& Properties)
{
	// Assert important values
	dbg_assert(Properties.length() > 0, "The properties string is empty");

	// Parse the JSON string
	Tools::Json::parseFromString(Properties, [this](nlohmann::json& pJson)
	{
		// Assert for important properties
		dbg_assert(pJson.find("pos") != pJson.end(), "The importal properties value is empty");

		auto pHousePosData = pJson["pos"];
		m_Position.x = (float)pHousePosData.value("x", 0);
		m_Position.y = (float)pHousePosData.value("y", 0);
		m_Radius = (float)pHousePosData.value("radius", 300);

		// Create a new instance of CGuildHouseDecorationManager and assign it to m_pDecorations
		// The CGuildHouseDecorationManager will handle all the decorations for the guild house.
		m_pDecorations = new CGuildHouseDecorationManager(this);

		// Initialize text position
		if(pJson.find("text_pos") != pJson.end())
		{
			auto pTextPosData = pJson["text_pos"];
			m_TextPosition.x = (float)pTextPosData.value("x", 0);
			m_TextPosition.y = (float)pTextPosData.value("y", 0);
		}

		// Initialize doors
		if(pJson.find("doors") != pJson.end())
		{
			// Create a new instance of CGuildHouseDoorManager and assign it to m_pDoors
			// The CGuildHouseDecorationManager will handle all the doors for the guild house.
			m_pDoors = new CGuildHouseDoorManager(this);

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

	// Create a new instance of CGuildHousePlantzonesManager and assign it to m_pPlantzones
	// The CGuildHousePlantzonesManager will handle all the plantzones for the guild house.
	m_pPlantzones = new CGuildHousePlantzonesManager(this, std::move(Plantzones));

	// Asserts
	dbg_assert(m_pPlantzones != nullptr, "The house plantzones manager is null");
	dbg_assert(m_pDecorations != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoors != nullptr, "The house doors manager is null");
}

int CGuildHouseData::GetRentPrice() const
{
	int DoorCount = (int)GetDoorManager()->GetContainer().size();
	int PlantzoneCount = (int)GetPlantzonesManager()->GetContainer().size();
	return (int)m_Radius + (DoorCount * 200) + (PlantzoneCount * 500);
}

void CGuildHouseData::GetRentTimeStamp(char* aBuffer, size_t Size) const
{
	if(IsPurchased())
	{
		int Bank = m_pGuild->GetBank()->Get();
		int Days = maximum(1, Bank) / GetRentPrice();
		time_t currentTimestamp = time(nullptr);
		tm desiredTime = *localtime(&currentTimestamp);
		desiredTime.tm_hour = 23;
		desiredTime.tm_min = 59;
		desiredTime.tm_sec = 00;
		time_t desiredTimestamp = mktime(&desiredTime) + (static_cast<long long>(Days) * 86400);
		str_timestamp_ex(desiredTimestamp, aBuffer, Size, FORMAT_SPACE);
	}
}

const char* CGuildHouseData::GetOwnerName() const
{
	if(!m_pGuild)
		return "FREE GUILD HOUSE";
	return m_pGuild->GetName();
}

void CGuildHouseData::TextUpdate(int LifeTime)
{
	// Check if the last tick text update is greater than the current server tick
	if(is_negative_vec(m_TextPosition) || m_LastTickTextUpdated > Server()->Tick())
		return;

	// Set the initial value of the variable "Name"
	std::string Name = "FREE GUILD HOUSE";
	if(IsPurchased())
		Name = m_pGuild->GetName();

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
