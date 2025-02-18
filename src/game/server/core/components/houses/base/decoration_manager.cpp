#include "decoration_manager.h"
#include <game/server/gamecontext.h>

#include <game/server/core/entities/tools/draw_board.h>

static bool DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	// check valid player and manager
	auto* pManager = (CDecorationManager*)pUser;
	if(!pPlayer || !pManager)
		return false;

	// check valid house
	const auto* pHouse = pManager->GetHouse();
	if(!pHouse)
		return false;


	// add point event
	const int ClientID = pPlayer->GetCID();
	if(Event == DrawboardToolEvent::OnPointAdd && pPoint)
	{
		// check free slots
		const auto* pDecorationItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(!pManager->HasFreeSlots())
		{
			pManager->GS()->Chat(ClientID, "You have reached the maximum number of decorations for house!");
			return false;
		}

		// try add item
		if(pManager->Add(pPoint))
		{
			pManager->GS()->Chat(ClientID, "You have added '{}' to house!", pDecorationItem->Info()->GetName());
			return true;
		}

		return false;
	}

	// erase point event
	if(Event == DrawboardToolEvent::OnPointErase)
	{
		// try erase element
		const auto* pDecorationItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(pManager->Remove(pPoint))
		{
			pManager->GS()->Chat(ClientID, "You have removed '{}' from house!", pDecorationItem->Info()->GetName());
			return true;
		}

		return false;
	}

	// update drawing event
	if(Event == DrawboardToolEvent::OnUpdate)
	{
		const auto MousePos = pPlayer->GetCharacter()->GetMousePos();
		const auto optNumber = pManager->GS()->Collision()->GetSwitchTileNumberAtIndex(MousePos, TILE_SW_HOUSE_ZONE);

		if(!optNumber || (*optNumber != pHouse->GetID()))
		{
			pHouse->GS()->Chat(ClientID, "Editing is not allowed in your cursor area. Editor closed!");
			return false;
		}

		return true;
	}

	// end drawing event
	if(Event == DrawboardToolEvent::OnEnd)
	{
		pHouse->GS()->Chat(ClientID, "You have finished decorating your house!");
		return true;
	}

	return true;
}


CGS* CDecorationManager::GS() const
{
	return m_pHouse->GS();
}


CDecorationManager::~CDecorationManager()
{
	delete m_pDrawBoard;
}


void CDecorationManager::Init()
{
	// initialize drawboard
	m_pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_pHouse->GetPos(), 3200.f);
	m_pDrawBoard->RegisterEvent(&DrawboardToolEventCallback, this);
	m_pDrawBoard->SetFlags(DRAWBOARDFLAG_PLAYER_ITEMS);


	// load points from database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", m_DecorationTableName.c_str(), "WHERE WorldID = '{}' AND HouseID = '{}'", GS()->GetWorldID(), m_pHouse->GetID());
	while(pRes->next())
	{
		const auto ItemID = pRes->getInt("ItemID");
		const auto Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		m_pDrawBoard->AddPoint(Pos, ItemID);
	}
}


bool CDecorationManager::StartDrawing(CPlayer* pPlayer) const
{
	// check valid
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;


	// start drawing
	return m_pDrawBoard->StartDrawing(pPlayer);
}


bool CDecorationManager::EndDrawing(CPlayer* pPlayer)
{
	// check valid
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;


	// end drawing
	m_pDrawBoard->EndDrawing(pPlayer);
	return true;
}


bool CDecorationManager::HasFreeSlots() const
{
	const auto& sizePoints = m_pDrawBoard->GetEntityPoints().size();
	const auto& maxPoints = (size_t)MAX_DECORATIONS_PER_HOUSE;

	return sizePoints < maxPoints;
}


bool CDecorationManager::Add(const EntityPoint* pPoint) const
{
	// check valid point
	if(!pPoint || !pPoint->m_pEntity)
		return false;


	// initialize variables
	const auto* pEntity = pPoint->m_pEntity;
	const auto ItemID = pPoint->m_ItemID;
	const auto Pos = pEntity->GetPos();

	// Execute a database insert query with the values
	Database->Execute<DB::INSERT>(m_DecorationTableName.c_str(),
		"(ItemID, HouseID, PosX, PosY, WorldID) VALUES ('{}', '{}', '{}', '{}', '{}')",
		ItemID, m_pHouse->GetID(), round_to_int(Pos.x), round_to_int(Pos.y), GS()->GetWorldID());
	return true;
}


bool CDecorationManager::Remove(const EntityPoint* pPoint) const
{
	// check valid
	if(!pPoint || !pPoint->m_pEntity)
		return false;


	// initialize variables
	const auto* pEntity = pPoint->m_pEntity;
	const auto ItemID = pPoint->m_ItemID;
	const auto Pos = pEntity->GetPos();

	// Remove the decoration from the database
	Database->Execute<DB::REMOVE>(m_DecorationTableName.c_str(),
		"WHERE HouseID = '{}' AND ItemID = '{}' AND PosX = '{}' AND PosY = '{}'",
		m_pHouse->GetID(), ItemID, round_to_int(Pos.x), round_to_int(Pos.y));
	return true;
}