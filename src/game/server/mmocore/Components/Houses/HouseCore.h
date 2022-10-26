/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_CORE_H
#define GAME_SERVER_COMPONENT_HOUSE_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "HouseData.h"

class HouseDoor;
class CDecorationHouses;

class CHouseCore : public MmoComponent
{
	~CHouseCore() override
	{
		CHouseData::ms_aHouse.clear();
	};

	/* #########################################################################
		VAR AND OBJECTS HOUSES
	######################################################################### */
	std::map < int , CDecorationHouses * > m_aDecorationHouse;

	void OnInitWorld(const char* pWhereLocalWorld) override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	/* #########################################################################
		FUNCTIONS HOUSES PLANTS
	######################################################################### */
	void ChangePlantsID(int HouseID, int PlantID);

public:
	/* #########################################################################
		FUNCTIONS HOUSES DECORATION
	######################################################################### */
	bool AddDecorationHouse(int DecoID, int OwnerID, vec2 Position);

private:
	bool DeleteDecorationHouse(int ID);
	void ShowDecorationList(CPlayer *pPlayer);

	/* #########################################################################
		MENUS HOUSES
	######################################################################### */
	void ShowHouseMenu(CPlayer* pPlayer, int HouseID);
	void ShowPersonalHouse(CPlayer* pPlayer);

	/* #########################################################################
		GET CHECK HOUSES
	######################################################################### */
public:
	bool IsHouseHasOwner(int HouseID) const;
	int GetHouse(vec2 Pos, bool Plants = false) const;
	int GetHousePrice(int HouseID) const;
	bool GetHouseDoor(int HouseID) const;
	vec2 GetPositionHouse(int HouseID) const;
	int PlayerHouseID(CPlayer *pPlayer) const;
	int OwnerHouseID(int UserID) const;
	int GetPlantsID(int HouseID) const;

	const char *ClassName(int HouseID) const;
	const char *OwnerName(int HouseID);

	/* #########################################################################
		FUNCTIONS HOUSES
	######################################################################### */
	void BuyHouse(int HouseID, CPlayer *pPlayer);
	void SellHouse(int HouseID);

	void TakeFromSafeDeposit(CPlayer *pPlayer, int TakeValue);
	void AddSafeDeposit(CPlayer *pPlayer, int Balance);
	bool ChangeStateDoor(int HouseID);
};

#endif