/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseData.h"

#include "game/server/gamecontext.h"
#include "game/server/mmocore/GameEntities/jobitems.h"
#include "game/server/mmocore/GameEntities/decoration_houses.h"

CGS* CHouseData::GS() const { return static_cast<CGS*>(Server()->GameServer(m_WorldID)); }
CPlayer* CHouseData::GetPlayer() const { return GS()->GetPlayerByUserID(m_AccountID); }

// Destructor for CHouseData class
CHouseData::~CHouseData()
{
	// Remove the owner before destruct
	CPlayer* pPlayer = GetPlayer();
	if(pPlayer)
	{
		m_AccountID = -1;
		pPlayer->Acc().ReinitializeHouse();
	}

	// Delete the CHouseData
	delete m_pDoorData;
	delete m_pBank;
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
		delete m_apDecorations[i];
}

void CHouseData::InitDecorations()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_HOUSES_DECORATION_TABLE, "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_ID);
	while(pRes->next())
	{
		for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
		{
			if(!m_apDecorations[i])
			{
				int DecorationID = pRes->getInt("ID");
				int ItemID = pRes->getInt("ItemID");
				vec2 DecorationPos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
				m_apDecorations[i] = new CDecorationHouses(&GS()->m_World, DecorationPos, m_ID, DecorationID, ItemID);
				break;
			}
		}
	}
}

bool CHouseData::AddDecoration(ItemIdentifier ItemID, vec2 Position)
{
	// Check if the distance between the current position house and the given position (Position) is greater than 400.0f
	if(distance(m_Pos, Position) > 400.0f)
	{
		GS()->Chat(GetPlayer()->GetCID(), "There is too much distance from home!");
		return false;
	}

	// Add decoration
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(!m_apDecorations[i])
		{
			// Insert to last identifier and got it
			ResultPtr pRes2 = Database->Execute<DB::SELECT>("ID", TW_HOUSES_DECORATION_TABLE, "ORDER BY ID DESC LIMIT 1");
			const int InitID = pRes2->next() ? pRes2->getInt("ID") + 1 : 1;
			Database->Execute<DB::INSERT>(TW_HOUSES_DECORATION_TABLE, "(ID, ItemID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')", InitID, ItemID, m_ID, (int)Position.x, (int)Position.y, GS()->GetWorldID());

			// Create new decoration on gameworld
			m_apDecorations[i] = new CDecorationHouses(&GS()->m_World, Position, m_AccountID, InitID, ItemID);
			return true;
		}
	}

	// Send what all decoration slots occupied
	GS()->Chat(GetPlayer()->GetCID(), "All decoration slots have been occupied.");
	return false;
}

bool CHouseData::RemoveDecoration(HouseDecorationIdentifier DecoID)
{
	// Remove decoration
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i] && m_apDecorations[i]->GetDecorationID() == DecoID)
		{
			// Delete from gameworld
			delete m_apDecorations[i];
			m_apDecorations[i] = nullptr;

			// Remove from database
			Database->Execute<DB::REMOVE>(TW_HOUSES_DECORATION_TABLE, "WHERE ID = '%d'", DecoID);
			return true;
		}
	}

	return false;
}

// This function represents the action of a player buying a house in the game.
void CHouseData::Buy(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	// Check if the player has a house
	if(pPlayer->Acc().HasHouse())
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
	if(pPlayer->SpendCurrency(m_Price))
	{
		// update data
		m_AccountID = pPlayer->Acc().GetID();
		m_pDoorData->Close();
		m_pBank->Reset();
		pPlayer->Acc().ReinitializeHouse();
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '%d', HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "{STR} becomes the owner of the house class {STR}", Server()->ClientName(ClientID), GetClassName());
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**{STR} becomes the owner of the house class {STR}**", Server()->ClientName(ClientID), GetClassName());
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
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
	GS()->SendInbox("System", m_AccountID, "House is sold", "Your house is sold !", itGold, Price, 0);

	// Update the house data
	m_pDoorData->Open();
	m_pBank->Reset();
	m_AccountID = -1;
	if(pPlayer)
	{
		pPlayer->Acc().ReinitializeHouse();
	}
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = NULL, HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_ID);

	// Send informations
	if(pPlayer)
	{
		GS()->Chat(pPlayer->GetCID(), "Your House is sold!");
		GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
	}
	GS()->Chat(-1, "House: {INT} have been is released!", m_ID);
	GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**[House: {INT}] have been sold!**", m_ID);
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

// It is used to display the list of decorations in the house.
void CHouseData::ShowDecorationList() const
{
	// If the player object does not exist, return
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	int ClientID = pPlayer->GetCID();
	GS()->AVL(ClientID, "null", "Select item for to put in inventory.");

	bool Found = false;
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i])
		{
			const int DecoID = m_apDecorations[i]->GetDecorationID();
			const vec2 DecoPos = m_apDecorations[i]->GetPos();
			CItemDescription* pItemDecoration = GS()->GetItemInfo(m_apDecorations[i]->GetItemID());
			GS()->AVD(ClientID, "DECORATION_HOUSE_DELETE", DecoID, pItemDecoration->GetID(), 1, "[Slot {INT}]: {STR} (x: {INT} y: {INT})", i + 1,
				pItemDecoration->GetName(), (int)DecoPos.x / 32, (int)DecoPos.y / 32);
			Found = true;
		}
	}

	// Check if the variable "Found" is false
	if(!Found)
	{
		GS()->AVL(ClientID, "null", "There are no decorations in your house.");
	}
}