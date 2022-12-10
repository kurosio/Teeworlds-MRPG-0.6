#ifndef GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#define GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#include <game/server/entity.h>

class CDecorationHouses : public CEntity
{
	int m_DecoID {};
	int m_HouseID {};
	int m_ItemID {};

	enum
	{
		PERSPECT = 1,
		BODY,
		NUM_IDS,
	};
	int m_IDs[NUM_IDS]{};
	int SwitchToObject(bool Data) const;

public:
	CDecorationHouses(CGameWorld* pGameWorld, vec2 Pos, int HouseID, int DecoID, int ItemID);
	~CDecorationHouses() override;

	int GetDecorationID() const { return m_DecoID; }
	int GetHouseID() const { return m_HouseID; }
	int GetItemID() const { return m_ItemID; }

	void Snap(int SnappingClient) override;
};

#endif