/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseDecorationsManager.h"
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Guilds/Houses/GuildHouseData.h>

CGS* CGuildHouseDecorationManager::GS() const { return m_pHouse->GS(); }

CGuildHouseDecorationManager::CGuildHouseDecorationManager(CGuildHouseData* pHouse) : m_pHouse(pHouse)
{
}

CGuildHouseDecorationManager::~CGuildHouseDecorationManager()
{
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i])
		{
			delete m_apDecorations[i];
			m_apDecorations[i] = nullptr;
		}
	}
}

HouseDecorationsContainer&& CGuildHouseDecorationManager::GetContainer() const
{
	HouseDecorationsContainer Decorations;
	Decorations.reserve(MAX_DECORATIONS_HOUSE);

	for(int i = 0; i < MAX_DECORATIONS_HOUSE)
	{
		if(m_apDecorations[i])
			Decorations.push_back(m_apDecorations[i]);
	}
	
	return std::move(Decorations);
}


bool CGuildHouseDecorationManager::Add(int ItemID, vec2 Position, CPlayer* pPlayerBy)
{
	if(!pPlayerBy)
		return false;

	// Check if the distance between the current position house and the given position (Position) is greater than 400.0f
	if(distance(m_pHouse->GetPos(), Position) > 400.0f)
	{
		GS()->Chat(pPlayerBy->GetCID(), "There is too much distance from home!");
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