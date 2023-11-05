/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/mmocore/Components/Bots/BotData.h>
#include "quest_mob_path_finder.h"

#include <game/server/mmocore/Components/Worlds/WorldManager.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/GameEntities/Tools/path_navigator.h>

CStepPathFinder::CStepPathFinder(CGameWorld* pGameWorld, vec2 SearchPos, int ClientID, QuestBotInfo QuestBot, std::deque < CStepPathFinder* >* apCollection)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_FINDQUEST, SearchPos)
{
	vec2 PosTo{0,0};
	GS()->Mmo()->WorldSwap()->FindPosition(QuestBot.m_WorldID, SearchPos, &PosTo);

	m_PosTo = PosTo;
	m_ClientID = ClientID;
	m_apCollection = apCollection;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	m_SubBotID = QuestBot.m_SubBotID;
	m_MainScenario = str_startswith_nocase(GS()->GetQuestInfo(QuestBot.m_QuestID)->GetStory(), "Ch") != nullptr;
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	if(m_pPlayer && m_pPlayer->GetItem(itShowQuestNavigator)->IsEquipped())
	{
		new CEntityPathNavigator(&GS()->m_World, this, true, m_pPlayer->m_ViewPos, SearchPos, QuestBot.m_WorldID, false, CmaskOne(ClientID));
	}
}

CStepPathFinder::~CStepPathFinder()
{
	if(m_apCollection && !m_apCollection->empty())
	{
		for(auto it = m_apCollection->begin(); it != m_apCollection->end(); ++it)
		{
			if(mem_comp((*it), this, sizeof(CStepPathFinder)) == 0)
			{
				m_apCollection->erase(it);
				break;
			}
		}
	}
}

void CStepPathFinder::Tick()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !total_size_vec2(m_PosTo))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	const int QuestID = QuestBotInfo::ms_aQuestBot[m_SubBotID].m_QuestID;
	const int Step = QuestBotInfo::ms_aQuestBot[m_SubBotID].m_Step;
	CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
	if (pQuest->GetCurrentStepPos() != Step || pQuest->GetState() != QuestState::ACCEPT || pQuest->GetStepByMob(m_SubBotID)->m_StepComplete || pQuest->GetStepByMob(m_SubBotID)->m_ClientQuitting)
	{
		GS()->CreateDeath(m_Pos, m_ClientID);
		GameWorld()->DestroyEntity(this);
	}
}

void CStepPathFinder::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		m_Pos = m_pPlayer->GetCharacter()->m_Core.m_Pos;
		vec2 Pos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = (m_MainScenario ? (int)POWERUP_HEALTH : (int)POWERUP_ARMOR);
		pPickup->m_Subtype = 0;
	}
}