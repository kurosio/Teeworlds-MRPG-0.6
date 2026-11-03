/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_DROP_PICKUP_H
#define GAME_SERVER_CORE_ENTITIES_ITEMS_DROP_PICKUP_H
#include <game/server/entity.h>

class CEntityDropPickup : public CEntity
{
	int m_Type;
	int m_Subtype;
	int m_Value;
	int m_LifeSpan;
	CFlashingTick m_Flash;
	bool m_FromMap {};

public:
	CEntityDropPickup(CGameWorld* pGameWorld, vec2 Pos, vec2 Vel, int Type, int Subtype, int Value);

	// Detached map pickups restore health/mana/ammo, rather than granting loot XP.
	CEntityDropPickup(CGameWorld* pGameWorld, vec2 Pos, int Type, int Subtype);
	~CEntityDropPickup() override;
	bool IsVisibleForClient(int ClientID) const { return !m_Flash.IsFlashing() && (ClientID == -1 || IsValidSnappingState(ClientID)); }

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
