/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_GUILD_ENTITIES_DOOR_H
#include <game/server/entity.h>

class CDoor;
class CGuildHouse;

class CEntityGuildDoor : public CEntity
{
	enum States
	{
		CLOSED,
		OPENED
	};

	std::string m_Name {};
	CGuildHouse* m_pHouse {};
	int m_State {};
	vec2 m_PosControll {};

public:
	CEntityGuildDoor(CGameWorld* pGameWorld, CGuildHouse* pHouse, std::string&& Name, vec2 Pos);
	void Reverse();

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Open() { m_State = OPENED; }
	void Close() { m_State = CLOSED; }
	bool IsClosed() const { return m_State == CLOSED; }
	std::string GetName() { return m_Name; }
};


#endif