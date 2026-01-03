#ifndef GAME_SERVER_CORE_ENTITIES_TOOLS_TEXT_H
#define GAME_SERVER_CORE_ENTITIES_TOOLS_TEXT_H

#include <game/server/entity.h>

class CEntityTextPixel : public CEntity
{
private:
	int m_Lifetime;
	EEntityTextType m_Type;

public:
	CEntityTextPixel(CGameWorld* pGameWorld, vec2 Pos, int Lifespan, EEntityTextType Type);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

class CEntityText
{
public:
	static void Create(CGameWorld* pGameWorld, vec2 Pos, int Lifespan, const char* pText, EEntityTextType Type = EEntityTextType::Projectile);
};

#endif
