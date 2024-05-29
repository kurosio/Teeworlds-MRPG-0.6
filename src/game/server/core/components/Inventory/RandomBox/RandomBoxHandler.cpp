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
		// Select a random item
		auto IterRandomElement = SelectRandomItem();

		// Check if the player exists and if the player's character exists
		if(m_pPlayer && m_pPlayer->GetCharacter())
		{
			const vec2 PlayerPos = m_pPlayer->GetCharacter()->m_Core.m_Pos;
			GS()->EntityManager()->Text(PlayerPos + vec2(0, -80), 50, GS()->GetItemInfo(IterRandomElement->m_ItemID)->GetName());
		}

		if(!m_LifeTime)
		{
			// function lambda for check allowed get or send it from inbox
			auto GiveRandomItem = [&](const CRandomItem& RandItem)
			{
				// Prepare mail
				MailWrapper Mail("System", m_AccountID, "Random box.");
				Mail.AddDescLine("Item was not received by you personally.");

				// Check if the item is enchantable
				if(GS()->GetItemInfo(RandItem.m_ItemID)->IsEnchantable())
				{
					// Loop through the value of the random item
					for(int i = 0; i < RandItem.m_Value; i++)
					{
						// Check if the player doesn't exist or already has the item
						if(!m_pPlayer || m_pPlayer->GetItem(RandItem.m_ItemID)->HasItem())
						{
							// Send a message to the player's inbox stating the item was not received personally
							Mail.AttachItem(CItem(RandItem.m_ItemID, 1));
							Mail.Send();
							continue;
						}

						// Add the item to the player
						m_pPlayer->GetItem(RandItem.m_ItemID)->Add(1, 0, 0, false);
						GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
					}
				}
				else // if the item is not enchantable
				{
					// Check if the player doesn't exist
					if(!m_pPlayer)
					{
						// Send a message to the player's inbox stating the item was not received personally
						Mail.AttachItem(CItem(RandItem.m_ItemID, RandItem.m_Value));
						Mail.Send();
						return;
					}

					// Add the item to the player
					m_pPlayer->GetItem(RandItem.m_ItemID)->Add(RandItem.m_Value, 0, 0, false);
					GS()->CreateDeath(m_pPlayer->m_ViewPos, m_pPlayer->GetCID());
				}
			};

			// get list received items
			struct ReceivedItem { CRandomItem RandomItem; int Coincidences; };
			std::list<ReceivedItem> aReceivedItems;

			// Define a lambda function called "findMatchingItem"
			auto findMatchingItem = [&IterRandomElement](const ReceivedItem& pItem)
			{
				return pItem.RandomItem.m_ItemID == IterRandomElement->m_ItemID;
			};

			// Iterate "m_Used" number of times
			for(int i = 0; i < m_Used; i++)
			{
				// Find the first element in the vector "aReceivedItems" that satisfies the condition specified by the lambda function "findMatchingItem"
				auto iter = std::find_if(aReceivedItems.begin(), aReceivedItems.end(), findMatchingItem);

				// If an element was found
				if(iter != aReceivedItems.end())
				{
					// Increment the values of the element's RandomItem member
					iter->RandomItem.m_Value += IterRandomElement->m_Value;
					iter->Coincidences++;
				}
				else
				{
					// If no element was found, add a new element
					aReceivedItems.push_back({ *IterRandomElement, 1 });
				}

				// Assign a new random item to IterRandomElement
				IterRandomElement = SelectRandomItem();
			}

			// Check if the player exists
			if(m_pPlayer)
			{
				// Send a chat message
				const char* pClientName = GS()->Server()->ClientName(m_pPlayer->GetCID());
				GS()->Chat(-1, "---------------------------------");
				GS()->Chat(-1, "{} uses '{}x{}' and got:", pClientName, m_pPlayerUsesItem->Info()->GetName(), m_Used);

				// Iterate through all the received items / information
				for(auto& pItem : aReceivedItems)
				{
					CPlayerItem* pPlayerItem = m_pPlayer->GetItem(pItem.RandomItem.m_ItemID);
					GiveRandomItem(pItem.RandomItem);
					GS()->Chat(-1, "* {}x{} - ({})", pPlayerItem->Info()->GetName(), pItem.RandomItem.m_Value, pItem.Coincidences);
				}
				GS()->Chat(-1, "---------------------------------");
			}
			else
			{
				// Give the random items to the player offline
				for(auto& pItem : aReceivedItems)
					GiveRandomItem(pItem.RandomItem);
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