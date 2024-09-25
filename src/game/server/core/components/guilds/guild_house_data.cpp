/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_house_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/harvesting_item.h>
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

	// Parse the JSON string
	mystd::json::parse(JsonProperties, [this](nlohmann::json& pJson)
	{
		// Assert for important properties
		dbg_assert(pJson.find("pos") != pJson.end(), "The importal properties value is empty");

		auto pHousePosData = pJson["pos"];
		m_Position.x = (float)pHousePosData.value("x", 0);
		m_Position.y = (float)pHousePosData.value("y", 0);
		m_Radius = (float)pHousePosData.value("radius", 300);

		// Initialize text position
		if(pJson.find("text_pos") != pJson.end())
		{
			auto pTextPosData = pJson["text_pos"];
			m_TextPosition.x = (float)pTextPosData.value("x", 0);
			m_TextPosition.y = (float)pTextPosData.value("y", 0);
		}
	});

	// Create a new instance of CDecorationManager and assign it to m_pDecorationManager
	// The CDecorationManager will handle all the decorations for the guild house.
	m_pDecorationManager = new CDecorationManager(this);

	// Create a new instance of CGuildHouseDoorManager and assign it to m_pDoors
	// The CDecorationManager will handle all the doors for the guild house.
	m_pDoors = new CDoorManager(this, std::move(JsonDoors));

	// Create a new instance of CFarmzonesManager and assign it to m_pFarmzonesManager
	// The CFarmzonesManager will handle all the farmzones for the guild house.
	m_pFarmzonesManager = new CFarmzonesManager(this, std::move(JsonFarmzones));

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
	// check valid vector and now time
	if(!is_negative_vec(m_TextPosition))
		return;

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

/* -------------------------------------
 * Farmzones impl
 * ------------------------------------- */
void CGuildHouse::CFarmzone::ChangeItem(int ItemID)
{
	for(auto& pFarm : m_vFarms)
		pFarm->m_ItemID = ItemID;
	m_ItemID = ItemID;
	m_pManager->Save();
}

CGS* CGuildHouse::CFarmzonesManager::GS() const { return m_pHouse->GS(); }
CGuildHouse::CFarmzonesManager::CFarmzonesManager(CGuildHouse* pHouse, std::string&& JsonFarmzones) : m_pHouse(pHouse)
{
	// Parse the JSON string
	mystd::json::parse(JsonFarmzones, [this](nlohmann::json& pJson)
	{
		for(const auto& Farmzone : pJson)
		{
			std::string Farmname = Farmzone.value("name", "");
			vec2 Position = vec2(Farmzone.value("x", 0), Farmzone.value("y", 0));
			int ItemID = Farmzone.value("item_id", 0);
			float Radius = (float)Farmzone.value("radius", 100);
			AddFarmzone({ this, Farmname.c_str(), ItemID, Position, Radius });
		}
	});
}

CGuildHouse::CFarmzonesManager::~CFarmzonesManager()
{
	m_vFarmzones.clear();
}

void CGuildHouse::CFarmzonesManager::AddFarmzone(CFarmzone&& Farmzone)
{
	m_vFarmzones.emplace(m_vFarmzones.size() + 1, std::forward<CFarmzone>(Farmzone));
}

CGuildHouse::CFarmzone* CGuildHouse::CFarmzonesManager::GetFarmzoneByPos(vec2 Pos)
{
	for(auto& p : m_vFarmzones)
	{
		if(distance(p.second.GetPos(), Pos) <= p.second.GetRadius())
			return &p.second;
	}

	return nullptr;
}

CGuildHouse::CFarmzone* CGuildHouse::CFarmzonesManager::GetFarmzoneByID(int ID)
{
	const auto it = m_vFarmzones.find(ID);
	return it != m_vFarmzones.end() ? &it->second : nullptr;
}

void CGuildHouse::CFarmzonesManager::Save() const
{
	// Create a JSON object to store farm zones data
	nlohmann::json Farmzones;
	for(auto& p : m_vFarmzones)
	{
		// Create a JSON object to store data for each farm zone
		nlohmann::json farmzoneData;
		farmzoneData["name"] = p.second.GetName();
		farmzoneData["x"] = round_to_int(p.second.GetPos().x);
		farmzoneData["y"] = round_to_int(p.second.GetPos().y);
		farmzoneData["item_id"] = p.second.GetItemID();
		farmzoneData["radius"] = round_to_int(p.second.GetRadius());
		Farmzones.push_back(farmzoneData);
	}

	// update database
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "Farmzones = '{}' WHERE ID = '{}'", Farmzones.dump().c_str(), m_pHouse->GetID());
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
	if(Event == DrawboardToolEvent::ON_POINT_ADD && pPoint)
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
	if(Event == DrawboardToolEvent::ON_POINT_ERASE && pPoint)
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
	if(Event == DrawboardToolEvent::ON_END)
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
			vec2 Position = vec2(pDoor.value("x", 0), pDoor.value("y", 0));
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