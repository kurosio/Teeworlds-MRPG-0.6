/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DECORATIONS_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DECORATIONS_MANAGER_H

class CGS;
class CPlayer;
class CPlayerItem;
class CGuildHouseData;
class CDrawingData;
class CEntityHouseDecoration;
class CEntityDrawboard;
class EntityPoint;

using HouseDecorationIdentifier = int;
using HouseDecorationsContainer = std::vector<CEntityHouseDecoration*>;

class CGuildHouseDecorationManager
{
	CGS* GS() const;

	CEntityDrawboard* m_pDrawBoard {};
	CGuildHouseData* m_pHouse {};

public:
	CGuildHouseDecorationManager() = delete;
	CGuildHouseDecorationManager(CGuildHouseData* pHouse);
	~CGuildHouseDecorationManager();

	bool StartDrawing(CPlayer* pPlayer) const;
	bool HasFreeSlots() const;

private:
	void Init();

	static bool DrawboardToolEventCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser);

	bool Add(const EntityPoint* pPoint) const;
	bool Remove(const EntityPoint* pPoint) const;
};

#endif
