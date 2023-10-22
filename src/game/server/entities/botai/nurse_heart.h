/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_BOTAI_NURSE_HEART_H
#define GAME_SERVER_ENTITIES_BOTAI_NURSE_HEART_H
#include <game/server/entity.h>

class CNurseHeart : public CEntity
{
public:
	// Constructor that takes a pointer to a CGameWorld object and an integer ClientID
	CNurseHeart(CGameWorld* pGameWorld, int ClientID);

	// Override the Tick() function from the CEntity class
	void Tick() override;

	// Override the Snap() function from the CEntity class
	void Snap(int SnappingClient) override;

private:
	int m_ClientID; // An integer variable to store the ClientID
};

#endif
