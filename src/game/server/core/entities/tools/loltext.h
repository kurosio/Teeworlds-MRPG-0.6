#ifndef GAME_SERVER_ENTITIES_LOLTEXT_H
#define GAME_SERVER_ENTITIES_LOLTEXT_H

#include <game/server/entity.h>

class CLolPlasma : public CEntity
{
	CEntity* m_pParent;
	int m_Life;
	vec2 m_StartOff; // initial offset from parent, for proper following

public:
	CLolPlasma(CGameWorld* pGameWorld, CEntity* pParent, vec2 Pos, int Lifespan);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

class CLoltext
{
public:
	static void Create(CGameWorld* pGameWorld, CEntity* pParent, vec2 Pos, int Lifespan, const char* pText);
};

#endif

