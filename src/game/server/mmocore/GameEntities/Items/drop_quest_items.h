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
	int m_LifeSpan;
	CFlashingTick m_Flash;

public:
	CDropQuestItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, int ItemID, int Needed, int QuestID, int Step, int ClientID);
	~CDropQuestItem() override;

	int m_ClientID;
	int m_ItemID;
	int m_Needed;
	int m_QuestID;
	int m_Step;

	virtual void Tick();
	virtual void Snap(int SnappingClient);
};

#endif
