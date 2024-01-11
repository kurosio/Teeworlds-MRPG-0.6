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

bool CGuildHouseDecorationManager::StartDrawing(const int& ItemID, CPlayer* pPlayer)
{
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;

	const vec2& MousePos = pPlayer->GetCharacter()->GetMousePos();
	auto* pEntity = new CEntityHouseDecoration(&GS()->m_World, MousePos, -1, ItemID);
	pEntity->StartDrawingMode(pPlayer, HouseType::GUILD, m_pHouse->GetPos(), 900.f);
	return true;
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
				m_apDecorations[i] = new CEntityHouseDecoration(&GS()->m_World, DecorationPos, DecorationID, ItemID);
				break;
			}
		}
	}
}

bool CGuildHouseDecorationManager::Add(CEntityHouseDecoration* pEntity)
{
	if(!pEntity)
		return false;

	const ItemIdentifier& ItemID = pEntity->GetItemID();
	const vec2& EntityPos = pEntity->GetPos();

	// Check if the distance between the current position house and the given position (Position) is greater than 400.0f
	if(distance(m_pHouse->GetPos(), EntityPos) > 900.f)
	{
		//GS()->Chat(pPlayerBy->GetCID(), "There is too much distance from home!");
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
				InitID, ItemID, m_pHouse->GetID(), (int)EntityPos.x, (int)EntityPos.y, GS()->GetWorldID());

			// Create new decoration on gameworld
			pEntity->SetUniqueID(InitID);
			m_apDecorations[i] = pEntity;
			return true;
		}
	}

	// Send what all decoration slots occupied
	//GS()->Chat(pPlayerBy->GetCID(), "All decoration slots have been occupied.");
	return false;
}

bool CGuildHouseDecorationManager::Remove(HouseDecorationIdentifier ID)
{
	// Remove decoration
	for(int i = 0; i < MAX_DECORATIONS_HOUSE; i++)
	{
		if(m_apDecorations[i] && m_apDecorations[i]->GetUniqueID() == ID)
		{
			// Delete from gameworld
			delete m_apDecorations[i];
			m_apDecorations[i] = nullptr;

			// Remove from database
			Database->Execute<DB::REMOVE>(TW_GUILD_HOUSES_DECORATION_TABLE, "WHERE ID = '%d'", ID);
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
