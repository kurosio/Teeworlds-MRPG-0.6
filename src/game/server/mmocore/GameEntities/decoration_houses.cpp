#include "decoration_houses.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "laser_orbite.h"

#include <game/server/mmocore/Components/Houses/HouseData.h>
#include <game/server/mmocore/Components/Guilds/GuildData.h>

// Constructor for the CDrawingData class in the CEntityHouseDecoration namespace
// Takes a pointer to a CPlayer object, a HouseType enum value, a vec2 Position, and a float Radius as parameters
CEntityHouseDecoration::CDrawingData::CDrawingData(CPlayer* pPlayer, HouseType Type, vec2 Position, float Radius)
	: m_pPlayer(pPlayer), m_Type(Type)
{
	// Set the member variables
	m_Position = Position;
	m_Radius = Radius;
	m_pLaserOrbite = nullptr;
	m_Working = true;
	m_Cancel = false;
}

// Destructor for the CDrawingData class in the CEntityHouseDecoration namespace
CEntityHouseDecoration::CDrawingData::~CDrawingData()
{
	// Delete the m_pLaserOrbite object
	delete m_pLaserOrbite;
}

CEntityHouseDecoration::CEntityHouseDecoration(CGameWorld* pGameWorld, vec2 Pos, int UniqueID, int ItemID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DECOHOUSE, Pos)
{
	m_UniqueID = UniqueID;
	m_ItemID = ItemID;
	m_pDrawing = nullptr;

	if(GetObjectType(true) >= 0)
	{
		for(int i = 0; i < NUM_IDS; i++)
			m_IDs[i] = Server()->SnapNewID();
	}

	GameWorld()->InsertEntity(this);
}

CEntityHouseDecoration::~CEntityHouseDecoration()
{
	if(GetObjectType(true) >= 0)
	{
		for(int i = 0; i < NUM_IDS; i++)
			Server()->SnapFreeID(m_IDs[i]);
	}

	delete m_pDrawing;
}

// This function starts the drawing mode for a house decoration entity
// It takes a pointer to a player, the type of house decoration, the position of the decoration, and the radius of the decoration
void CEntityHouseDecoration::StartDrawingMode(CPlayer* pPlayer, HouseType Type, const vec2& HousePos, float Radius)
{
	// Check if the player pointer is valid and if there is no ongoing drawing
	if(pPlayer && !m_pDrawing)
	{
		// Get the client ID of the player
		const int& ClientID = pPlayer->GetCID();

		// Create a new drawing data object and laser orbite and assign it to the m_pDrawing pointer
		m_pDrawing = new CDrawingData(pPlayer, Type, HousePos, Radius);
		m_pDrawing->m_pLaserOrbite = new CLaserOrbite(GameWorld(), HousePos, 15, EntLaserOrbiteType::INSIDE_ORBITE, 0.f, Radius, LASERTYPE_FREEZE, CmaskOne(ClientID));
	}
}

void CEntityHouseDecoration::Tick()
{
	// If m_pDrawing is null, return
	if(!m_pDrawing)
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

		// Check if the player has the required item to continue drawing
		if(!pPlayerItem->HasItem())
		{
			GS()->Chat(ClientID, "The {STR} has ended, drawing mode has been canceled.", pPlayerItem->Info()->GetName());
			GameWorld()->DestroyEntity(this);
			return;
		}

		// Check if the menu key is clicked by the player
		if(pPlayer->IsClickedKey(KEY_EVENT_MENU))
		{
			GS()->Chat(ClientID, "Drawing mode has been canceled.", pPlayerItem->Info()->GetName());
			GS()->UpdateVotes(ClientID, pPlayer->m_LastVoteMenu);
			GameWorld()->DestroyEntity(this);
			return;
		}

		// Check if the player has moved too far away from the drawing zone
		if(distance(m_pDrawing->m_Position, m_Pos) > (m_pDrawing->m_Radius + 300.f))
		{
			GS()->Chat(ClientID, "You've left the zone, drawing mode has been canceled.", pPlayerItem->Info()->GetName());
			GameWorld()->DestroyEntity(this);
			return;
		}

		if(m_pDrawing->m_Type == HouseType::DEFAULT)
		{
			if(CHouseData* pHouse = pPlayer->Account()->GetHouse(); pHouse)
			{
				if(pPlayer->IsClickedKey(KEY_EVENT_FIRE_HAMMER) && pHouse->AddDecoration(this))
				{
					GS()->Chat(ClientID, "You have added {STR} to your house!", pPlayerItem->Info()->GetName());
					pPlayer->GetItem(m_ItemID)->Remove(1);
					m_pDrawing->m_Working = false;
				}

				// broadcast
			}
		}
		else if(m_pDrawing->m_Type == HouseType::GUILD)
		{
			if(CGuildData* pGuild = pPlayer->Account()->GetGuild(); pGuild && pGuild->HasHouse())
			{
				if(pPlayer->IsClickedKey(KEY_EVENT_FIRE_HAMMER) && pGuild->GetHouse()->GetDecorations()->Add(this))
				{
					GS()->Chat(ClientID, "You have added {STR} to your house!", pPlayerItem->Info()->GetName());
					pPlayer->GetItem(m_ItemID)->Remove(1);
					m_pDrawing->m_Working = false;
				}

				GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Drawing with: {STR} (has {VAL})",
					pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
				// broadcast
			}
		}

	}

	if(!m_pDrawing->m_Working)
	{
		auto* pEntDeco = new CEntityHouseDecoration(GameWorld(), m_Pos, m_UniqueID, m_ItemID);
		pEntDeco->StartDrawingMode(pPlayer, m_pDrawing->m_Type, m_pDrawing->m_Position, m_pDrawing->m_Radius);
		delete m_pDrawing;
		m_pDrawing = nullptr;
	}
}

void CEntityHouseDecoration::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(GetObjectType(true) <= -1)
	{
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
		if(!pP)
			return;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = GetObjectType(false);
		return;
	}

	float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) / 10.0f;
	float AngleStep = 2.0f * pi / (float)BODY;
	float Radius = 30.0f;
	for(int i = 0; i < BODY; i++)
	{
		vec2 PosStart = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * i), Radius * sin(AngleStart + AngleStep * i));
		CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_X = (int)PosStart.x;
		pObj->m_Y = (int)PosStart.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick() - 1;
		pObj->m_Type = GetObjectType(true);
	}


	CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_IDs[BODY], sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = GetObjectType(false);
}

int CEntityHouseDecoration::GetObjectType(bool Projectile) const
{
	if(m_ItemID == itDecoHeart)
	{
		return (Projectile ? -1 : static_cast<int>(POWERUP_HEALTH));
	}
	else if(m_ItemID == itDecoArmor)
	{
		return (Projectile ? -1 : static_cast<int>(POWERUP_ARMOR));
	}
	else if(m_ItemID == itEliteDecoHeart)
	{
		return (Projectile ? static_cast<int>(WEAPON_SHOTGUN) : static_cast<int>(POWERUP_HEALTH));
	}
	else if(m_ItemID == itEliteDecoNinja)
	{
		return (Projectile ? -1 : static_cast<int>(POWERUP_NINJA));
	}

	return -1;
}