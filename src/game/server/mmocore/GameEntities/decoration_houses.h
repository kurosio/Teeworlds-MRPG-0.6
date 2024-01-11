#ifndef GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#define GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#include <game/server/entity.h>

// Forward declaration and alias
class CPlayer;
class CLaserOrbite;

// Enumeration for the type of a house
enum class HouseType : int
{
	DEFAULT,               // Default type of house
	GUILD                  // Guild type of house
};

class CEntityHouseDecoration : public CEntity
{
	class CDrawingData
	{
	public:
		CDrawingData() = delete;
		CDrawingData(CPlayer* pPlayer, HouseType Type, vec2 Position, float Radius);
		~CDrawingData();

		CPlayer* m_pPlayer {};
		CLaserOrbite* m_pLaserOrbite {};
		vec2 m_Position {};
		float m_Radius {};
		bool m_Working {};
		bool m_Cancel {};
		HouseType m_Type{};
	};

	CDrawingData* m_pDrawing;
	int m_UniqueID {};
	int m_HouseID {};
	int m_ItemID {};

	enum
	{
		PERSPECT = 1,
		BODY,
		NUM_IDS,
	};
	int m_IDs[NUM_IDS] {};

public:
	CEntityHouseDecoration(CGameWorld* pGameWorld, vec2 Pos, int UniqueID, int ItemID);
	~CEntityHouseDecoration() override;

	void SetUniqueID(int UniqueID) { m_UniqueID = UniqueID; }
	void StartDrawingMode(CPlayer* pPlayer, HouseType Type, const vec2& HousePos, float Radius);

	int GetUniqueID() const { return m_UniqueID; }
	int GetHouseID() const { return m_HouseID; }
	int GetItemID() const { return m_ItemID; }

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	int GetObjectType(bool Projectile) const;
};

#endif