/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_DROPINGITEMS_H
#define GAME_SERVER_ENTITIES_DROPINGITEMS_H
#include <game/server/entity.h>

class CDropItem : public CEntity
{
	enum
	{
		NUM_IDS = 4,
	};
	int m_IDs[NUM_IDS];

	vec2 m_Vel;
	float m_Angle;
	float m_AngleForce;
	bool m_Flashing;
	int m_LifeSpan;
	int m_FlashTimer;

	CItemData m_DropItem;
	int m_OwnerID;

public:
	CDropItem(class CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, CItemData DropItem, int OwnerID);
	~CDropItem() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

	bool TakeItem(int ClientID);
};

#endif
