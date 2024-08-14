#ifndef GAME_SERVER_ENTITIES_DRAW_BOARD_H
#define GAME_SERVER_ENTITIES_DRAW_BOARD_H

#include <game/server/entity.h>

enum CBrushFlags
{
	BRUSHFLAG_NONE = 0,
	BRUSHFLAG_CELL_POSITION = 1 << 0,
};

enum DrawboardFlags
{
	DRAWBOARDFLAG_NONE = 0,
	DRAWBOARDFLAG_PLAYER_ITEMS = 1 << 0,
};

class EntityPoint
{
public:
	EntityPoint(CEntity* pEntity, int ItemID) : m_pEntity(pEntity), m_ItemID(ItemID) {}
	CEntity* m_pEntity {};
	int m_ItemID {};
};

// Forward declaration and alias
class CPlayer;
class CEntityDrawboard;
class CEntityLaserOrbite;
typedef bool (*DrawboardToolCallback)(DrawboardToolEvent, CPlayer*, const EntityPoint*, void*);
typedef struct { DrawboardToolCallback m_Callback; void* m_pData; } DrawboardEvent;

class CBrush
{
	friend class CEntityDrawboard;
	IServer* Server() const;
	CGS* GS() const;

	int64_t m_Flags {};
	vec2 m_Position {};
	CEntity* m_pEntity {};
	CPlayer* m_pPlayer {};
	CEntityDrawboard* m_pBoard {};
	DrawboardEvent* m_pToolEvent {};

	std::vector<int>::iterator m_BrushItem {};
	std::vector<int> m_vBrushItemsCollection {};

public:
	CBrush() = delete;
	CBrush(CPlayer* pPlayer, CEntityDrawboard* pBoard, DrawboardEvent* pToolEvent);
	~CBrush();

private:
	void InitBrushCollection();
	bool OnUpdate();

	void Draw();
	void Erase();
	void SwitchMode();
	void NextItem();
	void PrevItem();

	bool HandleInput();
	void SendBroadcast() const;
	void UpdateEntity();
	bool UpdatePosition();

	bool ProccessEvent(DrawboardToolEvent Event, EntityPoint* pPoint) const;
};

class CEntityDrawboard : public CEntity
{
	friend class CBrush;
	DrawboardEvent m_ToolEvent {};
	std::vector<EntityPoint*> m_vEntities {};
	ska::unordered_set<CBrush*> m_vBrushes {};
	CEntityLaserOrbite* m_pOrbite {};
	float m_Radius {};
	int64_t m_Flags {};

public:
	CEntityDrawboard(CGameWorld* pGameWorld, vec2 Pos, float Radius);
	~CEntityDrawboard() override;

	bool StartDrawing(CPlayer* pPlayer);
	void EndDrawing(CPlayer* pPlayer);
	void RegisterEvent(DrawboardToolCallback Callback, void* pUser);
	void SetFlags(int64_t Flags) { m_Flags = Flags; }
	void AddPoint(vec2 Pos, int ItemID);

	const ska::unordered_set<CBrush*>& GetBrushes() const { return m_vBrushes; }
	const std::vector<EntityPoint*>& GetEntityPoints() const { return m_vEntities; }

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	ska::unordered_set<CBrush*>::iterator SearchBrush(CPlayer* pPlayer);
	std::vector<EntityPoint*>::iterator SearchPoint(vec2 Pos);
	bool Draw(CBrush* pBrush, EntityPoint* pPoint);
	bool Erase(CBrush* pBrush, vec2 Pos);
};

#endif