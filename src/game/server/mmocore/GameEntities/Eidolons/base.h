/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_EIDOLON_H
#define GAME_SERVER_ENTITIES_EIDOLON_H
#include <game/server/entity.h>

class CEidolon : public CEntity
{
	int m_EidolonCID;
	int m_OwnerCID;
	int m_Type;

	int m_TickMove;
	vec2 m_MoveTo;
	vec2 m_MovePos;
	enum
	{
		NUM_PARTICLES_AROUND_EIDOLON = 3
	};
	int m_IDs[NUM_PARTICLES_AROUND_EIDOLON];

public:
	CEidolon(CGameWorld* pGameWorld, vec2 Pos, int Type, int EidolonCID, int OwnerCID);
	~CEidolon() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
