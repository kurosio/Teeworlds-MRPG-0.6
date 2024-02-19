/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/core/components/Bots/BotData.h>
#include "quest_bot_navigator.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/tools/path_navigator.h>

CQuestBotNavigator::CQuestBotNavigator(CGameWorld* pGameWorld, int ClientID, const QuestBotInfo& Bot)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_FINDQUEST, Bot.m_Position, 0, ClientID), m_QuestBotID(Bot.m_ID)
{
	vec2 PosTo { 0,0 };
	GS()->Core()->WorldManager()->FindPosition(Bot.m_WorldID, m_Pos, &PosTo);

	m_PosTo = PosTo;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	if(m_pPlayer && m_pPlayer->GetItem(itShowQuestNavigator)->IsEquipped())
		new CEntityPathNavigator(&GS()->m_World, this, true, m_pPlayer->m_ViewPos, m_Pos, Bot.m_WorldID, false, CmaskOne(ClientID));
}

void CQuestBotNavigator::Tick()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || is_negative_vec(m_PosTo))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	const auto& Bot = QuestBotInfo::ms_aQuestBot[m_QuestBotID];
	const int& StepPos = Bot.m_StepPos;
	auto* pQuest = m_pPlayer->GetQuest(Bot.m_QuestID);
	auto* pStep = pQuest->GetStepByMob(m_QuestBotID);
	if(pQuest->GetStepPos() != StepPos || pQuest->GetState() != QuestState::ACCEPT || pStep->m_StepComplete || pStep->m_ClientQuitting)
	{
		GS()->CreateDeath(m_Pos, m_ClientID);
		GameWorld()->DestroyEntity(this);
	}
}

void CQuestBotNavigator::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		m_Pos = m_pPlayer->GetCharacter()->m_Core.m_Pos;
		vec2 Pos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = (int)POWERUP_HEALTH;
		pPickup->m_Subtype = 0;
	}
}