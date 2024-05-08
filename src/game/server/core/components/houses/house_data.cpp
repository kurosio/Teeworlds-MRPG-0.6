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
	// Remove the owner before destruct
	CPlayer* pPlayer = GetPlayer();
	if(pPlayer)
	{
		m_AccountID = -1;
		pPlayer->Account()->ReinitializeHouse();
	}

	// Delete the CHouseData
	delete m_pDrawBoard;
	delete m_pDoorManager;
	delete m_pBank;
}

void CHouseData::InitDecorations()
{
	m_pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_Pos, 900.f);
	m_pDrawBoard->RegisterEvent(&CHouseData::DrawboardToolEventCallback, this);
	m_pDrawBoard->SetFlags(DRAWBOARDFLAG_PLAYER_ITEMS);

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_HOUSES_DECORATION_TABLE, "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_ID);
	while(pRes->next())
	{
		int ItemID = pRes->getInt("ItemID");
		vec2 Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		m_pDrawBoard->AddPoint(Pos, ItemID);
	}
}

bool CHouseData::StartDrawing() const
{
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	return pPlayer ? m_pDrawBoard->StartDrawing(pPlayer) : false;
}

bool CHouseData::DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
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
			if(pHouse->AddDecoration(pPoint))
			{
				pHouse->GS()->Chat(ClientID, "You have added {} to your house!", pPlayerItem->Info()->GetName());
				return true;
			}

			return false;
		}

		if(Event == DrawboardToolEvent::ON_POINT_ERASE)
		{
			if(pHouse->RemoveDecoration(pPoint))
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

bool CHouseData::AddDecoration(const EntityPoint* pPoint) const
{
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	const CEntity* pEntity = pPoint->m_pEntity;
	const ItemIdentifier& ItemID = pPoint->m_ItemID;
	const vec2& EntityPos = pEntity->GetPos();

	// Insert to last identifier and got it
	Database->Execute<DB::INSERT>(TW_HOUSES_DECORATION_TABLE, "(ItemID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d')",
		ItemID, m_ID, round_to_int(EntityPos.x), round_to_int(EntityPos.y), GS()->GetWorldID());
	return true;
}

bool CHouseData::RemoveDecoration(const EntityPoint* pPoint) const
{
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// Remove the decoration from the database
	Database->Execute<DB::REMOVE>(TW_HOUSES_DECORATION_TABLE, "WHERE HouseID = '%d' AND ItemID = '%d' AND PosX = '%d' AND PosY = '%d'",
		m_ID, pPoint->m_ItemID, round_to_int(pPoint->m_pEntity->GetPos().x), round_to_int(pPoint->m_pEntity->GetPos().y));
	return true;
}

// This function represents the action of a player buying a house in the game.
void CHouseData::Buy(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	// Check if the player has a house
	if(pPlayer->Account()->HasHouse())
	{
		GS()->Chat(ClientID, "You already have a home.");
		return;
	}

	// Check if the house has an owner
	if(HasOwner())
	{
		GS()->Chat(ClientID, "House has already been purchased!");
		return;
	}

	// buy house
	if(pPlayer->Account()->SpendCurrency(m_Price))
	{
		// update data
		m_AccountID = pPlayer->Account()->GetID();
		m_pDoorManager->CloseAll();
		m_pBank->Reset();
		pPlayer->Account()->ReinitializeHouse();
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '%d', HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "{} becomes the owner of the house class {}", Server()->ClientName(ClientID), GetClassName());
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**{} becomes the owner of the house class {}**", Server()->ClientName(ClientID), GetClassName());
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
}

// This function is used to sell the house data.
void CHouseData::Sell()
{
	// Check if the house has an owner
	if(!HasOwner())
		return;

	// Get the pointer to the player object using the GetPlayer() function
	CPlayer* pPlayer = GetPlayer();

	// Returned fully value gold
	const int Price = m_Price + m_pBank->Get();

	// Send mail
	MailWrapper Mail("System", m_AccountID, "House is sold.");
	Mail.AddDescLine("Your house is sold!");
	Mail.AttachItem(CItem(itGold, Price));
	Mail.Send();

	// Update the house data
	m_pDoorManager->CloseAll();
	m_pBank->Reset();
	m_AccountID = -1;
	if(pPlayer)
	{
		pPlayer->Account()->ReinitializeHouse();
	}
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = NULL, HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_ID);

	// Send informations
	if(pPlayer)
	{
		GS()->Chat(pPlayer->GetCID(), "Your House is sold!");
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
	GS()->Chat(-1, "House: {} have been is released!", m_ID);
	GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**[House: {}] have been sold!**", m_ID);
}

// This function sets the plant item ID for the house data
void CHouseData::SetPlantItemID(ItemIdentifier ItemID)
{
	// Check if the ItemID already is equal to the m_PlantItemID
	if(ItemID == m_PlantedItem.GetID())
		return;

	// Check for update and set new plant itemid
	bool Updated = false;
	for(CJobItems* pPlant = (CJobItems*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_JOBITEMS); pPlant; pPlant = (CJobItems*)pPlant->TypeNext())
	{
		if(pPlant->m_HouseID == m_ID)
		{
			pPlant->m_ItemID = ItemID;
			Updated = true;
		}
	}

	// Save data
	if(Updated)
	{
		m_PlantedItem.SetID(ItemID);
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "PlantID = '%d' WHERE ID = '%d'", ItemID, m_ID);
	}
}

void CHouseData::TextUpdate(int LifeTime)
{
	// Check if the last tick text update is greater than the current server tick
	if(m_LastTickTextUpdated > Server()->Tick())
		return;

	// Set the initial value of the variable "Name" to "HOUSE"
	std::string Name = "HOUSE";

	// Check if the object has an owner
	if(HasOwner())
	{
		// If it has an owner, update the value of "Name" to the player's name
		Name = Server()->GetAccountNickname(m_AccountID);
	}

	// Create a text object with the given parameters
	if(GS()->CreateText(nullptr, false, m_TextPos, {}, LifeTime - 5, Name.c_str()))
	{
		// Update the value of "m_LastTickTextUpdated" to the current server tick plus the lifetime of the text object
		m_LastTickTextUpdated = Server()->Tick() + LifeTime;
	}
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
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", TW_HOUSES_TABLE, "WHERE ID = '%d'", m_pHouse->GetID());
	if(!pRes->next())
		return;

	// check spend value
	if(!pPlayer->Account()->SpendCurrency(Value))
		return;

	// update
	m_Bank = pRes->getInt("HouseBank") + Value;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "HouseBank = '%d' WHERE ID = '%d'", m_Bank, m_pHouse->GetID());

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
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, HouseBank", TW_HOUSES_TABLE, "WHERE ID = '%d'", m_pHouse->GetAccountID());
	if(!pRes->next())
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	int HouseID = pRes->getInt("ID");
	int Bank = pRes->getInt("HouseBank");

	// clamp value maximal
	Value = minimum(Value, Bank);
	if(Value > 0)
	{
		// update
		m_Bank = Bank - Value;
		pPlayer->Account()->AddGold(Value);
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "HouseBank = '%d' WHERE ID = '%d'", m_Bank, HouseID);

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
	// prepare
	m_AccessUserIDs.reserve(MAX_HOUSE_DOOR_INVITED_PLAYERS);

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
	for(auto& p : m_Set.GetDataItems())
	{
		if(int UID = std::atoi(p.first.c_str()); UID > 0)
			m_AccessUserIDs.insert(UID);
	}
}

CHouseData::CDoorManager::~CDoorManager()
{
	for(auto& p : m_apEntDoors)
		delete p.second;
	m_apEntDoors.clear();
}

void CHouseData::CDoorManager::Open(int Number)
{
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Open();
}

void CHouseData::CDoorManager::Close(int Number)
{
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Close();
}

void CHouseData::CDoorManager::Reverse(int Number)
{
	if(m_apEntDoors.find(Number) == m_apEntDoors.end())
		return;

	// Check if the door is closed
	if(m_apEntDoors[Number]->IsClosed())
		Open(Number); // Open the door
	else
		Close(Number); // Close the door
}

void CHouseData::CDoorManager::OpenAll()
{
	for(auto& p : m_apEntDoors)
		Open(p.first);
}

void CHouseData::CDoorManager::CloseAll()
{
	for(auto& p : m_apEntDoors)
		Close(p.first);
}

void CHouseData::CDoorManager::ReverseAll()
{
	for(auto& p : m_apEntDoors)
		Reverse(p.first);
}

void CHouseData::CDoorManager::AddAccess(int UserID)
{
	// Check if the size of the m_AccessUserIDs set is greater than or equal to the maximum number of invited players allowed
	if(m_AccessUserIDs.size() >= MAX_HOUSE_DOOR_INVITED_PLAYERS)
	{
		GS()->ChatAccount(m_pHouse->GetAccountID(), "You have reached the limit of the allowed players!");
		return;
	}

	// try add access to list
	if(!m_AccessUserIDs.insert(UserID).second)
		return;

	// update
	SaveAccessList();
}

void CHouseData::CDoorManager::RemoveAccess(int UserID)
{
	// try remove access
	if(m_AccessUserIDs.erase(UserID) > 0)
		SaveAccessList();
}

bool CHouseData::CDoorManager::HasAccess(int UserID)
{
	return m_pHouse->GetAccountID() == UserID || m_AccessUserIDs.find(UserID) != m_AccessUserIDs.end();
}

int CHouseData::CDoorManager::GetAvailableAccessSlots() const { return (int)MAX_HOUSE_DOOR_INVITED_PLAYERS - (int)m_AccessUserIDs.size(); }

void CHouseData::CDoorManager::SaveAccessList() const
{
	// initialize variables
	std::string AccessData;
	AccessData.reserve(m_AccessUserIDs.size() * 8);

	// parse access user ids to string
	for(const auto& UID : m_AccessUserIDs)
	{
		AccessData += std::to_string(UID);
		AccessData += ',';
	}

	// remove last comma character
	if(!AccessData.empty())
		AccessData.pop_back();

	// update
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "AccessData = '%s' WHERE ID = '%d'", AccessData.c_str(), m_pHouse->GetID());
}

void CHouseData::CDoorManager::AddDoor(const char* pDoorname, vec2 Position)
{
	// Add the door to the m_apEntDoors map using the door name as the key
	m_apEntDoors.emplace(m_apEntDoors.size() + 1, new CEntityHouseDoor(&GS()->m_World, m_pHouse, std::string(pDoorname), Position));
}

void CHouseData::CDoorManager::RemoveDoor(const char* pDoorname, vec2 Position)
{
	auto iter = std::find_if(m_apEntDoors.begin(), m_apEntDoors.end(), [&](const std::pair<int, CEntityHouseDoor*>& p)
	{
		return p.second->GetName() == pDoorname && p.second->GetPos() == Position;
	});

	if(iter != m_apEntDoors.end())
	{
		delete iter->second;
		m_apEntDoors.erase(iter);
	}
}