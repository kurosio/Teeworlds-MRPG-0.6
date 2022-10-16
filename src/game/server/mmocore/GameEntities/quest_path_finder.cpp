/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/mmocore/Components/Bots/BotData.h>
#include "quest_path_finder.h"

#include <game/server/mmocore/Components/Worlds/WorldSwapCore.h>
#include <game/server/gamecontext.h>

CQuestPathFinder::CQuestPathFinder(CGameWorld* pGameWorld, vec2 Pos, int ClientID, QuestBotInfo QuestBot)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FINDQUEST, Pos)
{
	m_PosTo = GS()->Mmo()->WorldSwap()->GetPositionQuestBot(ClientID, QuestBot);

	m_ClientID = ClientID;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);

	m_SubBotID = QuestBot.m_SubBotID;
	m_MainScenario = str_startswith(GS()->GetQuestInfo(QuestBot.m_QuestID).GetStory(), "Main") != nullptr;

	GameWorld()->InsertEntity(this);
}

void CQuestPathFinder::Tick()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !total_size_vec2(m_PosTo))
	{
		GS()->m_World.DestroyEntity(this);
		return;
	}

	const int QuestID = QuestBotInfo::ms_aQuestBot[m_SubBotID].m_QuestID;
	const int Step = QuestBotInfo::ms_aQuestBot[m_SubBotID].m_Step;
	if (m_pPlayer->GetQuest(QuestID).m_Step != Step || m_pPlayer->GetQuest(QuestID).GetState() != QuestState::QUEST_ACCEPT || m_pPlayer->GetQuest(QuestID).m_StepsQuestBot[m_SubBotID].m_StepComplete)
	{
		GS()->CreateDeath(m_Pos, m_ClientID);
		GS()->m_World.DestroyEntity(this);
		return;
	}
}

void CQuestPathFinder::Snap(int SnappingClient)
{
	if(SnappingClient != m_ClientID || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		m_Pos = m_pPlayer->GetCharacter()->GetPos();
		m_Pos -= normalize(m_pPlayer->GetCharacter()->GetPos() - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);

		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = (m_MainScenario ? (int)POWERUP_HEALTH : (int)POWERUP_ARMOR);
		pPickup->m_Subtype = 0;
	}
}