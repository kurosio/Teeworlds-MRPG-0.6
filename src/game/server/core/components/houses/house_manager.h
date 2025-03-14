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
		// free data
		mystd::freeContainer(CHouse::Data());
	}

	void OnInitWorld(const std::string& SqlQueryWhereWorld) override;
	void OnTick() override;
	void OnTimePeriod(ETimePeriod Period) override;
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnPlayerMotdCommand(CPlayer* pPlayer, CMotdPlayerData* pMotdData, const char* pCmd) override;

	void ShowDetail(CPlayer* pPlayer, CHouse* pHouse);
	void ShowMenu(CPlayer* pPlayer) const;
	void ShowSell(CPlayer* pPlayer) const;
	void ShowDoorsController(CPlayer* pPlayer) const;
	void ShowFarmzonesControl(CPlayer* pPlayer) const;
	void ShowFarmzoneEdit(CPlayer* pPlayer, int FarmzoneID) const;

public:
	CHouse* GetHouse(HouseIdentifier ID) const;
	CHouse* GetHouseByPos(vec2 Pos) const;
	CFarmzone* GetHouseFarmzoneByPos(vec2 Pos) const;
};

#endif