/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H
#define GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include <game/server/mmocore/Components/Bots/BotData.h>
#include "WorldSwapData.h"

class CWorldSwapCore : public MmoComponent
{
	~CWorldSwapCore() override
	{
		CWorldSwapData::ms_aWorldSwap.clear();
		CWorldSwapPosition::ms_aWorldPositionLogic.clear();
	};

	void OnInit() override;
	void OnInitWorld(const char* pWhereLocalWorld) override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;

public:
	int GetWorldType() const;
	int GetNecessaryQuest(int WorldID = -1) const;
	vec2 GetPositionQuestBot(int ClientID, const QuestBotInfo& QuestBot) const;
	void CheckQuestingOpened(CPlayer* pPlayer, int QuestID) const;

private:
	bool ChangeWorld(CPlayer* pPlayer, vec2 Pos);
	int GetID(vec2 Pos) const;
};

#endif