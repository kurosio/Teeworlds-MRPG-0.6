/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "nurse_heart.h"

#include <game/server/gamecontext.h>

// Constructor for CNurseHeart class
// Takes a pointer to a CGameWorld object and a ClientID as parameters
CNurseHeart::CNurseHeart(CGameWorld* pGameWorld, int ClientID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, {}, 0.0f) // Call CEntity constructor with appropriate parameters
{
	dbg_assert(ClientID >= MAX_PLAYERS, "CNurseHeart only for bot's"); // Check if ClientID is valid for CNurseHeart class

	m_ClientID = ClientID; // Assign ClientID to member variable m_ClientID
	GameWorld()->InsertEntity(this); // Insert current entity into the game world
}

void CNurseHeart::Tick()
{
	// Get the player object
	CPlayer* pPlayer = GS()->m_apPlayers[m_ClientID];

	// Check if the player or their character is null
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		// Destroy the nurse heart entity
		GameWorld()->DestroyEntity(this);
		return;
	}

	// Set the radius of the nurse heart
	float Radius = 24.f;

	// Get the angle of the player's target position
	float Angle = angle(normalize(vec2(pPlayer->GetCharacter()->m_Core.m_Input.m_TargetX, pPlayer->GetCharacter()->m_Core.m_Input.m_TargetY)));

	// Calculate the new position of the nurse heart
	m_Pos = pPlayer->GetCharacter()->GetPos() - vec2(Radius * cos(Angle + pi), Radius * sin(Angle + pi));
}

void CNurseHeart::Snap(int SnappingClient)
{
	// If SnappingClient is network clipped, return and exit the function
	if(NetworkClipped(SnappingClient))
		return;

	// Create a new CNetObj_Pickup object and assign it to pP using static_cast
	CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = 0;
	pP->m_Subtype = 0;
}
