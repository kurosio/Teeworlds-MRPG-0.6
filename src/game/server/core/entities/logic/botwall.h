/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_NPCWALL_H
#define GAME_SERVER_ENTITIES_NPCWALL_H

#include <game/server/entity.h>

class CBotWall : public CEntity
{
public:
	enum Flags
	{
		WALLLINEFLAG_NPC_BOT = 1 << 0,
		WALLLINEFLAG_QUEST_BOT = 1 << 1,
		WALLLINEFLAG_MOB_BOT = 1 << 2,

		WALLLINEFLAG_FRIENDLY_BOT = WALLLINEFLAG_NPC_BOT|WALLLINEFLAG_QUEST_BOT,
		WALLLINEFLAG_AGRESSED_BOT = WALLLINEFLAG_MOB_BOT
	};

	CBotWall(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, int Flag);
	void HitCharacter(CCharacter* pChar);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	int m_Flag;
	bool m_Active;
};

#endif
