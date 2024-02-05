/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AETHER_CORE_H
#define GAME_SERVER_COMPONENT_AETHER_CORE_H
#include <game/server/core/mmo_component.h>

#include "AetherData.h"

class CAetherManager : public MmoComponent
{
	inline static ska::unordered_map<int, std::deque<CAetherData*>> ms_vpAetherGroupCollector {};

	~CAetherManager() override
	{
		for(auto& pAether : CAetherData::Data())
			delete pAether;
		CAetherData::Data().clear();
		CAetherData::Data().shrink_to_fit();
		ms_vpAetherGroupCollector.clear();
	};

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	void ShowMenu(CCharacter* pChar) const;
	void UnlockLocationByPos(CPlayer* pPlayer, vec2 Pos) const;
	CAetherData* GetAetherByID(int AetherID) const;
	CAetherData* GetAetherByPos(vec2 Pos) const;
};

#endif