/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_QUESTITEM_H
#define GAME_SERVER_ENTITIES_QUESTITEM_H
#include <game/server/entity.h>

class CDropQuestItem : public CEntity
{
	enum
	{
		NUM_IDS = 3
	};
	int m_IDs[NUM_IDS];

	vec2 m_Vel;
	float m_Angle;
	float m_AngleForce;
	bool m_Flashing;
	int m_LifeSpan;
	int m_FlashTimer;

public:
	CDropQuestItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, QuestBotInfo BotData, int ClientID);
	virtual ~CDropQuestItem();

	int m_ClientID;
	QuestBotInfo m_QuestBot;

	virtual void Tick();
	virtual void Snap(int SnappingClient);

};

#endif
