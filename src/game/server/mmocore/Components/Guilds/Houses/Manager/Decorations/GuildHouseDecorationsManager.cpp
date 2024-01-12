/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseDecorationsManager.h"
#include <game/server/gamecontext.h>

#include "game/server/mmocore/GameEntities/decoration_houses.h"
#include <game/server/mmocore/Components/Guilds/Houses/GuildHouseData.h>

CGS* CGuildHouseDecorationManager::GS() const { return m_pHouse->GS(); }

CGuildHouseDecorationManager::CGuildHouseDecorationManager(CGuildHouseData* pHouse) : m_pHouse(pHouse)
{
	m_apDecorations.reserve(MAX_DECORATIONS_HOUSE);
	CGuildHouseDecorationManager::Init();
}

CGuildHouseDecorationManager::~CGuildHouseDecorationManager()
{
	for(auto p : m_apDecorations)
	{
		delete p;
	}
	m_apDecorations.clear();
	m_apDecorations.shrink_to_fit();
}

bool CGuildHouseDecorationManager::StartDrawing(const int& ItemID, CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;

	const vec2& MousePos = pPlayer->GetCharacter()->GetMousePos();
	auto* pEntity = new CEntityHouseDecoration(&GS()->m_World, MousePos, -1, m_pHouse->GetID(), ItemID);
	pEntity->RegisterDrawTool(&CGuildHouseDecorationManager::DrawToolCallback, m_pHouse);
	pEntity->StartDrawingMode(pPlayer, m_pHouse->GetPos(), 900.f);
	return true;
}

bool CGuildHouseDecorationManager::DrawToolCallback(bool EraseMode, CEntityHouseDecoration* pEntity, CPlayer* pPlayer, int DecorationItemID, void* pUser)
{
	// Check if pPlayer or pHouse is null
	auto pHouse = (CGuildHouseData*)pUser;
	if(!pPlayer || !pHouse)
		return false;

	// Get the player item with the given DecorationItemID
	CPlayerItem* pPlayerItem = pPlayer->GetItem(DecorationItemID);

	// If EraseMode is true and the decoration is successfully removed from the house
	if(EraseMode && pHouse->GetDecorations()->Remove(pEntity))
	{
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You returned {STR} to your inventory!", pPlayerItem->Info()->GetName());
		pPlayerItem->Add(1);
		return true;
	}

	// If EraseMode is false and the decoration is successfully added to the house
	if(!EraseMode && pHouse->GetDecorations()->Add(pEntity))
	{
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You have added {STR} to your house!", pPlayerItem->Info()->GetName());
		pPlayerItem->Remove(1);
		return true;
	}

	return false;
}

void CGuildHouseDecorationManager::Init()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_pHouse->GetID());
	while(pRes->next())
	{
		if(!HasFreeSlots())
			break;

		int UniqueID = pRes->getInt("ID");
		int ItemID = pRes->getInt("ItemID");
		GuildHouseIdentifier HouseID = pRes->getInt("HouseID");
		vec2 DecorationPos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		m_apDecorations.push_back(new CEntityHouseDecoration(&GS()->m_World, DecorationPos, UniqueID, HouseID, ItemID));
	}
}

bool CGuildHouseDecorationManager::Add(CEntityHouseDecoration* pEntity)
{
	if(!pEntity)
	{
		return false;
	}

	const ItemIdentifier& ItemID = pEntity->GetItemID();
	const vec2& EntityPos = pEntity->GetPos();

	// Check if the distance between the current position house and the given position (Position) is greater than 400.0f
	if(distance(m_pHouse->GetPos(), EntityPos) > 900.f)
	{
		//GS()->Chat(pPlayerBy->GetCID(), "There is too much distance from home!");
		return false;
	}

	if(HasFreeSlots())
	{
		// Insert to last identifier and got it
		ResultPtr pRes2 = Database->Execute<DB::SELECT>("ID", TW_GUILD_HOUSES_DECORATION_TABLE, "ORDER BY ID DESC LIMIT 1");
		const int InitID = pRes2->next() ? pRes2->getInt("ID") + 1 : 1;
		Database->Execute<DB::INSERT>(TW_GUILD_HOUSES_DECORATION_TABLE, "(ID, ItemID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')",
			InitID, ItemID, m_pHouse->GetID(), (int)EntityPos.x, (int)EntityPos.y, GS()->GetWorldID());

		// Create new decoration on gameworld
		pEntity->SetUniqueID(InitID);
		m_apDecorations.push_back(pEntity);
		return true;
	}

	// Send what all decoration slots occupied
	//GS()->Chat(pPlayerBy->GetCID(), "All decoration slots have been occupied.");
	return false;
}

bool CGuildHouseDecorationManager::Remove(CEntityHouseDecoration* pEntity)
{
	// Check if the entity pointer is null
	if(!pEntity)
	{
		return false;
	}

	// Find the decoration in the list of decorations
	auto iterDecoration = std::find_if(m_apDecorations.begin(), m_apDecorations.end(), [&pEntity](const CEntityHouseDecoration* p)
	{
		return p->GetUniqueID() == pEntity->GetUniqueID();
	});

	// If the decoration is found
	if(iterDecoration != m_apDecorations.end())
	{
		// Get the unique ID of the decoration
		const int UniqueID = (*iterDecoration)->GetUniqueID();

		// Delete the decoration from the game world
		(*iterDecoration)->MarkForDestroy();
		m_apDecorations.erase(iterDecoration);

		// Remove the decoration from the database
		Database->Execute<DB::REMOVE>(TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE ID = '%d'", UniqueID);
		return true;
	}

	// If the decoration is not found, return false
	return false;
}