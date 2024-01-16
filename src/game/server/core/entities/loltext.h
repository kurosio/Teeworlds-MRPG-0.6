#ifndef GAME_SERVER_ENTITIES_LOLTEXT_H
#define GAME_SERVER_ENTITIES_LOLTEXT_H
#include <game/server/entity.h>

class CLolPlasma : public CEntity
{
	vec2 m_LocalPos; // local coordinate system is origin'd wherever we actually start (i.e. this is (0,0) after creation)
	vec2 m_Vel;
	int m_Life; // remaining ticks
	int m_StartTick; // tick created
	vec2 m_StartOff; // initial offset from parent, for proper following
	CEntity* m_pParent;

public:
	CLolPlasma(CGameWorld* pGameWorld, CEntity* pParent, vec2 Pos, vec2 Vel, int Lifespan);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

class CLoltext
{
public:
	void Create(CGameWorld *pGameWorld, CEntity *pParent, vec2 Pos, vec2 Vel, int Lifespan, const char *pText, bool Center, bool Follow);
};

#endif

