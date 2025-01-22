/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_data.h"

#include "entities/house_door.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/core/entities/tools/draw_board.h>
#include <game/server/core/components/mails/mail_wrapper.h>

CGS* CHouse::GS() const { return static_cast<CGS*>(Server()->GameServer(m_WorldID)); }
CPlayer* CHouse::GetPlayer() const { return GS()->GetPlayerByUserID(m_AccountID); }

CHouse::~CHouse()
{
	delete m_pFarmzonesManager;
	delete m_pDecorationManager;
	delete m_pDoorManager;
	delete m_pBank;
}

void CHouse::InitProperties(int Bank, std::string&& AccessDoorList, std::string&& JsonDoors, std::string&& JsonFarmzones, std::string&& JsonProperties)
{
	// Assert important values
	dbg_assert(JsonProperties.length() > 0, "The properties string is empty");

	// Parse the JSON string
	mystd::json::parse(JsonProperties, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("position") != pJson.end(), "The importal properties value is empty");
		m_Position = pJson.value("position", vec2());
		m_Radius = (float)pJson.value("radius", 300);
		m_TextPosition = pJson.value("text_position", vec2());
	});

	// Create a new instance of CBank and assign it to m_pBank
	// The CBank will handle bank house.
	m_pBank = new CBank(this, Bank);

	// Create a new instance of CDoorManager and assign it to m_pDoors
	// The CDoorManager will handle all the doors for the house.
	m_pDoorManager = new CDoorManager(this, std::move(AccessDoorList), std::move(JsonDoors));

	// Create a new instance of CDecorationManager and assign it to m_pDecorationManager
	// The CDecorationManager will handle all the decorations for the house.
	m_pDecorationManager = new CDecorationManager(this);

	// Create a new instance of CFarmzonesManager and assign it to m_pFarmzonesManager
	// The CFarmzonesManager will handle all the farmzones for the house.
	m_pFarmzonesManager = new CFarmzonesManager(std::move(JsonFarmzones));

	// Asserts
	dbg_assert(m_pBank != nullptr, "The house bank is null");
	dbg_assert(m_pFarmzonesManager != nullptr, "The house farmzones manager is null");
	dbg_assert(m_pDecorationManager != nullptr, "The house decorations manager is null");
	dbg_assert(m_pDoorManager != nullptr, "The house doors manager is null");
}

void CHouse::Buy(CPlayer* pPlayer)
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
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '{}', Bank = '0', AccessList = NULL WHERE ID = '{}'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "{} becomes the owner of the house class {}", Server()->ClientName(ClientID), GetClassName());
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
}

void CHouse::Sell()
{
	// check is has owner
	if(!HasOwner())
		return;

	// send mail
	const int ReturnValue = m_Price + m_pBank->Get();
	MailWrapper Mail("System", m_AccountID, "House is sold.");
	Mail.AddDescLine("Your house is sold!");
	Mail.AttachItem(CItem(itGold, ReturnValue));
	Mail.Send();

	// Update the house data
	if(CPlayer* pPlayer = GetPlayer())
	{
		pPlayer->Account()->ReinitializeHouse(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
	m_pDoorManager->CloseAll();
	m_pBank->Reset();
	m_AccountID = -1;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = NULL, Bank = '0', AccessList = NULL WHERE ID = '{}'", m_ID);

	// send information
	GS()->ChatAccount(m_AccountID, "Your House is sold!");
	GS()->Chat(-1, "House: {} have been is released!", m_ID);
}

void CHouse::UpdateText(int Lifetime) const
{
	// check valid vector and now time
	if(is_negative_vec(m_TextPosition))
		return;

	// initialize variable with name
	const char* pName = HasOwner() ? Server()->GetAccountNickname(m_AccountID) : "FREE HOUSE";
	GS()->EntityManager()->Text(m_TextPosition, Lifetime - 5, pName);
}

void CHouse::HandleTimePeriod(ETimePeriod Period)
{
	// day event rent paid
	if(Period == DAILY_STAMP && HasOwner())
	{
		// try spend to rent paid
		if(!m_pBank->Spend(GetRentPrice()))
		{
			Sell();
			return;
		}

		// send message
		GS()->ChatAccount(m_AccountID, "Your house rent has been paid.");
	}
}

void CHouse::Save()
{
	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "Farmzones = '{}' WHERE ID = '{}'", m_pFarmzonesManager->DumpJsonString(), m_ID);
}

int CHouse::GetRentPrice() const
{
	int DoorCount = (int)GetDoorManager()->GetContainer().size();
	int FarmzoneCount = (int)GetFarmzonesManager()->GetContainer().size();
	return (int)m_Radius + (DoorCount * 200) + (FarmzoneCount * 500);
}

/* -------------------------------------
 * Bank impl
 * ------------------------------------- */
CGS* CHouse::CBank::GS() const { return m_pHouse->GS(); }
CPlayer* CHouse::CBank::GetPlayer() const { return m_pHouse->GetPlayer(); }
void CHouse::CBank::Add(int Value)
{
	// check player valid
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	// check spend value
	if(!pPlayer->Account()->SpendCurrency(Value))
		return;

	// update
	m_Bank += Value;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "Bank = '{}' WHERE ID = '{}'", m_Bank, m_pHouse->GetID());
	GS()->Chat(pPlayer->GetCID(), "You put {} gold in the safe, now {}!", Value, m_Bank);
}

void CHouse::CBank::Take(int Value)
{
	// check player valid
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || m_Bank <= 0)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	Value = minimum(Value, m_Bank);

	if(Value > 0)
	{
		// update
		m_Bank -= Value;
		pPlayer->Account()->AddGold(Value);
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "Bank = '{}' WHERE ID = '{}'", m_Bank, m_pHouse->GetID());
		GS()->Chat(ClientID, "You take {} gold in the safe {}!", Value, m_Bank);
	}
}

bool CHouse::CBank::Spend(int Value)
{
	if(m_Bank <= 0 || m_Bank < Value)
		return false;

	m_Bank -= Value;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "Bank = '{}' WHERE ID = '{}'", m_Bank, m_pHouse->GetID());
	return true;
}

/* -------------------------------------
 * Door impl
 * ------------------------------------- */
CGS* CHouse::CDoorManager::GS() const { return m_pHouse->GS(); }
CPlayer* CHouse::CDoorManager::GetPlayer() const { return m_pHouse->GetPlayer(); }
CHouse::CDoorManager::CDoorManager(CHouse* pHouse, std::string&& AccessList, std::string&& JsonDoors) : m_pHouse(pHouse)
{
	// parse doors the JSON string
	mystd::json::parse(JsonDoors, [this](nlohmann::json& pJson)
	{
		for(const auto& pDoor : pJson)
		{
			auto Doorname = pDoor.value("name", "");
			auto Position = pDoor.value("position", vec2());

			AddDoor(Doorname.c_str(), Position);
		}
	});

	// initialize access list
	DBSet m_Set(AccessList);
	m_vAccessUserIDs.reserve(MAX_HOUSE_DOOR_INVITED_PLAYERS);
	for(auto& p : m_Set.GetDataItems())
	{
		if(int UID = std::atoi(p.first.c_str()); UID > 0)
			m_vAccessUserIDs.insert(UID);
	}
}

CHouse::CDoorManager::~CDoorManager()
{
	for(auto& p : m_vpEntDoors)
		delete p.second;
	m_vpEntDoors.clear();
	m_vAccessUserIDs.clear();
}

void CHouse::CDoorManager::Open(int Number)
{
	if(m_vpEntDoors.find(Number) != m_vpEntDoors.end())
		m_vpEntDoors[Number]->Open();
}

void CHouse::CDoorManager::Close(int Number)
{
	if(m_vpEntDoors.find(Number) != m_vpEntDoors.end())
		m_vpEntDoors[Number]->Close();
}

void CHouse::CDoorManager::Reverse(int Number)
{
	if(m_vpEntDoors.find(Number) != m_vpEntDoors.end())
	{
		if(m_vpEntDoors[Number]->IsClosed())
			Open(Number); // Open the door
		else
			Close(Number); // Close the door
	}
}

void CHouse::CDoorManager::OpenAll()
{
	for(auto& p : m_vpEntDoors)
		Open(p.first);
}

void CHouse::CDoorManager::CloseAll()
{
	for(auto& p : m_vpEntDoors)
		Close(p.first);
}

void CHouse::CDoorManager::ReverseAll()
{
	for(auto& p : m_vpEntDoors)
		Reverse(p.first);
}

void CHouse::CDoorManager::AddAccess(int UserID)
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

void CHouse::CDoorManager::RemoveAccess(int UserID)
{
	// try remove access
	if(m_vAccessUserIDs.erase(UserID) > 0)
		SaveAccessList();
}

bool CHouse::CDoorManager::HasAccess(int UserID)
{
	return m_pHouse->GetAccountID() == UserID || m_vAccessUserIDs.find(UserID) != m_vAccessUserIDs.end();
}

int CHouse::CDoorManager::GetAvailableAccessSlots() const
{
	return (int)MAX_HOUSE_DOOR_INVITED_PLAYERS - (int)m_vAccessUserIDs.size();
}

void CHouse::CDoorManager::SaveAccessList() const
{
	// initialize variables
	std::string AcessList;
	AcessList.reserve(m_vAccessUserIDs.size() * 8);

	// parse access user ids to string
	for(const auto& UID : m_vAccessUserIDs)
	{
		AcessList += std::to_string(UID);
		AcessList += ',';
	}

	// remove last comma character
	if(!AcessList.empty())
		AcessList.pop_back();

	// update
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "AccessList = '{}' WHERE ID = '{}'", AcessList.c_str(), m_pHouse->GetID());
}

void CHouse::CDoorManager::AddDoor(const char* pDoorname, vec2 Position)
{
	// Add the door to the m_vpEntDoors map using the door name as the key
	m_vpEntDoors.emplace(m_vpEntDoors.size() + 1, new CEntityHouseDoor(&GS()->m_World, m_pHouse, std::string(pDoorname), Position));
}

void CHouse::CDoorManager::RemoveDoor(const char* pDoorname, vec2 Position)
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
CGS* CHouse::CDecorationManager::GS() const { return m_pHouse->GS(); }
CHouse::CDecorationManager::CDecorationManager(CHouse* pHouse) : m_pHouse(pHouse)
{
	CDecorationManager::Init();
}

CHouse::CDecorationManager::~CDecorationManager()
{
	delete m_pDrawBoard;
}

void CHouse::CDecorationManager::Init()
{
	// Create a new instance of CEntityDrawboard and pass the world and house position as parameters
	m_pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_pHouse->GetPos(), m_pHouse->GetRadius());
	m_pDrawBoard->RegisterEvent(&CDecorationManager::DrawboardToolEventCallback, m_pHouse);
	m_pDrawBoard->SetFlags(DRAWBOARDFLAG_PLAYER_ITEMS);

	// Load from database decorations
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_HOUSES_DECORATION_TABLE, "WHERE WorldID = '{}' AND HouseID = '{}'", GS()->GetWorldID(), m_pHouse->GetID());
	while(pRes->next())
	{
		int ItemID = pRes->getInt("ItemID");
		vec2 Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));

		// Add a point to the drawboard with the position and item ID
		m_pDrawBoard->AddPoint(Pos, ItemID);
	}
}

bool CHouse::CDecorationManager::StartDrawing(CPlayer* pPlayer) const
{
	return pPlayer && pPlayer->GetCharacter() && m_pDrawBoard->StartDrawing(pPlayer);
}

bool CHouse::CDecorationManager::HasFreeSlots() const
{
	return m_pDrawBoard->GetEntityPoints().size() < (int)MAX_DECORATIONS_PER_HOUSE;
}

bool CHouse::CDecorationManager::DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	const auto pHouse = (CHouse*)pUser;
	if(!pPlayer || !pHouse)
		return false;

	const int& ClientID = pPlayer->GetCID();

	if(pPoint)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(Event == DrawboardToolEvent::OnPointAdd)
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

		if(Event == DrawboardToolEvent::OnPointErase)
		{
			if(pHouse->GetDecorationManager()->Remove(pPoint))
			{
				pHouse->GS()->Chat(ClientID, "You have removed {} from your house!", pPlayerItem->Info()->GetName());
				return true;
			}

			return false;
		}
	}

	if(Event == DrawboardToolEvent::OnEnd)
	{
		pHouse->GS()->Chat(ClientID, "You have finished decorating your house!");
		return true;
	}

	return true;
}

bool CHouse::CDecorationManager::Add(const EntityPoint* pPoint) const
{
	// Check if pPoint or pPoint->m_pEntity is null
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// Get ItemID pEntity and EntityPos
	const CEntity* pEntity = pPoint->m_pEntity;
	const ItemIdentifier& ItemID = pPoint->m_ItemID;
	const vec2& EntityPos = pEntity->GetPos();

	// Execute a database insert query with the values
	Database->Execute<DB::INSERT>(TW_GUILD_HOUSES_DECORATION_TABLE, "(ItemID, HouseID, PosX, PosY, WorldID) VALUES ('{}', '{}', '{}', '{}', '{}')",
		ItemID, m_pHouse->GetID(), round_to_int(EntityPos.x), round_to_int(EntityPos.y), GS()->GetWorldID());
	return true;
}

bool CHouse::CDecorationManager::Remove(const EntityPoint* pPoint) const
{
	if(!pPoint || !pPoint->m_pEntity)
		return false;

	// Remove the decoration from the database
	Database->Execute<DB::REMOVE>(TW_HOUSES_DECORATION_TABLE, "WHERE HouseID = '{}' AND ItemID = '{}' AND PosX = '{}' AND PosY = '{}'",
		m_pHouse->GetID(), pPoint->m_ItemID, round_to_int(pPoint->m_pEntity->GetPos().x), round_to_int(pPoint->m_pEntity->GetPos().y));
	return true;
}