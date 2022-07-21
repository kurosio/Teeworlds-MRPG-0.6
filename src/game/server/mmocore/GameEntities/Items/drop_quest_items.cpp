/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/mmocore/Components/Bots/BotData.h>
#include "drop_quest_items.h"

#include <game/server/gamecontext.h>

CDropQuestItem::CDropQuestItem(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, QuestBotInfo BotData, int ClientID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPQUEST, Pos, 24.0f)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Angle = 0.0f;
	m_AngleForce = AngleForce;

	m_ClientID = ClientID;
	m_QuestBot = BotData;
	m_Flashing = false;
	m_FlashTimer = 0;
	m_LifeSpan = Server()->TickSpeed() * 60;
	GameWorld()->InsertEntity(this);
	for(int i=0; i<NUM_IDS; i++)
	{
		m_IDs[i] = Server()->SnapNewID();
	}
}

CDropQuestItem::~CDropQuestItem()
{
	for(int i=0; i<NUM_IDS; i++)
	{
		Server()->SnapFreeID(m_IDs[i]);
	}
 }

void CDropQuestItem::Tick()
{
	// life time dk
	m_LifeSpan--;
	if (m_LifeSpan < 0 || !GS()->m_apPlayers[m_ClientID] || GS()->m_apPlayers[m_ClientID]->GetQuest(m_QuestBot.m_QuestID).GetState() != QuestState::QUEST_ACCEPT)
	{
		GS()->m_World.DestroyEntity(this);
		return;
	}

	// flashing
	if (m_LifeSpan < 150)
	{
		m_FlashTimer--;
		if (m_FlashTimer > 5)
			m_Flashing = true;
		else
		{
			m_Flashing = false;
			if (m_FlashTimer <= 0)
				m_FlashTimer = 10;
		}
	}

	// physic
	vec2 ItemSize = vec2(GetProximityRadius(), GetProximityRadius());
	GS()->Collision()->MovePhysicalAngleBox(&m_Pos, &m_Vel, ItemSize, &m_Angle, &m_AngleForce, 0.5f);

	// check step and collected it or no
	const int Value = m_QuestBot.m_aItemSearchValue[0];
	CPlayer* pOwnerPlayer = GS()->m_apPlayers[m_ClientID];
	CQuestData& pPlayerQuest = pOwnerPlayer->GetQuest(m_QuestBot.m_QuestID);
	CItemData& pPlayerItem = pOwnerPlayer->GetItem(m_QuestBot.m_aItemSearch[0]);
	if (pPlayerQuest.m_Step != m_QuestBot.m_Step || pPlayerItem.m_Value >= Value)
	{
		GS()->m_World.DestroyEntity(this);
		return;
	}

	// interactive
	if (pOwnerPlayer->GetCharacter() && distance(m_Pos, pOwnerPlayer->GetCharacter()->m_Core.m_Pos) < 32.0f)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_INFORMATION, 10, "Press 'Fire' for pick Quest Item");
		if (pOwnerPlayer->GetCharacter()->m_ReloadTimer)
		{
			pPlayerItem.Add(1);
			pOwnerPlayer->GetCharacter()->m_ReloadTimer = 0;
			GS()->CreatePlayerSound(m_ClientID, SOUND_ITEM_PICKUP);
			GS()->Chat(m_ClientID, "You pick {STR} for {STR}!", pPlayerItem.Info().GetName(), m_QuestBot.GetName());
			GS()->m_World.DestroyEntity(this);
			return;
		}
	}
}


void CDropQuestItem::Snap(int SnappingClient)
{
	if(m_Flashing || m_ClientID != SnappingClient || NetworkClipped(SnappingClient))
		return;

	// mrpg
	if (GS()->IsMmoClient(SnappingClient))
	{
		CNetObj_MmoPickup* pObj = static_cast<CNetObj_MmoPickup*>(Server()->SnapNewItem(NETOBJTYPE_MMOPICKUP, GetID(), sizeof(CNetObj_MmoPickup)));
		if (!pObj)
			return;

		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_Type = MMO_PICKUP_DROP;
		pObj->m_Angle = (int)(m_Angle * 256.0f);
		return;
	}

	// vanilla box
	CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if (pProj)
	{
		pProj->m_X = (int)m_Pos.x;
		pProj->m_Y = (int)m_Pos.y;
		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = Server()->Tick();
		pProj->m_Type = WEAPON_HAMMER;
	}

	static const float Radius = 16.0f;
	const float AngleStep = 2.0f * pi / CDropQuestItem::NUM_IDS;
	const float AngleStart = (pi / CDropQuestItem::NUM_IDS) + (2.0f * pi * m_Angle);
	for(int i = 0; i < CDropQuestItem::NUM_IDS; i++)
	{
		CNetObj_Laser *pRifleObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs[i], sizeof(CNetObj_Laser)));
		if(!pRifleObj)
			return;

		vec2 Pos = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * i), Radius * sin(AngleStart + AngleStep * i));
		vec2 PosTo = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * (i+1)), Radius * sin(AngleStart + AngleStep * (i+1)));
		pRifleObj->m_X = (int)Pos.x;
		pRifleObj->m_Y = (int)Pos.y;
		pRifleObj->m_FromX = (int)PosTo.x;
		pRifleObj->m_FromY = (int)PosTo.y;
		pRifleObj->m_StartTick = Server()->Tick() - 4;
	}
}