#include "decoration_houses.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "laser_orbite.h"

#include <game/server/mmocore/Components/Houses/HouseData.h>
#include <game/server/mmocore/Components/Guilds/GuildData.h>
#include <game/server/mmocore/Components/Inventory/InventoryManager.h>

// Constructor for the CDrawingData class in the CEntityHouseDecoration namespace
// Takes a pointer to a CPlayer object, a DrawingType enum value, a vec2 Position, and a float Radius as parameters
CEntityHouseDecoration::CDrawingData::CDrawingData(CPlayer* pPlayer, vec2 Position, float Radius)
	: m_pPlayer(pPlayer)
{
	// Set the member variables
	m_Position = Position;
	m_Radius = Radius;
	m_pZoneOrbite = nullptr;
	m_Working = true;
}

// Destructor for the CDrawingData class in the CEntityHouseDecoration namespace
CEntityHouseDecoration::CDrawingData::~CDrawingData()
{
	m_pPlayer = nullptr;

	// Delete the m_pZoneOrbite object
	delete m_pZoneOrbite;
	delete m_pEraseOrbite;
}

int CEntityHouseDecoration::CDrawingData::NextItemPos()
{
	m_ItemPos++;
	if(m_ItemPos >= (int)m_vFullDecorationItemlist.size())
		m_ItemPos = 0;
	return m_vFullDecorationItemlist[m_ItemPos];
}

int CEntityHouseDecoration::CDrawingData::PrevItemPos()
{
	m_ItemPos--;
	if(m_ItemPos < 0)
		m_ItemPos = (int)m_vFullDecorationItemlist.size() - 1;
	return m_vFullDecorationItemlist[m_ItemPos];
}

CEntityHouseDecoration::CEntityHouseDecoration(CGameWorld* pGameWorld, vec2 Pos, int UniqueID, int GroupID, int ItemID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DECOHOUSE, Pos)
{
	m_UniqueID = UniqueID;
	m_ItemID = ItemID;
	m_GroupID = GroupID;
	m_pDrawing = nullptr;
	CEntityHouseDecoration::ReinitilizeSnappingIDs();
	GameWorld()->InsertEntity(this);
}

CEntityHouseDecoration::~CEntityHouseDecoration()
{
	if(!m_vIDs.empty())
	{
		for(const auto& ID : m_vIDs)
			Server()->SnapFreeID(ID);
	}

	delete m_pDrawing;
}

// This function starts the drawing mode for a house decoration entity
// It takes a pointer to a player, the type of house decoration, the position of the decoration, and the radius of the decoration
void CEntityHouseDecoration::StartDrawingMode(CPlayer* pPlayer, const vec2& CenterPos, float Radius)
{
	// Check if the player pointer is valid and if there is no ongoing drawing
	if(pPlayer && !m_pDrawing)
	{
		// Initilize static default decoration item's list
		if(CEntityHouseDecoration::m_vFullDecorationItemlist.empty())
		{
			CEntityHouseDecoration::m_vFullDecorationItemlist = GS()->Mmo()->Item()->GetItemIDsByType(ItemType::TYPE_DECORATION);
		}

		// Get the client ID of the player
		const int& ClientID = pPlayer->GetCID();

		// Create a new drawing data object and laser orbite and assign it to the m_pDrawing pointer
		m_pDrawing = new CDrawingData(pPlayer, CenterPos, Radius);
		m_pDrawing->m_pZoneOrbite = new CLaserOrbite(GameWorld(), CenterPos, 15, EntLaserOrbiteType::INSIDE_ORBITE, 0.f, Radius, LASERTYPE_FREEZE, CmaskOne(ClientID));
	}
}

void CEntityHouseDecoration::Tick()
{
	// If m_pDrawing is null, return
	if(!m_pDrawing || IsMarkedForDestroy())
		return;

	// Get the player from m_pDrawing
	CPlayer* pPlayer = m_pDrawing->m_pPlayer;

	// If m_Working is true and either pPlayer is null or pPlayer's character is null, destroy the entity and return
	if(m_pDrawing->m_Working && (!pPlayer || !pPlayer->GetCharacter()))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// Get the client ID, player's item and character object of the player
	const int& ClientID = pPlayer->GetCID();
	CPlayerItem* pPlayerItem = pPlayer->GetItem(m_ItemID);
	CCharacter* pCharacter = pPlayer->GetCharacter();

	if(m_pDrawing->m_Working)
	{
		// Set position by the current character mouse position
		m_Pos = pCharacter->GetMousePos();

		// Check if the player has moved too far away from the drawing zone
		if(distance(m_pDrawing->m_Position, m_Pos) > (m_pDrawing->m_Radius + 300.f))
		{
			GS()->Chat(ClientID, "You've left the zone, drawing mode has been canceled.", pPlayerItem->Info()->GetName());
			GameWorld()->DestroyEntity(this);
			return;
		}

		// Check if the erase mode is not enabled in the drawing object
		if(!m_pDrawing->m_EraseMode)
		{
			// Check if the next weapon key is clicked
			if(pPlayer->IsClickedKey(KEY_EVENT_NEXT_WEAPON))
			{
				m_ItemID = m_pDrawing->NextItemPos();
				ReinitilizeSnappingIDs();
				return;
			}

			// Check if the previous weapon key is clicked
			if(pPlayer->IsClickedKey(KEY_EVENT_PREV_WEAPON))
			{
				m_ItemID = m_pDrawing->PrevItemPos();
				ReinitilizeSnappingIDs();
				return;
			}
		}

		// Check if the menu key is clicked
		if(pPlayer->IsClickedKey(KEY_EVENT_MENU))
		{
			GS()->Chat(ClientID, "Drawing mode has been canceled.", pPlayerItem->Info()->GetName());
			GS()->UpdateVotes(ClientID, pPlayer->m_LastVoteMenu);
			GameWorld()->DestroyEntity(this);
			return;
		}

		// Check if the vote yes key is clicked
		if(pPlayer->IsClickedKey(KEY_EVENT_VOTE_YES))
		{
			m_pDrawing->m_EraseMode ^= true;
			if(!m_pDrawing->m_EraseMode)
			{
				delete m_pDrawing->m_pEraseOrbite;
				m_pDrawing->m_pEraseOrbite = nullptr;
			}
			else if(!m_pDrawing->m_pEraseOrbite)
			{
				m_pDrawing->m_pEraseOrbite = new CLaserOrbite(GameWorld(), -1, this, 4, EntLaserOrbiteType::INSIDE_ORBITE, 0.1f, 32.f, LASERTYPE_DOOR, -1);
			}
			GS()->Chat(ClientID, "Switching mode to '{STR}'.", m_pDrawing->m_EraseMode ? "Erase mode" : "Drawing mode");
		}

		// Check if the fire hammer key is clicked
		if(pPlayer->IsClickedKey(KEY_EVENT_FIRE_HAMMER))
		{
			// Check if erase mode is enabled
			if(m_pDrawing->m_EraseMode)
			{
				if(const auto pEntity = FindByGroupID(m_GroupID))
				{
					if(m_DrawToolEvent.m_Callback(true, pEntity, pPlayer, pEntity->GetItemID(), m_DrawToolEvent.m_pData))
					{
						pEntity->MarkForDestroy();
					}
				}
			}
			// Check if erase mode is disabled
			else if(m_DrawToolEvent.m_Callback(false, this, pPlayer, m_ItemID, m_DrawToolEvent.m_pData))
			{
				m_pDrawing->m_Working = false;
			}
		}

		// broadcast
		if(m_pDrawing->m_EraseMode)
		{
			GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 50, "Drawing with: Erase mode"
				"\nKey 'fire hammer' - remove decoration"
				"\nKey 'menu' - cancel drawing mode"
				"\nKey 'vote yes' - drawing mode",
				pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
		}
		else
		{
			GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 50, "Drawing with: {STR} (has {VAL})"
				"\nKey 'fire hammer' - add selected decoration"
				"\nKey 'menu' - cancel drawing mode"
				"\nKey 'prev, next weapon' - switch decoration"
				"\nKey 'vote yes' - erase mode",
				pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
		}
	}

	if(!m_pDrawing->m_Working)
	{
		auto* pEntDeco = new CEntityHouseDecoration(GameWorld(), m_Pos, m_UniqueID, m_GroupID, m_ItemID);
		pEntDeco->RegisterDrawToolCallback(m_DrawToolEvent.m_Callback, m_DrawToolEvent.m_pData);
		pEntDeco->StartDrawingMode(pPlayer, m_pDrawing->m_Position, m_pDrawing->m_Radius);
		delete m_pDrawing;
		m_pDrawing = nullptr;
	}
}

void CEntityHouseDecoration::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || (m_pDrawing && m_pDrawing->m_EraseMode))
		return;

	int Type = POWERUP_HEALTH;
	int Subtype = 0;
	switch(m_ItemID)
	{
		case itDecoArmor:
		Type = POWERUP_ARMOR;
		break;
		case itDecoHeartElite:
		Type = POWERUP_HEALTH;
		break;
		case itDecoNinjaElite:
		Type = POWERUP_NINJA;
		break;
		default:
		Type = POWERUP_HEALTH;
		break;
	}

	ObjectType Objtype = GetObjectType();
	if(Objtype == OBJ_PICKUP)
	{
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
		if(!pP)
			return;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = Type;
		pP->m_Subtype = Subtype;
	}

	if(!m_vIDs.empty())
	{
		float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) / 10.0f;
		float AngleStep = 2.0f * pi / (int)m_vIDs.size();
		float Radius = 30.0f;
		for(int i = 0; i < (int)m_vIDs.size(); i++)
		{
			vec2 PosStart = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * i), Radius * sin(AngleStart + AngleStep * i));
			CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_vIDs[i], sizeof(CNetObj_Projectile)));
			if(!pObj)
				return;

			pObj->m_X = (int)PosStart.x;
			pObj->m_Y = (int)PosStart.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick() - 1;
			pObj->m_Type = WEAPON_SHOTGUN;
		}
	}
}

int CEntityHouseDecoration::GetIDsNum() const
{
	switch(m_ItemID)
	{
		case itDecoNinjaElite:
		case itDecoHeartElite: return 3;
		default: return 0;
	}
}

CEntityHouseDecoration::ObjectType CEntityHouseDecoration::GetObjectType() const
{
	/*switch(m_ItemID)
	{
		default: return OBJ_PICKUP;
	}*/
	return OBJ_PICKUP;
}

void CEntityHouseDecoration::ReinitilizeSnappingIDs()
{
	if(const int& NumIDs = GetIDsNum(); NumIDs <= 0 && !m_vIDs.empty())
	{
		m_vIDs.clear();
		m_vIDs.shrink_to_fit();
	}
	else if(NumIDs != (int)m_vIDs.size())
	{
		if(!m_vIDs.empty())
		{
			for(auto& ID : m_vIDs)
			{
				Server()->SnapFreeID(ID);
				ID = -1;
			}
		}

		m_vIDs.resize(NumIDs);
		for(auto& ID : m_vIDs)
		{
			ID = Server()->SnapNewID();
		}
	}
}

CEntityHouseDecoration* CEntityHouseDecoration::FindByGroupID(int GroupID)
{
	auto pEntity = (CEntityHouseDecoration*)GS()->m_World.ClosestEntity(m_Pos, 20.0f, CGameWorld::ENTTYPE_DECOHOUSE, this);
	if(!pEntity || pEntity->GetGroupID() != GroupID)
		return nullptr;
	return pEntity;
}
