/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_CORE_H
#define GAME_SERVER_COMPONENT_HOUSE_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "HouseData.h"

class CEntityHouseDoor;
class CEntityHouseDecoration;

class CHouseManager : public MmoComponent
{
	~CHouseManager() override
	{
		CHouseData::Data().clear();
	};

	/* #########################################################################
		VAR AND OBJECTS HOUSES
	######################################################################### */

	void OnInitWorld(const char* pWhereLocalWorld) override;
	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	/* #########################################################################
		MENUS HOUSES
	######################################################################### */
	void ShowHouseMenu(CPlayer* pPlayer, CHouseData* pHouse);

	/* #########################################################################
		GET CHECK HOUSES
	######################################################################### */
public:
	static CHouseData* GetHouseByAccountID(int AccountID);
	CHouseData* GetHouse(HouseIdentifier ID);
	CHouseData* GetHouseByPos(vec2 Pos);
	CHouseData* GetHouseByPlantPos(vec2 Pos);
};

#endif