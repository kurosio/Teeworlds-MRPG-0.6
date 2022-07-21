/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_GUILD_ENTITIES_DOOR_H
#include <game/server/entity.h>

class GuildDoor : public CEntity
{
	int m_GuildID;
public:
	GuildDoor(CGameWorld* pGameWorld, vec2 Pos, int HouseID);
	~GuildDoor() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
};


#endif