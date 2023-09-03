/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_ORBITE_H
#define GAME_SERVER_ENTITIES_LASER_ORBITE_H
#include <game/server/entity.h>

class CLaserOrbite : public CEntity
{
public:
	CLaserOrbite(CGameWorld* pGameWorld, int ClientID, CEntity* pEntParent, int Amount, EntLaserOrbiteType Type, float Speed, float Radius, int LaserType, int64 Mask);

	void Destroy() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	array<int> m_IDs;
	EntLaserOrbiteType m_MoveType {};
	int m_ClientID {};
	int m_LaserType {};
	float m_MoveSpeed{};
	float m_Radius{};
	int64 m_Mask{};
	CEntity* m_pEntParent{};

	vec2 UtilityOrbitePos(int PosID) const;
};

#endif
