#ifndef GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#include <game/server/entity.h>

#include "../base/interface_house.h"

class CEntityHouseDoor : public CEntity
{
	enum States
	{
		CLOSED,
		OPENED
	};

	IHouse* m_pHouse {};
	std::string m_Name {};
	vec2 m_PosControll {};
	int m_State {};

public:
	CEntityHouseDoor(CGameWorld* pGameWorld, IHouse* pHouse, const std::string& Name, vec2 Pos);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Open() { m_State = OPENED; }
	void Close() { m_State = CLOSED; }
	void Reverse();
	bool IsClosed() const { return m_State == CLOSED; }
	std::string GetName() { return m_Name; }

	bool PlayerHouseTick();
	bool GuildHouseTick();
};

#endif