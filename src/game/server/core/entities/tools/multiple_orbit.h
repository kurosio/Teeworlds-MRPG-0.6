/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_SNAP_FULL_H
#define GAME_SERVER_ENTITIES_SNAP_FULL_H

#include <game/server/entity.h>

class CMultipleOrbit : public CEntity
{
	struct SnapItem
	{
		int m_ID;
		int m_Type;
		int m_Subtype;
		int m_OrbitType;
		bool m_Projectile;
	};
	std::vector<SnapItem> m_Items {};
	CEntity* m_pParent {};

public:
	CMultipleOrbit(CGameWorld* pGameWorld, CEntity* pParents);
	~CMultipleOrbit() override;

	void Snap(int SnappingClient) override;
	void Tick() override;

	void Add(bool Projectile, int Value, int Type, int Subtype, int OrbitType);
	void Remove(bool Projectile, int Value, int Type, int Subtype, int OrbitType);

private:
	vec2 UtilityOrbitPos(int OrbitType, int Iter) const;
	vec2 RoseCurvePos(float Angle, float Time) const;
	vec2 HypotrochoidPos(float Angle, float Time) const;
};


#endif
