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
	delete m_pBankManager;
}

void CHouse::InitProperties(BigInt Bank, const std::string& AccessList, const std::string& DoorsData, const std::string& FarmzonesData, const std::string& PropertiesData)
{
	// assert main properties string
	dbg_assert(PropertiesData.length() > 0, "The properties string is empty");


	// parse properties
	mystd::json::parse(PropertiesData, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("position") != pJson.end(), "The importal properties value is empty");
		m_Position = pJson.value("position", vec2());
		m_TextPosition = pJson.value("text_position", vec2());
	});


	// initialize components
	m_pBankManager = new CBankManager(this, Bank);
	m_pDoorManager = new CDoorManager(this, AccessList, DoorsData);
	m_pDecorationManager = new CDecorationManager(this, TW_GUILD_HOUSES_DECORATION_TABLE);
	m_pFarmzonesManager = new CFarmzonesManager(FarmzonesData);

	// Asserts
	dbg_assert(m_pBankManager != nullptr, "The house bank is null");
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
		m_pBankManager->Reset();
		pPlayer->Account()->ReinitializeHouse();
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '{}', Bank = '0', AccessList = NULL WHERE ID = '{}'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "'{}' becomes the owner of the house class '{}'.", Server()->ClientName(ClientID), GetClassName());
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
}

void CHouse::Sell()
{
	// check is has owner
	if(!HasOwner())
		return;

	// send mail
	BigInt ReturnsGold = std::max((BigInt)1, m_pBankManager->Get());
	MailWrapper Mail("System", m_AccountID, "House is sold.");
	Mail.AddDescLine("Your house is sold.");
	mystd::process_bigint_in_chunks<int>(ReturnsGold, [&Mail](int chunk)
	{
		Mail.AttachItem(CItem(itGold, chunk));
	});
	Mail.Send();

	// Update the house data
	if(CPlayer* pPlayer = GetPlayer())
	{
		pPlayer->Account()->ReinitializeHouse(true);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
	}
	m_pDoorManager->CloseAll();
	m_pBankManager->Reset();
	m_AccountID = -1;
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = NULL, Bank = '0', AccessList = NULL WHERE ID = '{}'", m_ID);

	// send information
	GS()->ChatAccount(m_AccountID, "Your House is sold!");
	GS()->Chat(-1, "House: '{}' have been is released!", m_ID);
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
		if(!m_pBankManager->Spend(GetRentPrice()))
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
	return (DoorCount * 400) + (FarmzoneCount * 400);
}

/* -------------------------------------
 * Door impl
 * ------------------------------------- */
CGS* CHouse::CDoorManager::GS() const { return m_pHouse->GS(); }
CPlayer* CHouse::CDoorManager::GetPlayer() const { return m_pHouse->GetPlayer(); }
CHouse::CDoorManager::CDoorManager(CHouse* pHouse, const std::string& AccessList, const std::string& DoorsData) : m_pHouse(pHouse)
{
	// parse doors the JSON string
	mystd::json::parse(DoorsData, [this](nlohmann::json& pJson)
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

