#ifndef GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#define GAME_SERVER_ENTITIES_DECORATIONS_HOUSES_H
#include <game/server/entity.h>

// Forward declaration and alias
class CPlayer;
class CPlayerItem;
class CLaserOrbite;

class CEntityHouseDecoration : public CEntity
{
	inline static std::vector<int> m_vFullDecorationItemlist {};
	typedef bool (*DrawToolCallback)(bool, CEntityHouseDecoration*, CPlayer*, int, void*);
	typedef struct { void* m_pData; DrawToolCallback m_Callback; } EventDrawTool;

	class CDrawingData
	{
		int m_ItemPos {};

	public:
		CDrawingData() = delete;
		CDrawingData(CPlayer* pPlayer, vec2 Position, float Radius);
		~CDrawingData();

		CPlayer* m_pPlayer {};
		vec2 m_Position {};
		float m_Radius {};
		bool m_Working {};
		bool m_EraseMode {};
		CLaserOrbite* m_pZoneOrbite {};
		CLaserOrbite* m_pEraseOrbite {};
		EventDrawTool m_ToolEvent {};

		int NextItemPos();
		int PrevItemPos();
	};

	CDrawingData* m_pDrawing{};
	int m_UniqueID {};
	int m_GroupID {};
	int m_ItemID {};
	std::vector<int> m_vIDs {};

public:
	CEntityHouseDecoration(CGameWorld* pGameWorld, vec2 Pos, int UniqueID, int GroupID, int ItemID);
	~CEntityHouseDecoration() override;

	void SetUniqueID(int UniqueID) { m_UniqueID = UniqueID; }
	void StartDrawingMode(DrawToolCallback Callback, void* pCallbackData, CPlayer* pPlayer, const vec2& CenterPos, float Radius);

	int GetUniqueID() const { return m_UniqueID; }
	int GetGroupID() const { return m_GroupID; }
	int GetItemID() const { return m_ItemID; }

	void Tick() override;
	void Snap(int SnappingClient) override;
	CEntityHouseDecoration* FindByGroupID(int GroupID);

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