/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LASER_ORBITE_H
#define GAME_SERVER_ENTITIES_LASER_ORBITE_H

#include <game/server/entity.h>

class CEntityLaserOrbite : public CEntity
{
public:
	CEntityLaserOrbite(CGameWorld* pGameWorld, int ClientID, CEntity* pEntParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask);
	~CEntityLaserOrbite() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

	void AddClientMask(int ClientID);
	void RemoveClientMask(int ClientID);

	LaserOrbiteType GetType() const { return m_Type; }
	CEntity* GetEntityParent() const { return m_pEntParent; }

private:
	array<int> m_IDs;
	LaserOrbiteType m_Type {};
	int m_LaserType {};
	float m_MoveSpeed{};
	int64_t m_Mask{};
	vec2 m_AppendPos {};
	CEntity* m_pEntParent{};

	vec2 UtilityOrbitePos(int PosID) const;
};

#endif
