/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "RandomBox.h"

#include <game/server/gamecontext.h>

bool CRandomBox::Start(CPlayer *pPlayer, int Seconds, CItemData* pPlayerUsesItem, int UseValue)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayerUsesItem)
		return false;

	if(pPlayer->m_aPlayerTick[LastRandomBox] > pPlayer->GS()->Server()->Tick())
	{
		pPlayer->GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "Wait until the last random box opens!");
		return false;
	}

	UseValue = min(100, UseValue);
	if(pPlayerUsesItem->Remove(UseValue))
	{
		Seconds *= pPlayer->GS()->Server()->TickSpeed();
		pPlayer->m_aPlayerTick[LastRandomBox] = pPlayer->GS()->Server()->Tick() + Seconds;
		std::sort(m_ArrayItems.begin(), m_ArrayItems.end(), [](const StructRandomItem& pLeft, const StructRandomItem& pRight) { return pLeft.m_Chance < pRight.m_Chance; });
		new CRandomBoxRandomizer(&pPlayer->GS()->m_World, pPlayer, pPlayer->Acc().m_UserID, Seconds, m_ArrayItems, pPlayerUsesItem, UseValue);
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You used '{STR}x{INT}'.", pPlayerUsesItem->Info().GetName(), UseValue);
	}
	return true;
};

CRandomBoxRandomizer::CRandomBoxRandomizer(CGameWorld* pGameWorld, CPlayer* pPlayer, int PlayerAccountID, int LifeTime, std::vector<StructRandomItem> List, CItemData* pPlayerUsesItem, int UseValue)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_RANDOM_BOX, pPlayer->m_ViewPos)
{
	m_UseValue = UseValue;
	m_LifeTime = LifeTime;
	m_pPlayer = pPlayer;
	m_PlayerAccountID = PlayerAccountID;
	m_pPlayerUsesItem = pPlayerUsesItem;
	std::copy(List.begin(), List.end(), std::back_inserter(m_List));
	GameWorld()->InsertEntity(this);
}

std::vector<StructRandomItem>::iterator CRandomBoxRandomizer::SelectRandomItem()
{
	const float RandomDrop = frandom() * 100.0f;
	const auto pItem = std::find_if(m_List.begin(), m_List.end(), [RandomDrop](const StructRandomItem &pItem) { return RandomDrop < pItem.m_Chance; });
	return pItem != m_List.end() ? pItem : std::prev(m_List.end());
}

void CRandomBoxRandomizer::Tick()
{
	if(!m_LifeTime || m_LifeTime % Server()->TickSpeed() == 0)
	{
		auto pSelectedRandomItem = SelectRandomItem();
		if(m_pPlayer && m_pPlayer->GetCharacter())
		{
			const vec2 PlayerPosition = m_pPlayer->GetCharacter()->m_Core.m_Pos;
			GS()->CreateText(nullptr, false, vec2(PlayerPosition.x, PlayerPosition.y - 80), vec2(0, -0.3f), 15, GS()->GetItemInfo(pSelectedRandomItem->m_ItemID).GetName());
		}

		if(!m_LifeTime)
		{
			auto GiveRandomItem = [&](StructRandomItem& pItem)
			{
				if(GS()->GetItemInfo(pItem.m_ItemID).IsEnchantable())
				{
					for(int i = 0; i < pItem.m_Value; i++)
					{
						if(!m_pPlayer || m_pPlayer->GetItem(pItem.m_ItemID).m_Value >= 1)
						{
							GS()->SendInbox("System", m_PlayerAccountID, "Random box", "Item was not received by you personally.", pItem.m_ItemID, 1);
							continue;
						}

						if(m_pPlayer->GetItem(pItem.m_ItemID).m_Value <= 0)
						{
							m_pPlayer->GetItem(pItem.m_ItemID).Add(1, 0, 0, false);
							GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
						}
					}
				}
				else
				{
					if(!m_pPlayer)
						GS()->SendInbox("System", m_PlayerAccountID, "Random box", "Item was not received by you personally.", pItem.m_ItemID, pItem.m_Value);
					else
					{
						m_pPlayer->GetItem(pItem.m_ItemID).Add(pItem.m_Value, 0, 0, false);
						GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
					}
				}
			};
			struct ReceivedItem { StructRandomItem RandomItem; int Coincidences; };
			std::list<ReceivedItem> aReceivedItems;

			// get list received items
			for(int i = 0; i < m_UseValue; i++)
			{
				auto pItem = std::find_if(aReceivedItems.begin(), aReceivedItems.end(),
					[&pSelectedRandomItem](const ReceivedItem& pItem){ return pItem.RandomItem.m_ItemID == pSelectedRandomItem->m_ItemID; });
				if(pItem != aReceivedItems.end())
				{
					pItem->RandomItem.m_Value += pSelectedRandomItem->m_Value;
					pItem->Coincidences++;
				}
				else
					aReceivedItems.push_back({ *pSelectedRandomItem, 1});

				pSelectedRandomItem = SelectRandomItem();
			}
			
			// got all random items
			if(m_pPlayer)
			{
				const char* pClientName = GS()->Server()->ClientName(m_pPlayer->GetCID());
				GS()->Chat(-1, "---------------------------------");
				GS()->Chat(-1, "{STR} uses '{STR}x{INT}' and got:", pClientName, m_pPlayerUsesItem->Info().GetName(), m_UseValue);

				for(auto& pItem : aReceivedItems)
				{
					CItemData* pPlayerRandomItem = &m_pPlayer->GetItem(pItem.RandomItem.m_ItemID);
					GiveRandomItem(pItem.RandomItem);
					GS()->Chat(-1, "* {STR}x{INT} - ({INT})", pPlayerRandomItem->Info().GetName(), pItem.RandomItem.m_Value, pItem.Coincidences);
				}
				GS()->Chat(-1, "---------------------------------");
			}
			else
			{
				for(auto& pItem : aReceivedItems)
					GiveRandomItem(pItem.RandomItem);
			}
			
			GS()->m_World.DestroyEntity(this);
			return;
		}
	}
	m_LifeTime--;
}

void CRandomBoxRandomizer::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	// then add some effects?
}