/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_house_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/guilds/entities/guild_door.h>
#include <game/server/core/components/guilds/guild_data.h>

CGS* CGuildHouse::GS() const { return (CGS*)Instance::GameServer(m_WorldID); }
CGuildHouse::~CGuildHouse()
{
	delete m_pDoors;
	delete m_pFarmzonesManager;
	delete m_pDecorationManager;
}

void CGuildHouse::InitProperties(std::string&& JsonDoors, std::string&& JsonFarmzones, std::string&& JsonProperties)
{
	// Assert important values
	dbg_assert(JsonProperties.length() > 0, "The properties string is empty");

	// initialize main settings
	mystd::json::parse(JsonProperties, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("position") != pJson.end(), "The importal properties value is empty");
		m_Position = pJson.value("position", vec2());
		m_TextPosition = pJson.value("text_position", vec2());
		m_Radius = (float)pJson.value("radius", 300);
	});

	// Create a new instance of CDecorationManager and assign it to m_pDecorationManager
	// The CDecorationManager will handle all the decorations for the guild house.
	m_pDecorationManager = new CDecorationManager(this);

	// Create a new instance of CGuildHouseDoorManager and assign it to m_pDoors
	// The CDecorationManager will handle all the doors for the guild house.
	m_pDoors = new CDoorManager(this, std::move(JsonDoors));

	// Create a new instance of CFarmzonesManager and assign it to m_pFarmzonesManager
	// The CFarmzonesManager will handle all the farmzones for the guild house.
	m_pFarmzonesManager = new CFarmzonesManager(std::move(JsonFarmzones));

	// Asserts
	dbg_assert(m_pFarmzonesManager != nullptr, "The house farmzones manager is null");
	dbg_assert(m_pDecorationManager != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoors != nullptr, "The house doors manager is null");
}

int CGuildHouse::GetRentPrice() const
{
	int DoorCount = (int)GetDoorManager()->GetContainer().size();
	int FarmzoneCount = (int)GetFarmzonesManager()->GetContainer().size();
	return (int)m_Radius + (DoorCount * 200) + (FarmzoneCount * 500);
}

const char* CGuildHouse::GetOwnerName() const
{
	if(!m_pGuild)
		return "FREE GUILD HOUSE";
	return m_pGuild->GetName();
}

bool CGuildHouse::ExtendRentDays(int Days)
{
	// check validity
	if(!m_pGuild || !m_pGuild->GetBank())
		return false;

	// try spend for rent days
	if(m_pGuild->GetBank()->Spend(GetRentPrice() * Days))
	{
		m_RentDays += Days;
		Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "RentDays = '{}' WHERE ID = '{}'", m_RentDays, m_ID);
		return true;
	}

	return false;
}

bool CGuildHouse::ReduceRentDays(int Days)
{
	// check validity
	if(!m_pGuild || !m_pGuild->GetBank() || m_RentDays <= 0)
		return false;

	// reduce rent days
	m_RentDays -= clamp(Days, 1, m_RentDays);
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "RentDays = '{}' WHERE ID = '{}'", m_RentDays, m_ID);
	return true;
}

void CGuildHouse::UpdateText(int Lifetime) const
{
	// update text
	const char* pName = IsPurchased() ? m_pGuild->GetName() : "FREE GUILD HOUSE";
	GS()->EntityManager()->Text(m_TextPosition, Lifetime - 5, pName);
}

void CGuildHouse::UpdateGuild(CGuild* pGuild)
{
	m_pGuild = pGuild;
	if(m_pGuild)
	{
		m_pGuild->m_pHouse = this;
	}
}

void CGuildHouse::Save()
{
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "Farmzones = '{}' WHERE ID = '{}'", m_pFarmzonesManager->DumpJsonString(), m_ID);
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
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE WorldID = '{}' AND HouseID = '{}'", GS()->GetWorldID(), m_pHouse->GetID());
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
	return pPlayer && pPlayer->GetCharacter() && m_pDrawBoard->StartDrawing(pPlayer);
}

bool CGuildHouse::CDecorationManager::HasFreeSlots() const
{
	return m_pDrawBoard->GetEntityPoints().size() < (int)MAX_DECORATIONS_PER_HOUSE;
}

bool CGuildHouse::CDecorationManager::DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	// check validity
	const auto pHouse = (CGuildHouse*)pUser;
	if(!pPlayer || !pHouse)
		return false;

	// initialize variables
	const int& ClientID = pPlayer->GetCID();

	// event point add
	if(Event == DrawboardToolEvent::OnPointAdd && pPoint)
	{
		// check if there are free slots
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(!pHouse->GetDecorationManager()->HasFreeSlots())
		{
			pHouse->GS()->Chat(ClientID, "You have reached the maximum number of decorations for your house!");
			return false;
		}

		// try to add the point
		if(pHouse->GetDecorationManager()->Add(pPoint))
		{
			pHouse->GS()->Chat(ClientID, "You have added {} to your house!", pPlayerItem->Info()->GetName());
			return true;
		}

		return false;
	}

	// event point erase
	if(Event == DrawboardToolEvent::OnPointErase && pPoint)
	{
		// try to remove the point
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(pHouse->GetDecorationManager()->Remove(pPoint))
		{
			pHouse->GS()->Chat(ClientID, "You have removed {} from your house!", pPlayerItem->Info()->GetName());
			return true;
		}

		return false;
	}

	// event end
	if(Event == DrawboardToolEvent::OnEnd)
	{
		pHouse->GS()->Chat(ClientID, "You have finished decorating your house!");
		return true;
	}

	return true;
}

bool CGuildHouse::CDecorationManager::Add(const EntityPoint* pPoint) const
{
	// check validity of pPoint
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// initialize variables
	const CEntity* pEntity = pPoint->m_pEntity;
	const ItemIdentifier& ItemID = pPoint->m_ItemID;
	const vec2& EntityPos = pEntity->GetPos();

	// execute a database insert query
	Database->Execute<DB::INSERT>(TW_GUILD_HOUSES_DECORATION_TABLE, "(ItemID, HouseID, PosX, PosY, WorldID) VALUES ('{}', '{}', '{}', '{}', '{}')",
		ItemID, m_pHouse->GetID(), round_to_int(EntityPos.x), round_to_int(EntityPos.y), GS()->GetWorldID());
	return true;
}

bool CGuildHouse::CDecorationManager::Remove(const EntityPoint* pPoint) const
{
	// check validity of pPoint
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// execute a database remove query
	Database->Execute<DB::REMOVE>(TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE HouseID = '{}' AND ItemID = '{}' AND PosX = '{}' AND PosY = '{}'",
		m_pHouse->GetID(), pPoint->m_ItemID, round_to_int(pPoint->m_pEntity->GetPos().x), round_to_int(pPoint->m_pEntity->GetPos().y));
	return true;
}

/* -------------------------------------
 * Doors impl
 * ------------------------------------- */
CGS* CGuildHouse::CDoorManager::GS() const { return m_pHouse->GS(); }
CGuildHouse::CDoorManager::CDoorManager(CGuildHouse* pHouse, std::string&& JsonDoors) : m_pHouse(pHouse)
{
	// Parse the JSON string
	mystd::json::parse(JsonDoors, [this](nlohmann::json& pJson)
	{
		for(const auto& pDoor : pJson)
		{
			std::string Doorname = pDoor.value("name", "");
			vec2 Position = pDoor.value("position", vec2());
			AddDoor(Doorname.c_str(), Position);
		}
	});
}

CGuildHouse::CDoorManager::~CDoorManager()
{
	// delete all doors
	for(auto& p : m_apEntDoors)
		delete p.second;

	m_apEntDoors.clear();
}

void CGuildHouse::CDoorManager::Open(int Number)
{
	// open door by number
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Open();
}

void CGuildHouse::CDoorManager::Close(int Number)
{
	// close door by number
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Close();
}

void CGuildHouse::CDoorManager::Reverse(int Number)
{
	// check valid door by number
	if(m_apEntDoors.find(Number) == m_apEntDoors.end())
		return;

	// implement reverse
	return m_apEntDoors[Number]->Reverse();
}

void CGuildHouse::CDoorManager::OpenAll()
{
	// open the state for each door
	for(auto& p : m_apEntDoors)
		Open(p.first);
}

void CGuildHouse::CDoorManager::CloseAll()
{
	// close the state for each door
	for(auto& p : m_apEntDoors)
		Close(p.first);
}

void CGuildHouse::CDoorManager::ReverseAll()
{
	// reverse the state for each door
	for(auto& p : m_apEntDoors)
		Reverse(p.first);
}

void CGuildHouse::CDoorManager::AddDoor(const char* pDoorname, vec2 Position)
{
	// add door
	m_apEntDoors.emplace(m_apEntDoors.size() + 1, new CEntityGuildDoor(&GS()->m_World, m_pHouse, std::string(pDoorname), Position));
}

void CGuildHouse::CDoorManager::RemoveDoor(const char* pDoorname, vec2 Position)
{
	// find the door in the m_apEntDoors
	auto iter = std::find_if(m_apEntDoors.begin(), m_apEntDoors.end(), [&](const std::pair<int, CEntityGuildDoor*>& p)
	{
		return p.second->GetName() == pDoorname && p.second->GetPos() == Position;
	});

	// implement removal
	if(iter != m_apEntDoors.end())
	{
		delete iter->second;
		m_apEntDoors.erase(iter);
	}
}