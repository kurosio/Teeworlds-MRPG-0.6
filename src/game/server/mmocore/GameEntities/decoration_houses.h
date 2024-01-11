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
	inline static std::vector<int> m_vFullDecorationItemlist {};

	class CDrawingData
	{
	public:
		CDrawingData() = delete;
		CDrawingData(CPlayer* pPlayer, HouseType Type, vec2 Position, float Radius);
		~CDrawingData();

		int m_ItemPos {};
		CPlayer* m_pPlayer {};
		CLaserOrbite* m_pLaserOrbite {};
		vec2 m_Position {};
		float m_Radius {};
		bool m_Working {};
		HouseType m_Type{};

		int NextItemPos();
		int PrevItemPos();
	};

	CDrawingData* m_pDrawing{};
	int m_UniqueID {};
	int m_HouseID {};
	int m_ItemID {};
	std::vector<int> m_vIDs {};

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
	enum ObjectType
	{
		OBJ_PICKUP,
		OBJ_PROJECTILE,
		OBJ_LASER,
	};
	int GetIDsNum() const;
	ObjectType GetObjectType() const;
	void ReinitilizeSnappingIDs();
};

#endif