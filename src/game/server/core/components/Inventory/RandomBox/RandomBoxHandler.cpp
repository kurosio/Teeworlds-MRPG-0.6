/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "RandomBoxData.h"
#include "RandomBoxHandler.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "game/server/core/components/mails/mail_wrapper.h"

CEntityRandomBoxRandomizer::CEntityRandomBoxRandomizer(CGameWorld* pGameWorld, int AccountID, int Lifetime,
	const ChanceProcessor<CRandomItem>& ChanceProcessor, CPlayerItem* pPlayerUsesItem, int Value)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_RANDOM_BOX, {})
{
	m_Used = Value;
	m_Lifetime = Lifetime;
	m_AccountID = AccountID;
	m_pPlayerUsesItem = pPlayerUsesItem;
	m_ChanceProcessor = ChanceProcessor;
	m_ChanceProcessor.normalizeChances();

	GameWorld()->InsertEntity(this);
}

void CEntityRandomBoxRandomizer::Tick()
{
	// select random element every sec
	if(m_Current.isEmpty() || m_Lifetime % Server()->TickSpeed() == 0 || !m_Lifetime)
	{
		m_Current = m_ChanceProcessor.getRandomElement();

		const auto* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
		if(pPlayer && pPlayer->GetCharacter())
		{
			const auto Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
			GS()->EntityManager()->Text(Pos + vec2(0, -80), 50, GS()->GetItemInfo(m_Current.ItemID)->GetName());
		}
	}

	// post giving element
	if(!m_Lifetime)
	{
		Finish();
		GameWorld()->DestroyEntity(this);
		return;
	}

	m_Lifetime--;
}

void CEntityRandomBoxRandomizer::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
}

void CEntityRandomBoxRandomizer::Finish()
{
	// initialize variables
	struct ReceivedItem
	{
		CRandomItem Element;
		int Coincidences;
	};
	std::unordered_map<int, ReceivedItem> vReceivedItems;

	// lambda find matching items
	for(int i = 0; i < m_Used; i++)
	{
		int itemID = m_Current.ItemID;
		auto& Received = vReceivedItems[itemID];

		if(Received.Coincidences <= 0)
		{
			Received.Element = m_Current;
			Received.Coincidences = 1;
		}
		else
		{
			Received.Element.Value += m_Current.Value;
			Received.Coincidences++;
		}

		// get next element
		m_Current = m_ChanceProcessor.getRandomElement();
	}

	// give from game
	if(auto pPlayer = GS()->GetPlayerByUserID(m_AccountID))
	{
		// send information
		const char* pClientName = GS()->Server()->ClientName(pPlayer->GetCID());
		GS()->Chat(-1, "---------------------------------");
		GS()->Chat(-1, "'{}' uses '{} x{}' and got:", pClientName, m_pPlayerUsesItem->Info()->GetName(), m_Used);
		for(const auto& [ItemID, Received] : vReceivedItems)
		{
			const auto Value = Received.Element.Value;
			auto* pItemInfo = GS()->GetItemInfo(ItemID);
			GS()->Chat(-1, "* '{} x{} - ({})'", pItemInfo->GetName(), Value, Received.Coincidences);
		}
		GS()->Chat(-1, "---------------------------------");

		// give items
		for(const auto& [ItemID, Received] : vReceivedItems)
		{
			const auto Value = Received.Element.Value;
			auto* pPlayerItem = pPlayer->GetItem(ItemID);
			pPlayerItem->Add(Value);
		}

		// create death effect
		constexpr int EffectNum = 8;
		constexpr float Radius = 98.f;
		const float AngleStep = 2.0f * pi / EffectNum;

		for(int i = 0; i < EffectNum; ++i)
		{
			const float Angle = i * AngleStep;
			const vec2 EffectPos = pPlayer->m_ViewPos + vec2(Radius * cos(Angle), Radius * sin(Angle));
			GS()->CreateDeath(EffectPos, pPlayer->GetCID());
		}

		return;
	}

	// send by mail
	MailWrapper Mail("System", m_AccountID, "Random box.");
	Mail.AddDescLine("Item was not received by you personally.");
	for(const auto& [ItemID, Received] : vReceivedItems)
	{
		Mail.AttachItem(CItem(ItemID, Received.Element.Value));
	}
	Mail.Send();
}
