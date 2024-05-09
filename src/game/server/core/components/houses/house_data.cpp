/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_data.h"

#include "entities/house_door.h"

#include <game/server/gamecontext.h>
#include <game/server/core/entities/items/jobitems.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/mails/mail_wrapper.h>

CGS* CHouseData::GS() const { return static_cast<CGS*>(Server()->GameServer(m_WorldID)); }
CPlayer* CHouseData::GetPlayer() const { return GS()->GetPlayerByUserID(m_AccountID); }

CHouseData::~CHouseData()
{
	delete m_pPlantzonesManager;
	delete m_pDecorationManager;
	delete m_pDoorManager;
	delete m_pBank;
}

void CHouseData::InitProperties(int Bank, std::string&& AccessDoorList, std::string&& JsonDoors, std::string&& JsonPlantzones, std::string&& JsonProperties)
{
	// Assert important values
	dbg_assert(JsonProperties.length() > 0, "The properties string is empty");

	// Parse the JSON string
	Tools::Json::parseFromString(JsonProperties, [this](nlohmann::json& pJson)
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

	// Create a new instance of CBank and assign it to m_pBank
	// The CBank will handle bank house.
	m_pBank = new CBank(this, Bank);

	// Create a new instance of CDoorManager and assign it to m_pDoors
	// The CDoorManager will handle all the doors for the house.
	m_pDoorManager = new CDoorManager(this, std::move(AccessDoorList), std::move(JsonDoors));

	// Create a new instance of CDecorationManager and assign it to m_pDecorations
	// The CDecorationManager will handle all the decorations for the house.
	m_pDecorationManager = new CDecorationManager(this);

	// Create a new instance of CPlantzonesManager and assign it to m_pPlantzones
	// The CPlantzonesManager will handle all the plantzones for the house.
	m_pPlantzonesManager = new CPlantzonesManager(this, std::move(JsonPlantzones));

	// Asserts
	dbg_assert(m_pBank != nullptr, "The house bank is null");
	dbg_assert(m_pPlantzonesManager != nullptr, "The house plantzones manager is null");
	dbg_assert(m_pDecorationManager != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoorManager != nullptr, "The house doors manager is null");
}

void CHouseData::Buy(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	// check is player already have a house
	if(pPlayer->Account()->HasHouse())
	{
		GS()->Chat(ClientID, "You already have a home.");
		return;
	}

	// check is house has owner
	if(HasOwner())
	{
		GS()->Chat(ClientID, "House has already been purchased!");
		return;
	}

	// try spend currency
	if(pPlayer->Account()->SpendCurrency(m_Price))
	{
		// update data
		m_AccountID = pPlayer->Account()->GetID();
		m_pDoorManager->CloseAll();
		m_pBank->Reset();
		pPlayer->Account()->ReinitializeHouse();
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '%d', Bank = '0', AccessData = NULL WHERE ID = '%d'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "{} becomes the owner of the house class {}", Server()->ClientName(ClientID), GetClassName());
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**{} becomes the owner of the house class {}**", Server()->ClientName(ClientID), GetClassName());
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
}

void CHouseData::Sell()
{
	// check is has owner
	if(!HasOwner())
		return;

	// initialize variables
	CPlayer* pPlayer = GetPlayer();
	const int ReturnValue = m_Price + m_pBank->Get();

	// send mail
	MailWrapper Mail("System", m_AccountID, "House is sold.");
	Mail.AddDescLine("Your house is sold!");
	Mail.AttachItem(CItem(itGold, ReturnValue));
	Mail.Send();

	// Update the house data
	if(pPlayer)
	{
		pPlayer->Account()->ReinitializeHouse(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
	m_pDoorManager->CloseAll();
	m_pBank->Reset();
	m_AccountID = -1;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = NULL, Bank = '0', AccessData = NULL WHERE ID = '%d'", m_ID);

	// send information
	GS()->ChatAccount(m_AccountID, "Your House is sold!");
	GS()->Chat(-1, "House: {} have been is released!", m_ID);
	GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**[House: {}] have been sold!**", m_ID);
}

void CHouseData::TextUpdate(int LifeTime)
{
	// check valid vector and now time
	if(is_negative_vec(m_TextPosition) || m_LastTickTextUpdated > Server()->Tick())
		return;

	// initialize variable with name
	std::string Name = "FREE HOUSE";
	if(HasOwner())
		Name = Server()->GetAccountNickname(m_AccountID);

	// try create new text object
	if(GS()->CreateText(nullptr, false, m_TextPosition, {}, LifeTime - 5, Name.c_str()))
		m_LastTickTextUpdated = Server()->Tick() + LifeTime;
}

/* -------------------------------------
 * Bank impl
 * ------------------------------------- */
CGS* CHouseData::CBank::GS() const { return m_pHouse->GS(); }
CPlayer* CHouseData::CBank::GetPlayer() const { return m_pHouse->GetPlayer(); }
void CHouseData::CBank::Add(int Value)
{
	// check player valid
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// try get result by house id
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Bank", TW_HOUSES_TABLE, "WHERE ID = '%d'", m_pHouse->GetID());
	if(!pRes->next())
		return;

	// check spend value
	if(!pPlayer->Account()->SpendCurrency(Value))
		return;

	// update
	m_Bank = pRes->getInt("Bank") + Value;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, m_pHouse->GetID());

	// send succesful message
	GS()->Chat(pPlayer->GetCID(), "You put {} gold in the safe, now {}!", Value, m_Bank);
}

void CHouseData::CBank::Take(int Value)
{
	// check player valid
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// try get result by house id
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Bank", TW_HOUSES_TABLE, "WHERE ID = '%d'", m_pHouse->GetAccountID());
	if(!pRes->next())
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	int HouseID = pRes->getInt("ID");
	int Bank = pRes->getInt("Bank");

	// clamp value maximal
	Value = minimum(Value, Bank);
	if(Value > 0)
	{
		// update
		m_Bank = Bank - Value;
		pPlayer->Account()->AddGold(Value);
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "Bank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

		// send succesful message
		GS()->Chat(ClientID, "You take {} gold in the safe {}!", Value, m_Bank);
	}
}

/* -------------------------------------
 * Door impl
 * ------------------------------------- */
CGS* CHouseData::CDoorManager::GS() const { return m_pHouse->GS(); }
CPlayer* CHouseData::CDoorManager::GetPlayer() const { return m_pHouse->GetPlayer(); }
CHouseData::CDoorManager::CDoorManager(CHouseData* pHouse, std::string&& AccessData, std::string&& JsonDoors) : m_pHouse(pHouse)
{
	// parse doors the JSON string
	Tools::Json::parseFromString(JsonDoors, [this](nlohmann::json& pJson)
	{
		for(const auto& pDoor : pJson)
		{
			std::string Doorname = pDoor.value("name", "");
			vec2 Position = vec2(pDoor.value("x", 0), pDoor.value("y", 0));
			AddDoor(Doorname.c_str(), Position);
		}
	});

	// initialize access list
	DBSet m_Set(AccessData);
	m_vAccessUserIDs.reserve(MAX_HOUSE_DOOR_INVITED_PLAYERS);
	for(auto& p : m_Set.GetDataItems())
	{
		if(int UID = std::atoi(p.first.c_str()); UID > 0)
			m_vAccessUserIDs.insert(UID);
	}
}

CHouseData::CDoorManager::~CDoorManager()
{
	for(auto& p : m_vpEntDoors)
		delete p.second;
	m_vpEntDoors.clear();
	m_vAccessUserIDs.clear();
}

void CHouseData::CDoorManager::Open(int Number)
{
	if(m_vpEntDoors.find(Number) != m_vpEntDoors.end())
		m_vpEntDoors[Number]->Open();
}

void CHouseData::CDoorManager::Close(int Number)
{
	if(m_vpEntDoors.find(Number) != m_vpEntDoors.end())
		m_vpEntDoors[Number]->Close();
}

void CHouseData::CDoorManager::Reverse(int Number)
{
	if(m_vpEntDoors.find(Number) != m_vpEntDoors.end())
	{
		if(m_vpEntDoors[Number]->IsClosed())
			Open(Number); // Open the door
		else
			Close(Number); // Close the door
	}
}

void CHouseData::CDoorManager::OpenAll()
{
	for(auto& p : m_vpEntDoors)
		Open(p.first);
}

void CHouseData::CDoorManager::CloseAll()
{
	for(auto& p : m_vpEntDoors)
		Close(p.first);
}

void CHouseData::CDoorManager::ReverseAll()
{
	for(auto& p : m_vpEntDoors)
		Reverse(p.first);
}

void CHouseData::CDoorManager::AddAccess(int UserID)
{
	// check if the size of the m_vAccessUserIDs set is greater than or equal to the maximum number of invited players allowed
	if(m_vAccessUserIDs.size() >= MAX_HOUSE_DOOR_INVITED_PLAYERS)
	{
		GS()->ChatAccount(m_pHouse->GetAccountID(), "You have reached the limit of the allowed players!");
		return;
	}

	// try add access to list
	if(!m_vAccessUserIDs.insert(UserID).second)
		return;

	// update
	SaveAccessList();
}

void CHouseData::CDoorManager::RemoveAccess(int UserID)
{
	// try remove access
	if(m_vAccessUserIDs.erase(UserID) > 0)
		SaveAccessList();
}

bool CHouseData::CDoorManager::HasAccess(int UserID)
{
	return m_pHouse->GetAccountID() == UserID || m_vAccessUserIDs.find(UserID) != m_vAccessUserIDs.end();
}

int CHouseData::CDoorManager::GetAvailableAccessSlots() const
{
	return (int)MAX_HOUSE_DOOR_INVITED_PLAYERS - (int)m_vAccessUserIDs.size();
}

void CHouseData::CDoorManager::SaveAccessList() const
{
	// initialize variables
	std::string AccessData;
	AccessData.reserve(m_vAccessUserIDs.size() * 8);

	// parse access user ids to string
	for(const auto& UID : m_vAccessUserIDs)
	{
		AccessData += std::to_string(UID);
		AccessData += ',';
	}

	// remove last comma character
	if(!AccessData.empty())
		AccessData.pop_back();

	// update
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "AccessList = '%s' WHERE ID = '%d'", AccessData.c_str(), m_pHouse->GetID());
}

void CHouseData::CDoorManager::AddDoor(const char* pDoorname, vec2 Position)
{
	// Add the door to the m_vpEntDoors map using the door name as the key
	m_vpEntDoors.emplace(m_vpEntDoors.size() + 1, new CEntityHouseDoor(&GS()->m_World, m_pHouse, std::string(pDoorname), Position));
}

void CHouseData::CDoorManager::RemoveDoor(const char* pDoorname, vec2 Position)
{
	auto iter = std::find_if(m_vpEntDoors.begin(), m_vpEntDoors.end(), [&](const std::pair<int, CEntityHouseDoor*>& p)
	{
		return p.second->GetName() == pDoorname && p.second->GetPos() == Position;
	});

	if(iter != m_vpEntDoors.end())
	{
		delete iter->second;
		m_vpEntDoors.erase(iter);
	}
}

/* -------------------------------------
 * Decorations impl
 * ------------------------------------- */
CGS* CHouseData::CDecorationManager::GS() const { return m_pHouse->GS(); }
CHouseData::CDecorationManager::CDecorationManager(CHouseData* pHouse) : m_pHouse(pHouse)
{
	CDecorationManager::Init();
}

CHouseData::CDecorationManager::~CDecorationManager()
{
	delete m_pDrawBoard;
}

void CHouseData::CDecorationManager::Init()
{
	// Create a new instance of CEntityDrawboard and pass the world and house position as parameters
	m_pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_pHouse->GetPos(), m_pHouse->GetRadius());
	m_pDrawBoard->RegisterEvent(&CDecorationManager::DrawboardToolEventCallback, m_pHouse);
	m_pDrawBoard->SetFlags(DRAWBOARDFLAG_PLAYER_ITEMS);

	// Load from database decorations
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_HOUSES_DECORATION_TABLE, "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_pHouse->GetID());
	while(pRes->next())
	{
		int ItemID = pRes->getInt("ItemID");
		vec2 Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));

		// Add a point to the drawboard with the position and item ID
		m_pDrawBoard->AddPoint(Pos, ItemID);
	}
}

bool CHouseData::CDecorationManager::StartDrawing(CPlayer* pPlayer) const
{
	return pPlayer && pPlayer->GetCharacter() && m_pDrawBoard->StartDrawing(pPlayer);
}

bool CHouseData::CDecorationManager::HasFreeSlots() const
{
	return m_pDrawBoard->GetEntityPoints().size() < (int)MAX_DECORATIONS_PER_HOUSE;
}

bool CHouseData::CDecorationManager::DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	const auto pHouse = (CHouseData*)pUser;
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

bool CHouseData::CDecorationManager::Add(const EntityPoint* pPoint) const
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

bool CHouseData::CDecorationManager::Remove(const EntityPoint* pPoint) const
{
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// Remove the decoration from the database
	Database->Execute<DB::REMOVE>(TW_HOUSES_DECORATION_TABLE, "WHERE HouseID = '%d' AND ItemID = '%d' AND PosX = '%d' AND PosY = '%d'",
		m_pHouse->GetID(), pPoint->m_ItemID, round_to_int(pPoint->m_pEntity->GetPos().x), round_to_int(pPoint->m_pEntity->GetPos().y));
	return true;
}

/* -------------------------------------
 * Plantzones impl
 * ------------------------------------- */
void CHouseData::CPlantzone::ChangeItem(int ItemID)
{
	for(auto& pPlant : m_vPlants)
	{
		pPlant->m_ItemID = ItemID;
	}
	m_ItemID = ItemID;
	m_pManager->Save();
}

CGS* CHouseData::CPlantzonesManager::GS() const { return m_pHouse->GS(); }
CHouseData::CPlantzonesManager::CPlantzonesManager(CHouseData* pHouse, std::string&& JsonPlantzones) : m_pHouse(pHouse)
{
	// Parse the JSON string
	Tools::Json::parseFromString(JsonPlantzones, [this](nlohmann::json& pJson)
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
CHouseData::CPlantzonesManager::~CPlantzonesManager()
{
	m_vPlantzones.clear();
}

void CHouseData::CPlantzonesManager::AddPlantzone(CPlantzone&& Plantzone)
{
	m_vPlantzones.emplace(m_vPlantzones.size() + 1, std::forward<CPlantzone>(Plantzone));
}

CHouseData::CPlantzone* CHouseData::CPlantzonesManager::GetPlantzoneByPos(vec2 Pos)
{
	for(auto& p : m_vPlantzones)
	{
		if(distance(p.second.GetPos(), Pos) <= p.second.GetRadius())
			return &p.second;
	}

	return nullptr;
}

CHouseData::CPlantzone* CHouseData::CPlantzonesManager::GetPlantzoneByID(int ID)
{
	const auto it = m_vPlantzones.find(ID);
	return it != m_vPlantzones.end() ? &it->second : nullptr;
}

void CHouseData::CPlantzonesManager::Save() const
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