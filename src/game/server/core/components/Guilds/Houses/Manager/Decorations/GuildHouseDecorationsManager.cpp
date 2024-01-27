/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseDecorationsManager.h"

#include <game/server/core/components/Guilds/Houses/GuildHouseData.h>
#include <game/server/core/entities/Tools/draw_board.h>
#include <game/server/gamecontext.h>

CGS* CGuildHouseDecorationManager::GS() const { return m_pHouse->GS(); }

CGuildHouseDecorationManager::CGuildHouseDecorationManager(CGuildHouseData* pHouse) : m_pHouse(pHouse)
{
	CGuildHouseDecorationManager::Init();
}

CGuildHouseDecorationManager::~CGuildHouseDecorationManager()
{
	delete m_pDrawBoard;
}

void CGuildHouseDecorationManager::Init()
{
	// Create a new instance of CEntityDrawboard and pass the world and house position as parameters
	m_pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_pHouse->GetPos(), m_pHouse->GetRadius());
	m_pDrawBoard->RegisterEvent(&CGuildHouseDecorationManager::DrawboardToolEventCallback, m_pHouse);
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

bool CGuildHouseDecorationManager::StartDrawing(CPlayer* pPlayer) const
{
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;
	return m_pDrawBoard->StartDrawing(pPlayer);
}

bool CGuildHouseDecorationManager::DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	const auto pHouse = (CGuildHouseData*)pUser;
	if(!pPlayer || !pHouse)
		return false;

	const int& ClientID = pPlayer->GetCID();

	if(pPoint)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPoint->m_ItemID);
		if(Event == DrawboardToolEvent::ON_POINT_ADD)
		{
			if(pHouse->GetDecorations()->Add(pPoint))
			{
				pHouse->GS()->Chat(ClientID, "You have added {STR} to your house!", pPlayerItem->Info()->GetName());
				return true;
			}

			return false;
		}

		if(Event == DrawboardToolEvent::ON_POINT_ERASE)
		{
			if(pHouse->GetDecorations()->Remove(pPoint))
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

bool CGuildHouseDecorationManager::Add(const EntityPoint* pPoint) const
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

bool CGuildHouseDecorationManager::Remove(const EntityPoint* pPoint) const
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