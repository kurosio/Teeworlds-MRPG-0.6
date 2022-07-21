/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H
#include <game/server/entity.h>

class CProjectile : public CEntity
{
	vec2 m_Direction;
	int m_LifeSpan;
	int m_Owner;
	int m_Type;
	int m_Damage;
	int m_SoundImpact;
	int m_Weapon;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;

	int m_OwnerMmoProjType;

public:
	CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, int Weapon);

	vec2 GetPos(float Time);

	void Reset() override;
	void Tick() override;
	virtual void TickPaused();
	void Snap(int SnappingClient) override;

private:
	int GetOwnerProjID(int ClientID) const;
};

#endif
