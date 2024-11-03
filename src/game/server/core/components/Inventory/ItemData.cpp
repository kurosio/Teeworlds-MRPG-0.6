/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemData.h"

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

inline int randomRangecount(int startrandom, int endrandom, int count)
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
	if(m_Value < 1)
		return false;

	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	m_Enchant = Enchant;
	return Save();
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

bool CPlayerItem::SetDurability(int Durability)
{
	if(m_Value < 1)
		return false;

	const auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	m_Durability = Durability;
	return Save();
}

bool CPlayerItem::SetValue(int Value)
{
	bool Changes = false;

	if(m_Value > Value)
	{
		Changes = Remove((m_Value - Value));
	}
	else if(m_Value < Value)
	{
		Changes = Add((Value - m_Value), m_Settings, m_Enchant, false);
	}

	return Changes;
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
		if(m_Value > 0)
		{
			MailWrapper Mail("System", pPlayer->Account()->GetID(), "No place for item.");
			Mail.AddDescLine("You already have this item.");
			Mail.AddDescLine("We can't put it in inventory");
			Mail.AttachItem({m_ID, 1, StartEnchant, 100});
			Mail.Send();
			return false;
		}

		Value = 1;
	}

	if(!m_Value)
	{
		m_Enchant = StartEnchant;
		m_Settings = StartSettings;
		Info()->StartItemScenario(pPlayer, ItemScenarioEvent::OnEventGot);
	}

	m_Value += Value;

	// achievement for item
	pPlayer->UpdateAchievement(AchievementType::ReceiveItem, m_ID, Value, PROGRESS_ACCUMULATE);
	pPlayer->UpdateAchievement(AchievementType::HaveItem, m_ID, m_Value, PROGRESS_ABSOLUTE);

	// check the empty slot if yes then put the item on
	if((Info()->IsType(ItemType::Equipment) && !pPlayer->GetEquippedItemID(Info()->GetFunctional()).has_value()) || Info()->IsType(ItemType::Module))
	{
		if(!IsEquipped())
			Equip(false);

		GS()->Chat(ClientID, "Auto equip {} - {}", Info()->GetName(), GetStringAttributesInfo(pPlayer));
	}

	if(!Message || Info()->IsType(ItemType::Setting) || Info()->IsType(ItemType::Invisible))
		return Save();

	if(Info()->IsType(ItemType::Equipment) || Info()->IsType(ItemType::Module))
	{
		GS()->Chat(-1, "{} got of the {}.", GS()->Server()->ClientName(ClientID), Info()->GetName());
		if(Info()->IsFunctional(EquipEidolon))
		{
			std::pair EidolonSize = GS()->Core()->EidolonManager()->GetEidolonsSize(ClientID);
			GS()->Chat(-1, "{} has a collection {} out of {} eidolons.", GS()->Server()->ClientName(ClientID), EidolonSize.first, EidolonSize.second);
		}
	}
	else
	{
		GS()->Chat(ClientID, "You obtain an {} x{}({}).", Info()->GetName(), Value, m_Value);
	}

	return Save();
}

bool CPlayerItem::Remove(int Value)
{
	Value = minimum(Value, m_Value);
	if(Value <= 0)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	// unequip if this is the last item
	if(m_Value <= Value && IsEquipped())
	{
		Equip(false);
		Info()->StartItemScenario(pPlayer, ItemScenarioEvent::OnEventLost);
	}

	m_Value -= Value;
	return Save();
}

bool CPlayerItem::Equip(bool SaveItem)
{
	if(m_Value < 1)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	m_Settings ^= true;

	if(Info()->IsType(ItemType::Equipment))
	{
		const ItemFunctional EquipID = Info()->GetFunctional();
		auto ItemID = pPlayer->GetEquippedItemID(EquipID, m_ID);
		while(ItemID.has_value())
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID.value());
			pPlayerItem->SetSettings(0);
			ItemID = pPlayer->GetEquippedItemID(EquipID, m_ID);
		}
	}

	if(pPlayer->GetCharacter())
	{
		pPlayer->GetCharacter()->UpdateEquippedStats(m_ID);
	}

	GS()->CreatePlayerSound(m_ClientID, SOUND_ITEM_EQUIP);
	pPlayer->UpdateAchievement(AchievementType::Equip, m_ID, m_Settings, PROGRESS_ABSOLUTE);
	Info()->StartItemScenario(pPlayer, m_Settings ? ItemScenarioEvent::OnEventEquip : ItemScenarioEvent::OnEventUnequip);
	GS()->MarkUpdatedBroadcast(m_ClientID);
	return SaveItem ? Save() : true;
}

bool CPlayerItem::Use(int Value)
{
	Value = Info()->IsFunctional(UseSingle) ? 1 : minimum(Value, m_Value);
	if(Value <= 0)
		return false;

	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	const int ClientID = pPlayer->GetCID();

	// ticket discount craft
	if(m_ID == itTicketDiscountCraft)
	{
		GS()->Chat(ClientID, "This item can only be used (Auto Use, and then craft).");
		return true;
	}
	// survial capsule experience
	if(m_ID == itCapsuleSurvivalExperience && Remove(Value))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "{} used {} x{} and got {} survival experience.", GS()->Server()->ClientName(ClientID), Info()->GetName(), Value, Getting);
		pPlayer->Account()->AddExperience(Getting);
		return true;
	}
	// little bag gold
	if(m_ID == itLittleBagGold && Remove(Value))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "{} used {} x{} and got {} gold.", GS()->Server()->ClientName(ClientID), Info()->GetName(), Value, Getting);
		pPlayer->Account()->AddGold(Getting);
		return true;
	}
	// ticket reset for class stats
	/*if(m_ID == itTicketResetClassStats && Remove(Value))
	{
		int BackUpgrades = 0;
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasDatabaseField() && pPlayer->Account()->m_aStats[ID] > 0)
			{
				// skip weapon spreading
				if(pAttribute->IsGroup(AttributeGroup::Weapon))
					continue;

				BackUpgrades += pPlayer->Account()->m_aStats[ID] * pAttribute->GetUpgradePrice();
				pPlayer->Account()->m_aStats[ID] = 0;
			}
		}

		GS()->Chat(-1, "{} used {} returned {} upgrades.", GS()->Server()->ClientName(ClientID), Info()->GetName(), BackUpgrades);
		pPlayer->Account()->m_UpgradePoint += BackUpgrades;
		GS()->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);
		return true;
	}
	// ticket reset for weapons stats
	if(m_ID == itTicketResetWeaponStats && Remove(Value))
	{
		int BackUpgrades = 0;
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasDatabaseField() && pPlayer->Account()->m_aStats[ID] > 0)
			{
				// skip all stats allow only weapons
				if(pAttribute->GetGroup() != AttributeGroup::Weapon)
					continue;

				const int UpgradeValue = pPlayer->Account()->m_aStats[ID];
				if(UpgradeValue <= 0)
					continue;

				BackUpgrades += UpgradeValue * pAttribute->GetUpgradePrice();
				pPlayer->Account()->m_aStats[ID] -= UpgradeValue;
			}
		}

		GS()->Chat(-1, "{} used {} returned {} upgrades.", GS()->Server()->ClientName(ClientID), Info()->GetName(), BackUpgrades);
		pPlayer->Account()->m_UpgradePoint += BackUpgrades;
		GS()->Core()->SaveAccount(pPlayer, SAVE_UPGRADES);
		return true;
	} TODO FIX Reset stats*/

	// potion health regen
	if(const auto optPotionContext = Info()->GetPotionContext())
	{
		// check potion recast time
		const auto Function = Info()->GetFunctional();
		if((Function == EquipPotionHeal && pPlayer->m_aPlayerTick[HealPotionRecast] >= Server()->Tick()) ||
			(Function == EquipPotionMana && pPlayer->m_aPlayerTick[ManaPotionRecast] >= Server()->Tick()))
		{
			return true;
		}

		// try use
		if(Remove(Value))
		{
			const auto PotionTime = optPotionContext->Lifetime;
			const auto EffectName = optPotionContext->Effect.c_str();

			pPlayer->m_Effects.Add(EffectName, PotionTime * Server()->TickSpeed());
			GS()->Chat(ClientID, "You used {} x{}", Info()->GetName(), Value);
			GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -140.0f), 70, EffectName);

			// Update the recast time based on potion type
			auto& recastTick = (Function == EquipPotionHeal) ? pPlayer->m_aPlayerTick[HealPotionRecast] : pPlayer->m_aPlayerTick[ManaPotionRecast];
			recastTick = Server()->Tick() + ((PotionTime + POTION_RECAST_APPEND_TIME) * Server()->TickSpeed());
		}

		return true;
	}

	// or if it random box
	if(Info()->GetRandomBox())
	{
		Info()->GetRandomBox()->Start(pPlayer, 5, this, Value);
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
	if(length(Force) > 8.0f)
	{
		Force = normalize(Force) * 8.0f;
	}

	CPlayerItem DropItem = *this;
	if(Remove(Value))
	{
		DropItem.m_Value = Value;
		GS()->EntityManager()->DropItem(pChar->m_Core.m_Pos, -1, static_cast<CItem>(DropItem), Force);
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
				m_Value, m_Settings, m_Enchant, m_Durability, GetPlayer()->Account()->GetID(), m_ID);
			return;
		}

		// insert item
		if(m_Value)
		{
			m_Durability = 100;
			Database->Execute<DB::INSERT>("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('{}', '{}', '{}', '{}', '{}')", m_ID, UserID, m_Value, m_Settings, m_Enchant);
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
	catch (nlohmann::json::exception& s)
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
	catch (nlohmann::json::exception& s)
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
	catch (nlohmann::json::exception& s)
	{
		dbg_msg("CItem(ToArrayJson)", "(json %s)", s.what());
	}
}
