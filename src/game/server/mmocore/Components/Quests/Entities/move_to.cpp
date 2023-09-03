/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "game/server/mmocore/Components/Bots/BotData.h"
#include "move_to.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Quests/QuestManager.h"

constexpr float s_Radius = 36.0f;

CEntityMoveTo::CEntityMoveTo(CGameWorld* pGameWorld, const QuestBotInfo::TaskRequiredMoveTo* pTaskMoveTo, int ClientID, int QuestID, bool* pComplete, std::deque < CEntityMoveTo* >* apCollection)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVE_TO, pTaskMoveTo->m_Position), m_QuestID(QuestID), m_ClientID(ClientID), m_pTaskMoveTo(pTaskMoveTo)
{
	m_pComplete = pComplete;
	m_apCollection = apCollection;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	GameWorld()->InsertEntity(this);

	GS()->CreateLaserOrbite(this, 4, EntLaserOrbiteType::DEFAULT, 0.0f, s_Radius, LASERTYPE_FREEZE, CmaskOne(ClientID));
}

CEntityMoveTo::~CEntityMoveTo()
{
	if(m_pPlayer && m_pPlayer->GetCharacter())
	{
		GS()->Mmo()->Quest()->UpdateSteps(m_pPlayer);
	}

	for(auto it = m_apCollection->begin(); it != m_apCollection->end(); ++it)
	{
		if(mem_comp((*it), this, sizeof(CEntityMoveTo)) == 0)
		{
			m_apCollection->erase(it);
			break;
		}
	}
}

bool CEntityMoveTo::PickItem() const
{
	if(m_pPlayer->GetCharacter()->m_ReloadTimer)
	{
		CPlayerItem* pItem = m_pPlayer->GetItem(m_pTaskMoveTo->m_CollectItemID);
		pItem->Add(1);
		GS()->Chat(m_ClientID, "You got {STR}.", pItem->Info()->GetName());

		m_pPlayer->GetCharacter()->m_ReloadTimer = 0;
		return true;
	}

	GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_INFORMATION, 10, "Press 'Fire', to pick up an item");
	return false;
}

void CEntityMoveTo::Tick()
{
	if(!m_pTaskMoveTo || !m_pPlayer || !m_pPlayer->GetCharacter() || !m_pComplete || (*m_pComplete == true))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(distance(m_pPlayer->m_ViewPos, m_Pos) < s_Radius)
	{
		auto FuncSuccess = [this]()
		{
			if(!m_pTaskMoveTo->m_aTextChat.empty())
				GS()->Chat(m_ClientID, m_pTaskMoveTo->m_aTextChat.c_str());

			(*m_pComplete) = true;
			m_pPlayer->GetQuest(m_QuestID)->SaveSteps();
			GS()->CreateDeath(m_Pos, m_ClientID);
			GameWorld()->DestroyEntity(this);
		};

		const bool HasCollectItem = m_pTaskMoveTo->m_CollectItemID > 0;
		const bool TextUseInChat = !m_pTaskMoveTo->m_aTextUseInChat.empty();


		// text use in chat type
		if(TextUseInChat)
		{
			if(m_pTaskMoveTo->m_aTextUseInChat == m_pPlayer->m_aLastMsg)
			{
				if(HasCollectItem)
				{
					CPlayerItem* pItem = m_pPlayer->GetItem(m_pTaskMoveTo->m_CollectItemID);
					pItem->Add(1);
					GS()->Chat(m_ClientID, "You got {STR}.", pItem->Info()->GetName());
				}

				FuncSuccess();
			}

			GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 10, "Open the chat and enter the text\n(without the brackets)\n'{STR}'", m_pTaskMoveTo->m_aTextUseInChat.c_str());
			return;
		}


		// default pickup item or just move
		if(HasCollectItem)
		{
			if(PickItem())
				FuncSuccess();
			return;
		}

		// only move
		FuncSuccess();
	}
}

void CEntityMoveTo::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	CNetObj_Pickup *pPickup = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		pPickup->m_X = (int)m_Pos.x;
		pPickup->m_Y = (int)m_Pos.y;
		pPickup->m_Type = POWERUP_ARMOR;
		pPickup->m_Subtype = 0;
	}
}