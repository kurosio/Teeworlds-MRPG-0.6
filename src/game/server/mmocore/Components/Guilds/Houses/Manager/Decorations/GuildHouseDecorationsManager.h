/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DECORATIONS_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DECORATIONS_MANAGER_H

#include "GuildHouseDecorationData.h"

class CGS;
class CPlayer;
class CPlayerItem;
class CGuildHouseData;
class CDrawingData;
class CEntityHouseDecoration;

using HouseDecorationIdentifier = int;
using HouseDecorationsContainer = std::vector<CEntityHouseDecoration*>;

class CGuildHouseDecorationManager
{
	CGS* GS() const;

	CGuildHouseData* m_pHouse {};
	HouseDecorationsContainer m_apDecorations {};

public:
	CGuildHouseDecorationManager() = delete;
	CGuildHouseDecorationManager(CGuildHouseData* pHouse);
	~CGuildHouseDecorationManager();

	bool StartDrawing(const int& ItemID, CPlayer* pPlayer);
	bool Add(CEntityHouseDecoration* pEntity);
	bool Remove(CEntityHouseDecoration* pEntity);

	const HouseDecorationsContainer& GetContainer() const { return m_apDecorations; };
	bool HasFreeSlots() const { return (int)m_apDecorations.size() < (int)MAX_DECORATIONS_HOUSE; }

	static bool DrawToolCallback(bool EraseMode, CEntityHouseDecoration* pEntity, CPlayer* pPlayer, int DecorationItemID, void* pUser);

private:
	void Init();
};

#endif
