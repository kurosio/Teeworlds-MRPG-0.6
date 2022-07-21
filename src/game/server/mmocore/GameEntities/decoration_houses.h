#ifndef GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#define GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#include <game/server/entity.h>

class CDecorationHouses : public CEntity
{
	enum
	{
		PERSPECT = 1,
		BODY,
		NUM_IDS,
	};
	int m_IDs[NUM_IDS]{};

	int SwitchToObject(bool Data) const;
public:
	int m_DecoID;
	int m_HouseID;

	CDecorationHouses(CGameWorld* pGameWorld, vec2 Pos, int OwnerID, int DecoID);
	~CDecorationHouses() override;

	void Snap(int SnappingClient) override;
};

#endif