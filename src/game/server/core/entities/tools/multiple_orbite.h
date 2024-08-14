/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_SNAP_FULL_H
#define GAME_SERVER_ENTITIES_SNAP_FULL_H

#include <game/server/entity.h>

class CMultipleOrbite : public CEntity
{
	struct SnapItem
	{
		int m_ID;
		int m_Type;
		int m_Subtype;
	};
	std::list< SnapItem > m_Items{};
	CEntity* m_pParent {};

public:
	CMultipleOrbite(CGameWorld *pGameWorld, CEntity* pParent);
	~CMultipleOrbite() override;

	void Snap(int SnappingClient) override;
	void Tick() override;

	void Add(int Value, int Type, int Subtype);
	void Remove(int Value, int Type, int Subtype);

private:
	vec2 UtilityOrbitePos(int PosID) const;

};

#endif
