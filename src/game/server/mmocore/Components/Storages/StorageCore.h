/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_STORAGE_CORE_H
#define GAME_SERVER_COMPONENT_STORAGE_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "StorageData.h"

class CStorageCore : public MmoComponent
{
	~CStorageCore() override
	{
		CStorageData::ms_aStorage.clear();
	};

	void OnInit() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

public:
	void ShowStorageMenu(CPlayer *pPlayer, int StorageID);
	int GetStorageID(vec2 Pos) const;
};

#endif