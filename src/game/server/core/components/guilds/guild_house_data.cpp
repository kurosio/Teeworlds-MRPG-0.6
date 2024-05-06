/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_house_data.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/jobitems.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/guilds/entities/guild_door.h>
#include <game/server/core/components/guilds/guild_data.h>

CGS* CGuildHouse::GS() const { return (CGS*)Instance::GameServer(m_WorldID); }

CGuildHouse::~CGuildHouse()
{
	delete m_pPlantzones;
	delete m_pDecorations;
	delete m_pDoors;
}

void CGuildHouse::InitProperties(std::string&& Plantzones, std::string&& Properties)
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

		// Create a new instance of CDecorationManager and assign it to m_pDecorations
		// The CDecorationManager will handle all the decorations for the guild house.
		m_pDecorations = new CDecorationManager(this);

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
			// The CDecorationManager will handle all the doors for the guild house.
			m_pDoors = new CDoorManager(this);

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

	// Create a new instance of CPlantzonesManager and assign it to m_pPlantzones
	// The CPlantzonesManager will handle all the plantzones for the guild house.
	m_pPlantzones = new CPlantzonesManager(this, std::move(Plantzones));

	// Asserts
	dbg_assert(m_pPlantzones != nullptr, "The house plantzones manager is null");
	dbg_assert(m_pDecorations != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoors != nullptr, "The house doors manager is null");
}

int CGuildHouse::GetRentPrice() const
{
	int DoorCount = (int)GetDoorManager()->GetContainer().size();
	int PlantzoneCount = (int)GetPlantzonesManager()->GetContainer().size();
	return (int)m_Radius + (DoorCount * 200) + (PlantzoneCount * 500);
}

void CGuildHouse::GetRentTimeStamp(char* aBuffer, size_t Size) const
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

const char* CGuildHouse::GetOwnerName() const
{
	if(!m_pGuild)
		return "FREE GUILD HOUSE";
	return m_pGuild->GetName();
}

void CGuildHouse::TextUpdate(int LifeTime)
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

void CGuildHouse::UpdateGuild(CGuild* pGuild)
{
	m_pGuild = pGuild;
	if(m_pGuild)
	{
		m_pGuild->m_pHouse = this;
	}
}

/* -------------------------------------
 * Plantzones impl
 * ------------------------------------- */
void CGuildHouse::CPlantzone::ChangeItem(int ItemID) const
{
	for(auto& pPlant : m_vPlants)
	{
		pPlant->m_ItemID = ItemID;
	}
	m_pManager->Save();
}

CGS* CGuildHouse::CPlantzonesManager::GS() const { return m_pHouse->GS(); }

CGuildHouse::CPlantzonesManager::CPlantzonesManager(CGuildHouse* pHouse, std::string&& JsPlantzones) : m_pHouse(pHouse)
{
	// Parse the JSON string
	Tools::Json::parseFromString(JsPlantzones, [this](nlohmann::json& pJson)
	{
		for(const auto& pPlantzone : pJson)
		{
			std::string Plantname = pPlantzone.value("name", "");
			vec2 Position = vec2(pPlantzone.value("x", 0), pPlantzone.value("y", 0));
			int ItemID = pPlantzone.value("item_id", 0);
			float Radius = pPlantzone.value("radius", 100);
			AddPlantzone({ this, Plantname.c_str(), ItemID, Position, Radius });
		}
	});
}

// Destructor for the CHouseDoorsController class
CGuildHouse::CPlantzonesManager::~CPlantzonesManager()
{
	m_vPlantzones.clear();
}

void CGuildHouse::CPlantzonesManager::AddPlantzone(CPlantzone&& Plantzone)
{
	m_vPlantzones.emplace(m_vPlantzones.size() + 1, std::forward<CPlantzone>(Plantzone));
}

CGuildHouse::CPlantzone* CGuildHouse::CPlantzonesManager::GetPlantzoneByPos(vec2 Pos)
{
	for(auto& p : m_vPlantzones)
	{
		if(distance(p.second.GetPos(), Pos) <= p.second.GetRadius())
			return &p.second;
	}

	return nullptr;
}

CGuildHouse::CPlantzone* CGuildHouse::CPlantzonesManager::GetPlantzoneByID(int ID)
{
	auto it = m_vPlantzones.find(ID);
	return it != m_vPlantzones.end() ? &it->second : nullptr;
}

void CGuildHouse::CPlantzonesManager::Save() const
{
	// Create a JSON object to store plant zones data
	nlohmann::json Plantzones;
	for(auto& p : m_vPlantzones)
	{
		// Create a JSON object to store data for each plant zone
		nlohmann::json plantzoneData;
		plantzoneData["name"] = p.second.GetName();
		plantzoneData["x"] = round_to_int(p.second.GetPos().x);
		plantzoneData["y"] = round_to_int(p.second.GetPos().y);
		plantzoneData["item_id"] = p.second.GetItemID();
		plantzoneData["radius"] = round_to_int(p.second.GetRadius());
		Plantzones.push_back(plantzoneData);
	}

	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "Plantzones = '%s' WHERE ID = '%d'", Plantzones.dump().c_str(), m_pHouse->GetID());
}

/* -------------------------------------
 * Decorations impl
 * ------------------------------------- */
CGS* CGuildHouse::CDecorationManager::GS() const { return m_pHouse->GS(); }

CGuildHouse::CDecorationManager::CDecorationManager(CGuildHouse* pHouse) : m_pHouse(pHouse)
{
	CDecorationManager::Init();
}

CGuildHouse::CDecorationManager::~CDecorationManager()
{
	delete m_pDrawBoard;
}

void CGuildHouse::CDecorationManager::Init()
{
	// Create a new instance of CEntityDrawboard and pass the world and house position as parameters
	m_pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_pHouse->GetPos(), m_pHouse->GetRadius());
	m_pDrawBoard->RegisterEvent(&CDecorationManager::DrawboardToolEventCallback, m_pHouse);
	m_pDrawBoard->SetFlags(DRAWBOARDFLAG_PLAYER_ITEMS);

	// Load from database decorations
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_pHouse->GetID());
	while(pRes->next())
	{
		int ItemID = pRes->getInt("ItemID");
		vec2 Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));

		// Add a point to the drawboard with the position and item ID
		m_pDrawBoard->AddPoint(Pos, ItemID);
	}
}

bool CGuildHouse::CDecorationManager::StartDrawing(CPlayer* pPlayer) const
{
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;
	return m_pDrawBoard->StartDrawing(pPlayer);
}

bool CGuildHouse::CDecorationManager::HasFreeSlots() const
{
	return m_pDrawBoard->GetEntityPoints().size() < (int)MAX_DECORATIONS_PER_HOUSE;
}

bool CGuildHouse::CDecorationManager::DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	const auto pHouse = (CGuildHouse*)pUser;
	if(!pPlayer || !pHouse)
		return false;

	const int& ClientID = pPlayer->GetCID();

	if(pPoint)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(Event == DrawboardToolEvent::ON_POINT_ADD)
		{
			if(!pHouse->GetDecorationManager()->HasFreeSlots())
			{
				pHouse->GS()->Chat(ClientID, "You have reached the maximum number of decorations for your house!");
				return false;
			}

			if(pHouse->GetDecorationManager()->Add(pPoint))
			{
				pHouse->GS()->Chat(ClientID, "You have added {} to your house!", pPlayerItem->Info()->GetName());
				return true;
			}

			return false;
		}

		if(Event == DrawboardToolEvent::ON_POINT_ERASE)
		{
			if(pHouse->GetDecorationManager()->Remove(pPoint))
			{
				pHouse->GS()->Chat(ClientID, "You have removed {} from your house!", pPlayerItem->Info()->GetName());
				return true;
			}

			return false;
		}
	}

	if(Event == DrawboardToolEvent::ON_END)
	{
		pHouse->GS()->Chat(ClientID, "You have finished decorating your house!");
		return true;
	}

	return true;
}

bool CGuildHouse::CDecorationManager::Add(const EntityPoint* pPoint) const
{
	// Check if pPoint or pPoint->m_pEntity is null
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// Get ItemID pEntity and EntityPos
	const CEntity* pEntity = pPoint->m_pEntity;
	const ItemIdentifier& ItemID = pPoint->m_ItemID;
	const vec2& EntityPos = pEntity->GetPos();

	// Execute a database insert query with the values
	Database->Execute<DB::INSERT>(TW_GUILD_HOUSES_DECORATION_TABLE, "(ItemID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d')",
		ItemID, m_pHouse->GetID(), round_to_int(EntityPos.x), round_to_int(EntityPos.y), GS()->GetWorldID());
	return true;
}

bool CGuildHouse::CDecorationManager::Remove(const EntityPoint* pPoint) const
{
	// Check if pPoint is null or if pPoint's m_pEntity is null
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// Execute a remove query on the TW_GUILD_HOUSES_DECORATION_TABLE in the Database
	// The query removes a row where HouseID, ItemID, PosX, and PosY match the given values
	// The values are obtained from m_pHouse's ID, pPoint's m_ItemID, and rounded positions of pPoint's m_pEntity
	Database->Execute<DB::REMOVE>(TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE HouseID = '%d' AND ItemID = '%d' AND PosX = '%d' AND PosY = '%d'",
		m_pHouse->GetID(), pPoint->m_ItemID, round_to_int(pPoint->m_pEntity->GetPos().x), round_to_int(pPoint->m_pEntity->GetPos().y));
	return true;
}


/* -------------------------------------
 * Doors impl
 * ------------------------------------- */
CGS* CGuildHouse::CDoorManager::GS() const { return m_pHouse->GS(); }

CGuildHouse::CDoorManager::CDoorManager(CGuildHouse* pHouse) : m_pHouse(pHouse) {}
CGuildHouse::CDoorManager::~CDoorManager()
{
	for(auto& p : m_apEntDoors)
		delete p.second;

	m_apEntDoors.clear();
}

void CGuildHouse::CDoorManager::Open(int Number)
{
	// Open the door
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Open();
}

void CGuildHouse::CDoorManager::Close(int Number)
{
	// Close the door
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Close();
}

void CGuildHouse::CDoorManager::Reverse(int Number)
{
	// Check if the door exists in the map
	if(m_apEntDoors.find(Number) == m_apEntDoors.end())
		return;

	// Check if the door is closed
	if(m_apEntDoors[Number]->IsClosed())
		Open(Number); // Open the door
	else
		Close(Number); // Close the door
}

void CGuildHouse::CDoorManager::OpenAll()
{
	// Open the state of the door by its number in iterate
	for(auto& p : m_apEntDoors)
		Open(p.first);
}

void CGuildHouse::CDoorManager::CloseAll()
{
	// Close the state of the door by its number in iterate
	for(auto& p : m_apEntDoors)
		Close(p.first);
}

void CGuildHouse::CDoorManager::ReverseAll()
{
	// Reverse the state of the door by its number in iterate
	for(auto& p : m_apEntDoors)
		Reverse(p.first);
}

void CGuildHouse::CDoorManager::AddDoor(const char* pDoorname, vec2 Position)
{
	// Add the door to the m_apEntDoors map using the door name as the key
	m_apEntDoors.emplace(m_apEntDoors.size() + 1, new CEntityGuildDoor(&GS()->m_World, m_pHouse, std::string(pDoorname), Position));
}

void CGuildHouse::CDoorManager::RemoveDoor(const char* pDoorname, vec2 Position)
{
	auto iter = std::find_if(m_apEntDoors.begin(), m_apEntDoors.end(), [&](const std::pair<int, CEntityGuildDoor*>& p)
	{
		return p.second->GetName() == pDoorname && p.second->GetPos() == Position;
	});

	if(iter != m_apEntDoors.end())
	{
		delete iter->second;
		m_apEntDoors.erase(iter);
	}
}