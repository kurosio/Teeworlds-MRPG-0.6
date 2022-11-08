/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_NPCWALL_H
#define GAME_SERVER_ENTITIES_NPCWALL_H
#include <game/server/entity.h>

class CNPCWall : public CEntity
{
public:
	enum Flags
	{
		NPC_BOT = 1 << 0,
		QUEST_BOT = 1 << 1,
		MOB_BOT = 1 << 2,
		FRIENDLY_BOT = NPC_BOT|QUEST_BOT,
		AGRESSED_BOT = MOB_BOT
	};

	CNPCWall(CGameWorld *pGameWorld, vec2 Pos, bool Left, int Flag);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	int m_Flag;
	bool m_Active;
};

#endif
