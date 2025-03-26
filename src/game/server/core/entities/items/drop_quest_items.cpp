/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "drop_quest_items.h"

#include <game/server/gamecontext.h>

CDropQuestItem::CDropQuestItem(CGameWorld* pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, int ItemID, int Needed, int QuestID, int Step, int ClientID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_QUEST_DROP, Pos, 24.0f)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Angle = 0.0f;
	m_AngleForce = AngleForce;
	m_ClientID = ClientID;
	m_ItemID = ItemID;
	m_Needed = Needed;
	m_QuestID = QuestID;
	m_Step = Step;
	m_LifeSpan = Server()->TickSpeed() * 60;

	GameWorld()->InsertEntity(this);
	for (int& m_ID : m_IDs)
	{
		m_ID = Server()->SnapNewID();
	}
}

CDropQuestItem::~CDropQuestItem()
{
	for(const int m_ID : m_IDs)
	{
		Server()->SnapFreeID(m_ID);
	}
}

void CDropQuestItem::Tick()
{
	auto* pPlayer = GS()->GetPlayer(m_ClientID);

	m_LifeSpan--;
	if (m_LifeSpan < 0 || !pPlayer)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(!HasPlayersInView())
		return;

	m_Flash.Tick(m_LifeSpan);
	GS()->Collision()->MovePhysicalAngleBox(&m_Pos, &m_Vel, vec2(m_Radius, m_Radius), &m_Angle, &m_AngleForce, 0.5f);

	// check quest valid and item value
	auto* pPlayerItem = pPlayer->GetItem(m_ItemID);
	const auto* pQuest = pPlayer->GetQuest(m_QuestID);
	if(pQuest->GetState() != QuestState::Accepted || pQuest->GetStepPos() != m_Step || pPlayerItem->GetValue() >= m_Needed)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// pickup
	if(pPlayer->GetCharacter() && distance(m_Pos, pPlayer->GetCharacter()->m_Core.m_Pos) < 32.0f)
	{
		if(Server()->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE_HAMMER))
		{
			pPlayerItem->Add(1);
			GS()->Chat(m_ClientID, "You got '{}'.", pPlayerItem->Info()->GetName());
			GS()->CreatePlayerSound(m_ClientID, SOUND_PICK_UP);
			GameWorld()->DestroyEntity(this);
			return;
		}

		GS()->Broadcast(m_ClientID, BroadcastPriority::GameInformation, 10, "Press hammer 'Fire', to pick up an item");
	}
}


void CDropQuestItem::Snap(int SnappingClient)
{
	if(m_Flash.IsFlashing() || m_ClientID != SnappingClient || NetworkClipped(SnappingClient))
		return;

	// body
	GS()->SnapProjectile(SnappingClient, GetID(), m_Pos, {}, Server()->Tick(), WEAPON_HAMMER, m_ClientID);

	// parts
	static const float Radius = 16.0f;
	const float AngleStep = 2.0f * pi / (float)CDropQuestItem::NUM_IDS;
	const float AngleStart = (pi / (float)CDropQuestItem::NUM_IDS) + (2.0f * pi * m_Angle);
	for(int i = 0; i < CDropQuestItem::NUM_IDS; i++)
	{
		const auto FinalPos = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * i), Radius * sin(AngleStart + AngleStep * i));
		const auto FinalPosTo = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * (i+1)), Radius * sin(AngleStart + AngleStep * (i+1)));
		GS()->SnapLaser(SnappingClient, m_IDs[i], FinalPosTo, FinalPos, Server()->Tick() - 4);
	}
}
