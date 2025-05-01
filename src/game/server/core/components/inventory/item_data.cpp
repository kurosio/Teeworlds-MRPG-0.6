/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "item_data.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>

#include "../Eidolons/EidolonManager.h"
#include "../achievements/achievement_data.h"
#include "../mails/mail_wrapper.h"

CGS* CPlayerItem::GS() const
{
	return (CGS*)Server()->GameServerPlayer(m_ClientID);
}

CPlayer* CPlayerItem::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

inline static int randomRangecount(int startrandom, int endrandom, int count)
{
	int result = 0;
	for(int i = 0; i < count; i++)
	{
		const int random = startrandom + rand() % (endrandom - startrandom);
		result += random;
	}
	return result;
}

bool CPlayerItem::SetEnchant(int Enchant)
{
	auto* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->IsAuthed() && m_Value >= 1)
	{
		m_Enchant = Enchant;

		if(Save())
		{
			g_EventListenerManager.Notify<IEventListener::PlayerEnchantItem>(pPlayer, this);
			return true;
		}
	}

	return false;
}

bool CPlayerItem::SetSettings(int Settings)
{
	if(m_Value < 1)
		return false;

	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	m_Settings = Settings;
	return Save();
}

bool CPlayerItem::ShouldAutoEquip() const
{
	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// is equipment slot
	if(Info()->IsEquipmentSlot())
		return !pPlayer->IsEquippedSlot(Info()->GetType());

	// is equipment non slot
	if(Info()->IsEquipmentNonSlot())
		return true;

	return false;
}

bool CPlayerItem::SetDurability(int Durability)
{
	auto* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->IsAuthed() && m_Value >= 1)
	{
		m_Durability = Durability;
		if(Save())
		{
			g_EventListenerManager.Notify<IEventListener::PlayerDurabilityItem>(pPlayer, this);
			return true;
		}
	}

	return false;
}

bool CPlayerItem::SetValue(int Value)
{
	bool Changes = false;

	if(m_Value > Value)
	{
		Changes = Remove(m_Value - Value);
	}
	else if(m_Value < Value)
	{
		Changes = Add((Value - m_Value), m_Settings, m_Enchant, false);
	}

	return Changes;
}

bool CPlayerItem::IsEquipped() const
{
	// is settings or module
	if(Info()->IsEquipmentNonSlot() || Info()->IsGameSetting())
		return m_Value > 0 && m_Settings > 0;

	// is account slots
	auto* pPlayer = GetPlayer();
	auto Type = Info()->GetType();
	auto equippedItemIdOpt = pPlayer->Account()->GetEquippedSlots().getEquippedItemID(Type);
	if(equippedItemIdOpt && *equippedItemIdOpt == m_ID)
		return true;

	// is profession slots
	auto* pProfession = pPlayer->Account()->GetActiveProfession();
	if(pProfession)
	{
		equippedItemIdOpt = pProfession->GetEquippedSlots().getEquippedItemID(Type);
		if(equippedItemIdOpt && *equippedItemIdOpt == m_ID)
			return true;
	}

	// other profession slots
	for(auto& Prof : pPlayer->Account()->GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER))
		{
			equippedItemIdOpt = Prof.GetEquippedSlots().getEquippedItemID(Type);
			if(equippedItemIdOpt && *equippedItemIdOpt == m_ID)
				return true;
		}
	}

	return false;
}

bool CPlayerItem::Add(int Value, int StartSettings, int StartEnchant, bool Message)
{
	if(Value < 1)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// check enchantable type
	const int ClientID = pPlayer->GetCID();
	if(!Info()->IsStackable())
	{
		// send by mail no space for item
		if(m_Value > 0)
		{
			MailWrapper Mail("System", pPlayer->Account()->GetID(), "No place for item.");
			Mail.AddDescLine("You already have this item.");
			Mail.AddDescLine("We can't put it in inventory");
			Mail.AttachItem(CItem(m_ID, 1, StartEnchant, 100));
			Mail.Send();
			return false;
		}

		// not stackable maximal is 1
		Value = 1;
	}

	// first initialize
	if(!m_Value)
	{
		m_Enchant = StartEnchant;
		m_Settings = StartSettings;
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_GOT);
	}

	// sync gold and bank
	if(m_ID == itGold)
	{
		auto NextValue = m_Value + Value;
		const auto MaxCapacity = pPlayer->Account()->GetGoldCapacity();

		if(NextValue > MaxCapacity)
		{
			NextValue -= MaxCapacity;
			pPlayer->Account()->AddGoldToBank(NextValue);
			m_Value = MaxCapacity;
		}
		else
		{
			m_Value = NextValue;
		}
	}
	else
	{
		m_Value += Value;
	}

	// notify listeners
	g_EventListenerManager.Notify<IEventListener::PlayerGotItem>(pPlayer, this, Value);

	// try auto equip item
	if(ShouldAutoEquip() && !IsEquipped())
	{
		Equip();
		GS()->Chat(ClientID, "Auto equip '{} - {}'.", Info()->GetName(), GetStringAttributesInfo(pPlayer));
	}

	// disable notify about items for special group and types
	const auto Group = Info()->GetGroup();
	if(!Message || Group == ItemGroup::Settings || m_ID == itGold)
		return Save();

	// Notify about items
	if(Group == ItemGroup::Equipment)
	{
		const auto Type = Info()->GetType();
		if(Type == ItemType::EquipTitle)
			GS()->Chat(-1, "'{}' unlocked the title: '{}'!", GS()->Server()->ClientName(ClientID), Info()->GetName());
		else if(Type == ItemType::EquipEidolon)
			GS()->Chat(-1, "'{}' obtained an Eidolon: '{}'!", GS()->Server()->ClientName(ClientID), Info()->GetName());
		else
			GS()->Chat(-1, "'{}' obtained the '{}'.", GS()->Server()->ClientName(ClientID), Info()->GetName());
	}
	else if(Group == ItemGroup::Currency)
		GS()->Chat(ClientID, "You received '{} x{} ({})'.", Info()->GetName(), Value, m_Value);
	else
		GS()->Chat(ClientID, "You obtained an '{} x{} ({})'.", Info()->GetName(), Value, m_Value);

	return Save();
}

bool CPlayerItem::Remove(int Value)
{
	Value = minimum(Value, m_Value);

	auto* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->IsAuthed() && Value > 0)
	{
		if(m_Value <= Value)
		{
			UnEquip();
			pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_LOST);
		}

		m_Value -= Value;
		if(Save())
		{
			g_EventListenerManager.Notify<IEventListener::PlayerLostItem>(pPlayer, this, Value);
			return true;
		}
	}

	return false;
}

bool CPlayerItem::Equip()
{
	if(m_Value < 1)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// is game setting or module
	if(Info()->IsGameSetting() || Info()->IsEquipmentNonSlot())
	{
		if(m_Settings)
			return false;

		m_Settings = true;
		g_EventListenerManager.Notify<IEventListener::PlayerEquipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_EQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_EQUIP);
		return Save();
	}

	// by slots
	auto* pAccount = pPlayer->Account();
	if(pAccount->EquipItem(m_ID))
	{
		g_EventListenerManager.Notify<IEventListener::PlayerEquipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_EQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_EQUIP);
		return true;
	}

	return false;
}

bool CPlayerItem::UnEquip()
{
	if(m_Value < 1)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// is game setting or modules
	if(Info()->IsGameSetting() || Info()->IsEquipmentNonSlot())
	{
		if(!m_Settings)
			return false;

		m_Settings = false;
		g_EventListenerManager.Notify<IEventListener::PlayerUnequipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_UNEQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_EQUIP);
		return Save();
	}

	// by slots
	auto* pAccount = pPlayer->Account();
	if(pAccount->UnequipItem(m_ID))
	{
		g_EventListenerManager.Notify<IEventListener::PlayerUnequipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_UNEQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_ITEM_EQUIP);
		return true;
	}

	return false;
}

bool CPlayerItem::Use(int Value)
{
	Value = Info()->IsType(ItemType::UseSingle) ? 1 : minimum(Value, m_Value);
	if(Value <= 0)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// survial capsule experience
	if(m_ID == itCapsuleSurvivalExperience && Remove(Value))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "'{}' used '{} x{}' and got '{} survival experience'.", GS()->Server()->ClientName(m_ClientID), Info()->GetName(), Value, Getting);
		pPlayer->Account()->AddExperience(Getting);
		return true;
	}

	// little bag gold
	if(m_ID == itLittleBagGold && Remove(Value))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "'{}' used '{} x{}' and got '{} gold'.", GS()->Server()->ClientName(m_ClientID), Info()->GetName(), Value, Getting);
		pPlayer->Account()->AddGold(Getting);
		return true;
	}

	// potion health regen
	if(const auto& optPotionContext = Info()->GetPotionContext())
	{
		// check potion recast time
		const auto Type = Info()->GetType();
		auto& recastTick = (Type == ItemType::EquipPotionHeal) ? pPlayer->m_aPlayerTick[HealPotionRecast] : pPlayer->m_aPlayerTick[ManaPotionRecast];
		if(recastTick >= Server()->Tick())
			return true;

		// try use
		if(Remove(Value))
		{
			const auto RecastTime = optPotionContext->Recasttime;
			const auto PotionTime = optPotionContext->Lifetime;
			const auto EffectName = optPotionContext->Effect.c_str();

			pPlayer->m_Effects.Add(EffectName, PotionTime * Server()->TickSpeed());
			GS()->Chat(m_ClientID, "You used '{} x{}'.", Info()->GetName(), Value);
			GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -140.0f), 70, EffectName);
			GS()->CreatePlayerSound(m_ClientID, SOUND_GAME_POTION_START);

			// Update the recast time based on potion type
			recastTick = Server()->Tick() + (RecastTime * Server()->TickSpeed());
		}

		return true;
	}

	// bonus context
	if(const auto& optBonusContext = Info()->GetBonusContext(); optBonusContext.has_value() && Remove(Value))
	{
		TemporaryBonus bonus;
		bonus.Amount = optBonusContext->Amount;
		bonus.Type = optBonusContext->Type;
		bonus.StartTime = time(nullptr);
		bonus.SetDuration(optBonusContext->DurationDays, optBonusContext->DurationHours, optBonusContext->DurationMinutes, 0);
		for(int i = 0; i < Value; i++)
		{
			pPlayer->Account()->GetBonusManager().AddBonus(bonus);
		}
		return true;
	}

	// random box
	if(Info()->GetRandomBox())
	{
		const auto Time = 5 * Server()->TickSpeed();
		Info()->GetRandomBox()->Start(pPlayer, Time, this, Value);
		return true;
	}

	return true;
}

bool CPlayerItem::Drop(int Value)
{
	Value = minimum(Value, m_Value);
	if(Value <= 0)
		return false;

	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayer->GetCharacter())
		return false;

	const auto* pChar = pPlayer->GetCharacter();
	auto Force = vec2(pChar->m_Core.m_Input.m_TargetX, pChar->m_Core.m_Input.m_TargetY);
	if(length(Force) > 10.0f)
	{
		Force = normalize(Force) * 10.0f;
	}

	CItem DropItem = *this;
	if(Remove(Value))
	{
		DropItem.SetValue(Value);
		GS()->EntityManager()->DropItem(pChar->m_Core.m_Pos, -1, DropItem, Force);
		return true;
	}

	return false;
}

bool CPlayerItem::Save()
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	int UserID = pPlayer->Account()->GetID();
	const auto pResCheck = Database->Prepare<DB::SELECT>("ItemID, UserID", "tw_accounts_items", "WHERE ItemID = '{}' AND UserID = '{}'", m_ID, UserID);
	pResCheck->AtExecute([this, UserID](ResultPtr pRes)
	{
		// check database value
		if(pRes->next())
		{
			// remove item
			if(!m_Value)
			{
				Database->Execute<DB::REMOVE>("tw_accounts_items", "WHERE ItemID = '{}' AND UserID = '{}'", m_ID, UserID);
				return;
			}

			// update an item
			Database->Execute<DB::UPDATE>("tw_accounts_items", "Value = '{}', Settings = '{}', Enchant = '{}', Durability = '{}' WHERE UserID = '{}' AND ItemID = '{}'",
				m_Value, m_Settings, m_Enchant, m_Durability, UserID, m_ID);
			return;
		}

		// insert item
		if(m_Value)
		{
			m_Durability = 100;
			Database->Execute<DB::INSERT>("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('{}', '{}', '{}', '{}', '{}')",
				m_ID, UserID, m_Value, m_Settings, m_Enchant);
		}
	});

	return true;
}

// helper functions
CItem CItem::FromJSON(const nlohmann::json& json)
{
	try
	{
		ItemIdentifier ID = json.value("id", 0);
		int Value = json.value("value", 1);
		int Enchant = json.value("enchant", 0);
		int Durability = json.value("durability", 0);

		CItem Item(ID, Value, Enchant, Durability);
		return Item;
	}
	catch(nlohmann::json::exception& s)
	{
		dbg_msg("CItem(FromJSON)", "%s (json %s)", json.dump().c_str(), s.what());
	}

	return {};
}

CItemsContainer CItem::FromArrayJSON(const nlohmann::json& json, const char* pField)
{
	CItemsContainer Container;
	try
	{
		if(json.find(pField) != json.end())
		{
			for(auto& p : json[pField])
			{
				ItemIdentifier ID = p.value("id", 0);
				int Value = p.value("value", 0);
				int Enchant = p.value("enchant", 0);
				int Durability = p.value("durability", 0);

				CItem Item(ID, Value, Enchant, Durability);
				Container.push_back(Item);
			}
		}
	}
	catch(nlohmann::json::exception& s)
	{
		dbg_msg("CItem(FromArrayJson)", "(json %s)", s.what());
	}

	return Container;
}

void CItem::ToJSON(CItem& Item, nlohmann::json& json)
{
	try
	{
		json["id"] = Item.GetID();
		json["value"] = Item.GetValue();
		json["enchant"] = Item.GetEnchant();
		json["durability"] = Item.GetDurability();
	}
	catch(nlohmann::json::exception& s)
	{
		dbg_msg("CItem(ToJSON)", "%s (json %s)", json.dump().c_str(), s.what());
	}
}

void CItem::ToArrayJSON(CItemsContainer& vItems, nlohmann::json& json, const char* pField)
{
	try
	{
		for(auto& p : vItems)
		{
			nlohmann::json jsItem {};
			jsItem["id"] = p.GetID();
			jsItem["value"] = p.GetValue();
			jsItem["enchant"] = p.GetEnchant();
			jsItem["durability"] = p.GetDurability();
			json[pField].push_back(jsItem);
		}
	}
	catch(nlohmann::json::exception& s)
	{
		dbg_msg("CItem(ToArrayJson)", "(json %s)", s.what());
	}
}