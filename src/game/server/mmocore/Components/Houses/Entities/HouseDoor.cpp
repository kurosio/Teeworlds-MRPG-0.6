/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseDoor.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Houses/HouseData.h"

CEntityHouseDoor::CEntityHouseDoor(CGameWorld* pGameWorld, vec2 Pos, CHouseDoor* pDoorInfo, CHouseData* pHouse)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PLAYER_HOUSE_DOOR, Pos), m_pHouse(pHouse), m_pDoorInfo(pDoorInfo)
{
	m_PosTo = GS()->Collision()->FindDirCollision(100, m_PosTo, 'y', '-');
	m_PosControll = Pos;
	m_State = CLOSED;
	GS()->CreateLaserOrbite(this, 4, EntLaserOrbiteType::DEFAULT, 0.f, 16.f, LASERTYPE_DOOR);
	GameWorld()->InsertEntity(this);
}

void CEntityHouseDoor::Tick()
{
	// Get the UID of the owner of the house
	int OwnerUID = m_pHouse->GetAccountID();

	// Check if the player object exists and if the player has a character
	CPlayer* pPlayer = GS()->GetPlayerByUserID(OwnerUID);
	if(pPlayer && pPlayer->GetCharacter())
	{
		// Check if the distance between the control position and the mouse position of the character is less than 64.0f
		CCharacter* pChar = pPlayer->GetCharacter();
		if(distance(m_PosControll, pChar->GetMousePos()) < 24.0f)
		{
			// Check if the character's reload timer is active
			if(pChar->m_ReloadTimer)
			{
				// Check the state of the door and perform the corresponding action
				if(m_State == OPENED)
					Close();
				else
					Open();
			}

			// Broadcast a game information message to the player and hammer hit effect at the position of the door control
			GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GAME_INFORMATION, 10, "Use 'fire.' To operate the door '{STR}'!", m_pDoorInfo->GetName());
		}
	}

	// Check if the door is opened
	if(m_State == CLOSED)
	{
		// Loop through all characters in the game world
		for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			vec2 IntersectPos;

			// Find the closest point on the line from the door position to the destination position to the character's position
			if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
			{
				// Calculate the distance between the intersect point and the character's position
				const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);

				// Check if the distance is within the door radius hit limit
				if(Distance <= g_Config.m_SvDoorRadiusHit)
				{
					// Skip characters who have access to the house door
					if(m_pHouse->GetDoorsController()->HasAccess(pChar->GetPlayer()->Account()->GetID()))
						continue;

					// Skip eidolon when the owner has access
					if(pChar->GetPlayer()->IsBot())
					{
						CPlayerBot* pPlayerBot = static_cast<CPlayerBot*>(pChar->GetPlayer());
						if(pPlayerBot->GetEidolonOwner() && m_pHouse->GetDoorsController()->HasAccess(pPlayerBot->GetEidolonOwner()->Account()->GetID()))
							continue;
					}

					// Set the character's door hit flag to true
					pChar->m_DoorHit = true;
				}
			}
		}
	}
}

void CEntityHouseDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || m_State == OPENED)
		return;

	if(GS()->GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser* pObj = static_cast<CNetObj_DDNetLaser*>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		pObj->m_ToX = int(m_Pos.x);
		pObj->m_ToY = int(m_Pos.y);
		pObj->m_FromX = int(m_PosTo.x);
		pObj->m_FromY = int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 2;
		pObj->m_Owner = -1;
		pObj->m_Type = LASERTYPE_DOOR;
	}
	else
	{
		CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = int(m_Pos.x);
		pObj->m_Y = int(m_Pos.y);
		pObj->m_FromX = int(m_PosTo.x);
		pObj->m_FromY = int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 2;
	}
}