/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseDecorationsManager.h"
#include <game/server/gamecontext.h>

#include "game/server/mmocore/GameEntities/decoration_houses.h"
#include <game/server/mmocore/Components/Guilds/Houses/GuildHouseData.h>

CGS* CGuildHouseDecorationManager::GS() const { return m_pHouse->GS(); }

CGuildHouseDecorationManager::CGuildHouseDecorationManager(CGuildHouseData* pHouse) : m_pHouse(pHouse)
{
	CGuildHouseDecorationManager::Init();
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

void CGuildHouseDecorationManager::Init()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE WorldID = '%d' AND HouseID = '%d'", GS()->GetWorldID(), m_pHouse->GetID());
	while(pRes->next())
	{
		for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
		{
			if(!m_apDecorations[i])
			{
				int DecorationID = pRes->getInt("ID");
				int ItemID = pRes->getInt("ItemID");
				vec2 DecorationPos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
				m_apDecorations[i] = new CDecorationHouses(&GS()->m_World, DecorationPos, m_pHouse->GetID(), DecorationID, ItemID);
				break;
			}
		}
	}
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
			ResultPtr pRes2 = Database->Execute<DB::SELECT>("ID", TW_GUILD_HOUSES_DECORATION_TABLE, "ORDER BY ID DESC LIMIT 1");
			const int InitID = pRes2->next() ? pRes2->getInt("ID") + 1 : 1;
			Database->Execute<DB::INSERT>(TW_GUILD_HOUSES_DECORATION_TABLE, "(ID, ItemID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')", 
				InitID, ItemID, m_pHouse->GetID(), (int)Position.x, (int)Position.y, GS()->GetWorldID());

			// Create new decoration on gameworld
			m_apDecorations[i] = new CDecorationHouses(&GS()->m_World, Position, pPlayerBy->Account()->GetID(), InitID, ItemID);
			return true;
		}
	}

	// Send what all decoration slots occupied
	GS()->Chat(pPlayerBy->GetCID(), "All decoration slots have been occupied.");
	return false;
}

bool CGuildHouseDecorationManager::Remove(HouseDecorationIdentifier DecoID)
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
			Database->Execute<DB::REMOVE>(TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE ID = '%d'", DecoID);
			return true;
		}
	}

	return false;
}

HouseDecorationsContainer&& CGuildHouseDecorationManager::GetContainer() const
{
	HouseDecorationsContainer Decorations;
	Decorations.reserve(MAX_DECORATIONS_HOUSE);

	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i])
			Decorations.push_back(m_apDecorations[i]);
	}

	return std::move(Decorations);
}
