/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/mmocore/Components/Bots/BotData.h>
#include "quest_path_finder.h"

#include <game/server/mmocore/Components/Worlds/WorldManager.h>
#include <game/server/gamecontext.h>

CStepPathFinder::CStepPathFinder(CGameWorld* pGameWorld, vec2 Pos, int ClientID, QuestBotInfo QuestBot)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FINDQUEST, Pos)
{
	vec2 GetterPos{0,0};
	GS()->Mmo()->WorldSwap()->FindPosition(QuestBot.m_WorldID, Pos, &GetterPos);

	m_PosTo = GetterPos;
	m_ClientID = ClientID;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	m_SubBotID = QuestBot.m_SubBotID;
	m_MainScenario = str_startswith_nocase(GS()->GetQuestInfo(QuestBot.m_QuestID)->GetStory(), "Ch") != nullptr;
	if(m_MainScenario)
		GS()->CreateLaserOrbite(this, 2, EntLaserOrbiteType::MOVE_LEFT, 10.0f, 6.f, CmaskOne(ClientID));


	GameWorld()->InsertEntity(this);
}

void CStepPathFinder::Tick()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !total_size_vec2(m_PosTo))
	{
		GS()->m_World.DestroyEntity(this);
		return;
	}

	const int QuestID = QuestBotInfo::ms_aQuestBot[m_SubBotID].m_QuestID;
	const int Step = QuestBotInfo::ms_aQuestBot[m_SubBotID].m_Step;
	CQuest* pQuest = m_pPlayer->GetQuest(QuestID);
	if (pQuest->GetCurrentStep() != Step || pQuest->GetState() != QuestState::ACCEPT || pQuest->GetStepByMob(m_SubBotID)->m_StepComplete || pQuest->GetStepByMob(m_SubBotID)->m_ClientQuitting)
	{
		GS()->CreateDeath(m_Pos, m_ClientID);
		GS()->m_World.DestroyEntity(this);
	}
}

void CStepPathFinder::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		vec2 CorePos = m_pPlayer->GetCharacter()->m_Core.m_Pos;
		m_Pos = CorePos;
		m_Pos -= normalize(CorePos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = (m_MainScenario ? (int)POWERUP_HEALTH : (int)POWERUP_ARMOR);
		pPickup->m_Subtype = 0;
	}
}