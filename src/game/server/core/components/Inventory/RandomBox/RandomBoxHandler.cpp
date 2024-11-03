/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "RandomBoxData.h"
#include "RandomBoxHandler.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "game/server/core/components/mails/mail_wrapper.h"

CEntityRandomBoxRandomizer::CEntityRandomBoxRandomizer(CGameWorld* pGameWorld, CPlayer* pPlayer, int PlayerAccountID, int LifeTime, const std::vector<CRandomItem>& List, CPlayerItem* pPlayerUsesItem, int UseValue)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_RANDOM_BOX, pPlayer->m_ViewPos)
{
	m_Used = UseValue;
	m_LifeTime = LifeTime;
	m_pPlayer = pPlayer;
	m_AccountID = PlayerAccountID;
	m_pPlayerUsesItem = pPlayerUsesItem;
	m_aRandomItems = List;

	GameWorld()->InsertEntity(this);
}

// This function selects a random item from the "m_aRandomItems" vector of CRandomItem objects
std::vector<CRandomItem>::iterator CEntityRandomBoxRandomizer::SelectRandomItem()
{
	// Find the first item in the vector that meets the criteria specified by the lambda function
	const auto iter = std::find_if(m_aRandomItems.begin(), m_aRandomItems.end(), [](const CRandomItem& pItem)
	{
		const float RandomDrop = random_float(100.0f);
		return RandomDrop < pItem.m_Chance;
	});

	// If any item was found, return the iterator pointing to it
	// Otherwise, return an iterator pointing to the last item in the vector
	return iter != m_aRandomItems.end() ? iter : std::prev(m_aRandomItems.end());
}

void CEntityRandomBoxRandomizer::Tick()
{
	// Check if m_LifeTime is zero or a multiple of the server tick speed
	if(!m_LifeTime || m_LifeTime % Server()->TickSpeed() == 0)
	{
		// select random item
		auto IterRandomElement = SelectRandomItem();
		if(m_pPlayer && m_pPlayer->GetCharacter())
		{
			const vec2 PlayerPos = m_pPlayer->GetCharacter()->m_Core.m_Pos;
			GS()->EntityManager()->Text(PlayerPos + vec2(0, -80), 50, GS()->GetItemInfo(IterRandomElement->m_ItemID)->GetName());
		}

		// is last iteration
		if(!m_LifeTime)
		{
			// initialize variables
			struct ReceivedItem
			{
				CRandomItem RandomItem;
				int Coincidences;
			};
			std::list<ReceivedItem> aReceivedItems;
			auto fnGiveItem = [&](const CRandomItem& RandItem)
			{
				const auto HasItem = m_pPlayer->GetItem(RandItem.m_ItemID)->HasItem();
				const auto IsStackable = GS()->GetItemInfo(RandItem.m_ItemID)->IsStackable();

				if(!m_pPlayer || (!IsStackable && HasItem))
				{
					// send mail
					MailWrapper Mail("System", m_AccountID, "Random box.");
					Mail.AddDescLine("Item was not received by you personally.");
					Mail.AttachItem(CItem(RandItem.m_ItemID, RandItem.m_Value));
					Mail.Send();
				}
				else
				{
					// add to inventory
					m_pPlayer->GetItem(RandItem.m_ItemID)->Add(RandItem.m_Value, 0, 0, false);
					GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
				}
			};
			auto findMatchingItem = [&IterRandomElement](const ReceivedItem& pItem)
			{
				return pItem.RandomItem.m_ItemID == IterRandomElement->m_ItemID;
			};

			// add random items
			for(int i = 0; i < m_Used; i++)
			{
				auto iter = std::ranges::find_if(aReceivedItems, findMatchingItem);
				if(iter != aReceivedItems.end())
				{
					iter->RandomItem.m_Value += IterRandomElement->m_Value;
					iter->Coincidences++;
				}
				else
				{
					aReceivedItems.push_back({ *IterRandomElement, 1 });
				}

				// next random item
				IterRandomElement = SelectRandomItem();
			}

			// is player valid
			if(m_pPlayer)
			{
				// send information to chat and give items to the player
				const char* pClientName = GS()->Server()->ClientName(m_pPlayer->GetCID());
				GS()->Chat(-1, "---------------------------------");
				GS()->Chat(-1, "{} uses '{} x{}' and got:", pClientName, m_pPlayerUsesItem->Info()->GetName(), m_Used);
				for(const auto& pItem : aReceivedItems)
				{
					fnGiveItem(pItem.RandomItem);
					GS()->Chat(-1, "* {} x{} - ({})", GS()->GetItemInfo(pItem.RandomItem.m_ItemID)->GetName(), pItem.RandomItem.m_Value, pItem.Coincidences);
				}
				GS()->Chat(-1, "---------------------------------");
			}
			else
			{
				// give items to the player by mail
				for(const auto& pItem : aReceivedItems)
					fnGiveItem(pItem.RandomItem);
			}

			// Destroy the current entity
			GameWorld()->DestroyEntity(this);
			return;
		}
	}

	// Decrement the lifetime variable.
	m_LifeTime--;
}

void CEntityRandomBoxRandomizer::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	// then add some effects?
}