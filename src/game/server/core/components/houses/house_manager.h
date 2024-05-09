/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_CORE_H
#define GAME_SERVER_COMPONENT_HOUSE_CORE_H
#include <game/server/core/mmo_component.h>

#include "house_data.h"

class CEntityHouseDoor;
class CEntityHouseDecoration;

class CHouseManager : public MmoComponent
{
	~CHouseManager() override
	{
		CHouseData::Data().clear();
	}

	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	void ShowBuyHouse(CPlayer* pPlayer, CHouseData* pHouse);

public:
	CHouseData* GetHouse(HouseIdentifier ID) const;
	CHouseData* GetHouseByPos(vec2 Pos) const;
	CHouseData::CPlantzone* GetHousePlantzoneByPos(vec2 Pos) const;
};

#endif