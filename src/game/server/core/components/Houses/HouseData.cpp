/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseData.h"

#include <game/server/gamecontext.h>

#include <game/server/core/entities/items/jobitems.h>
#include <game/server/core/entities/tools/draw_board.h>

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
		pPlayer->Account()->ReinitializeHouse();
	}

	// Delete the CHouseData
	delete m_pDrawBoard;
	delete m_pDoorsController;
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
	if(!pPlayer)
		return false;

	return m_pDrawBoard->StartDrawing(pPlayer);
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
				pHouse->GS()->Chat(ClientID, "You have added {STR} to your house!", pPlayerItem->Info()->GetName());
				return true;
			}

			return false;
		}

		if(Event == DrawboardToolEvent::ON_POINT_ERASE)
		{
			if(pHouse->RemoveDecoration(pPoint))
			{
				pHouse->GS()->Chat(ClientID, "You have removed {STR} from your house!", pPlayerItem->Info()->GetName());
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
		m_pDoorsController->CloseAll();
		m_pBank->Reset();
		pPlayer->Account()->ReinitializeHouse();
		Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "UserID = '%d', HouseBank = '0', AccessData = NULL WHERE ID = '%d'", m_AccountID, m_ID);

		// send information
		GS()->Chat(-1, "{STR} becomes the owner of the house class {STR}", Server()->ClientName(ClientID), GetClassName());
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**{STR} becomes the owner of the house class {STR}**", Server()->ClientName(ClientID), GetClassName());
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
	GS()->SendInbox("System", m_AccountID, "House is sold", "Your house is sold !", itGold, Price, 0);

	// Update the house data
	m_pDoorsController->CloseAll();
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