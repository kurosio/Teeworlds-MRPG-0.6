/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#include <game/server/entity.h>

class CEntityHouseDoor : public CEntity
{
	enum States
	{
		CLOSED,
		OPENED
	};

	class CHouseData* m_pHouse {};
	class CHouseDoor* m_pDoorInfo {};
	int m_State {};
	vec2 m_PosControll {};

public:
	CEntityHouseDoor(CGameWorld* pGameWorld, vec2 Pos, class CHouseDoor* pDoorInfo, class CHouseData* pHouse);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Open() { m_State = OPENED; }
	void Close() { m_State = CLOSED; }
	bool IsClosed() const { return m_State == CLOSED; }
};

#endif