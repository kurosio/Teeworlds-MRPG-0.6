/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DECORATIONS_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DECORATIONS_MANAGER_H

#include "GuildHouseDecorationData.h"

class CGS;
class CPlayer;
class CGuildHouseData;
class CDecorationHouses;

using HouseDecorationIdentifier = int;
using HouseDecorationsContainer = std::vector<CDecorationHouses*>;

class CGuildHouseDecorationManager
{
	CGS* GS() const;

	CGuildHouseData* m_pHouse {};
	CDecorationHouses* m_apDecorations[MAX_DECORATIONS_HOUSE] {};

public:
	CGuildHouseDecorationManager() = delete;
	CGuildHouseDecorationManager(CGuildHouseData* pHouse);
	~CGuildHouseDecorationManager();
	
	bool Add(int ItemID, vec2 Pos, CPlayer* pPlayerBy);
	bool Remove(HouseDecorationIdentifier DecoID);

	HouseDecorationsContainer&& GetContainer() const;


private:
	void Init();
};

#endif
