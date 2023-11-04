/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "RandomBoxData.h"
#include "RandomBoxHandler.h"

#include <game/server/gamecontext.h>

CEntityRandomBoxRandomizer::CEntityRandomBoxRandomizer(CGameWorld* pGameWorld, CPlayer* pPlayer, int PlayerAccountID, int LifeTime, std::vector<CRandomItem> List, CPlayerItem* pPlayerUsesItem, int UseValue)
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

std::vector<CRandomItem>::iterator CEntityRandomBoxRandomizer::SelectRandomItem()
{
	const auto iter = std::find_if(m_List.begin(), m_List.end(), [](const CRandomItem &pItem)
	{
		const float RandomDrop = frandom() * 100.0f;
		return RandomDrop < pItem.m_Chance;
	});
	return iter != m_List.end() ? iter : std::prev(m_List.end());
}

void CEntityRandomBoxRandomizer::Tick()
{
	if(!m_LifeTime || m_LifeTime % Server()->TickSpeed() == 0)
	{
		auto iterrandom = SelectRandomItem();
		if(m_pPlayer && m_pPlayer->GetCharacter())
		{
			const vec2 PlayerPosition = m_pPlayer->GetCharacter()->m_Core.m_Pos;
			GS()->CreateText(nullptr, false, vec2(PlayerPosition.x, PlayerPosition.y - 80), vec2(0, -0.3f), 15, GS()->GetItemInfo(iterrandom->m_ItemID)->GetName());
		}

		if(!m_LifeTime)
		{
			// function lambda for check allowed get or send it from inbox
			auto GiveRandomItem = [&](CRandomItem& pItem)
			{
				// for enchantable
				if(GS()->GetItemInfo(pItem.m_ItemID)->IsEnchantable())
				{
					for(int i = 0; i < pItem.m_Value; i++)
					{
						if(!m_pPlayer || m_pPlayer->GetItem(pItem.m_ItemID)->GetValue() >= 1)
						{
							GS()->SendInbox("System", m_PlayerAccountID, "Random box", "Item was not received by you personally.", pItem.m_ItemID, 1);
							continue;
						}

						if(m_pPlayer->GetItem(pItem.m_ItemID)->GetValue() <= 0)
						{
							m_pPlayer->GetItem(pItem.m_ItemID)->Add(1, 0, 0, false);
							GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
						}
					}
				}
				else // default
				{
					if(!m_pPlayer)
					{
						GS()->SendInbox("System", m_PlayerAccountID, "Random box", "Item was not received by you personally.", pItem.m_ItemID, pItem.m_Value);
					}
					else
					{
						m_pPlayer->GetItem(pItem.m_ItemID)->Add(pItem.m_Value, 0, 0, false);
						GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
					}
				}
			};

			// get list received items
			struct ReceivedItem { CRandomItem RandomItem; int Coincidences; };
			std::list<ReceivedItem> aReceivedItems;

			for(int i = 0; i < m_UseValue; i++)
			{
				auto iter = std::find_if(aReceivedItems.begin(), aReceivedItems.end(),[&iterrandom](const ReceivedItem& pItem)
				{
					return pItem.RandomItem.m_ItemID == iterrandom->m_ItemID;
				});

				if(iter != aReceivedItems.end())
				{
					iter->RandomItem.m_Value += iterrandom->m_Value;
					iter->Coincidences++;
				}
				else
				{
					aReceivedItems.push_back({ *iterrandom, 1 });
				}

				iterrandom = SelectRandomItem();
			}
			
			// got all random items
			if(m_pPlayer)
			{
				const char* pClientName = GS()->Server()->ClientName(m_pPlayer->GetCID());
				GS()->Chat(-1, "---------------------------------");
				GS()->Chat(-1, "{STR} uses '{STR}x{VAL}' and got:", pClientName, m_pPlayerUsesItem->Info()->GetName(), m_UseValue);

				for(auto& pItem : aReceivedItems)
				{
					CPlayerItem* pPlayerItem = m_pPlayer->GetItem(pItem.RandomItem.m_ItemID);
					GiveRandomItem(pItem.RandomItem);
					GS()->Chat(-1, "* {STR}x{VAL} - ({INT})", pPlayerItem->Info()->GetName(), pItem.RandomItem.m_Value, pItem.Coincidences);
				}
				GS()->Chat(-1, "---------------------------------");
			}
			else
			{
				for(auto& pItem : aReceivedItems)
					GiveRandomItem(pItem.RandomItem);
			}

			GameWorld()->DestroyEntity(this);
			return;
		}
	}
	m_LifeTime--;
}

void CEntityRandomBoxRandomizer::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	// then add some effects?
}