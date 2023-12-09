/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemData.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Eidolons/EidolonManager.h"

CGS* CPlayerItem::GS() const
{
	return (CGS*)Server()->GameServerPlayer(m_ClientID);
}

CPlayer* CPlayerItem::GetPlayer() const
{
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		return GS()->m_apPlayers[m_ClientID];
	}
	return nullptr;
}

inline int randomRangecount(int startrandom, int endrandom, int count)
{
	int result = 0;
	for(int i = 0; i < count; i++)
	{
		int random = startrandom + rand() % (endrandom - startrandom);
		result += random;
	}
	return result;
}

bool CPlayerItem::SetEnchant(int Enchant)
{
	if(m_Value < 1 || !GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	m_Enchant = Enchant;
	return Save();
}

bool CPlayerItem::SetSettings(int Settings)
{
	if(m_Value < 1 || !GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	m_Settings = Settings;
	return Save();
}

bool CPlayerItem::SetDurability(int Durability)
{
	if(m_Value < 1 || !GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	m_Durability = Durability;
	return Save();
}

bool CPlayerItem::SetValue(int Value)
{
	bool Changes = false;
	if(m_Value > Value)
		Changes = Remove((m_Value - Value));
	else if(m_Value < Value)
		Changes = Add((Value - m_Value), m_Settings, m_Enchant, false);
	return Changes;
}

bool CPlayerItem::Add(int Value, int StartSettings, int StartEnchant, bool Message)
{
	if(Value < 1 || !GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	const int ClientID = GetPlayer()->GetCID();
	if(Info()->IsEnchantable())
	{
		if(m_Value > 0)
		{
			GS()->Chat(ClientID, "This item cannot have more than 1 item");
			return false;
		}
		Value = 1;
	}

	if(!m_Value)
	{
		m_Enchant = StartEnchant;
		m_Settings = StartSettings;
	}
	m_Value += Value;

	// check the empty slot if yes then put the item on
	if((Info()->IsType(ItemType::TYPE_EQUIP) && GetPlayer()->GetEquippedItemID(Info()->GetFunctional()) <= 0) || Info()->IsType(ItemType::TYPE_MODULE))
	{
		if(!IsEquipped())
			Equip(false);

		char aAttributes[128];
		Info()->StrFormatAttributes(GetPlayer(), aAttributes, sizeof(aAttributes), StartEnchant);
		GS()->Chat(ClientID, "Auto equip {STR} - {STR}", Info()->GetName(), aAttributes);
	}

	if(!Message || Info()->IsType(ItemType::TYPE_SETTINGS) || Info()->IsType(ItemType::TYPE_INVISIBLE))
		return Save();

	if(Info()->IsType(ItemType::TYPE_EQUIP) || Info()->IsType(ItemType::TYPE_MODULE))
	{
		GS()->Chat(-1, "{STR} got of the {STR}.", GS()->Server()->ClientName(ClientID), Info()->GetName());
		if(Info()->IsFunctional(EQUIP_EIDOLON))
		{
			std::pair EidolonSize = GS()->Mmo()->Eidolons()->GetEidolonsSize(ClientID);
			GS()->Chat(-1, "{STR} has a collection {INT} out of {INT} eidolons.", GS()->Server()->ClientName(ClientID), EidolonSize.first, EidolonSize.second);
		}
	}
	else
		GS()->Chat(ClientID, "You obtain an {STR}x{VAL}({VAL}).", Info()->GetName(), Value, m_Value);
	return Save();
}

bool CPlayerItem::Remove(int Value)
{
	Value = minimum(Value, m_Value);
	if(Value <= 0 || !GetPlayer())
		return false;

	// unequip if this is the last item
	if(m_Value <= Value && IsEquipped())
		Equip(false);

	m_Value -= Value;
	return Save();
}

bool CPlayerItem::Equip(bool SaveItem)
{
	if(m_Value < 1 || !GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	m_Settings ^= true;

	if(Info()->IsType(ItemType::TYPE_EQUIP))
	{
		const ItemFunctional EquipID = Info()->GetFunctional();
		ItemIdentifier ItemID = GetPlayer()->GetEquippedItemID(EquipID, m_ID);
		while(ItemID >= 1)
		{
			CPlayerItem* pPlayerItem = GetPlayer()->GetItem(ItemID);
			pPlayerItem->SetSettings(0);
			ItemID = GetPlayer()->GetEquippedItemID(EquipID, m_ID);
		}
	}

	if(GetPlayer()->GetCharacter())
		GetPlayer()->GetCharacter()->UpdateEquipingStats(m_ID);

	GS()->MarkUpdatedBroadcast(m_ClientID);
	return SaveItem ? Save() : true;
}

bool CPlayerItem::Use(int Value)
{
	Value = Info()->IsFunctional(FUNCTION_ONE_USED) ? 1 : minimum(Value, m_Value);
	if(Value <= 0 || !GetPlayer() || !GetPlayer()->IsAuthed())
		return false;

	const int ClientID = GetPlayer()->GetCID();

	// potion mana regen
	if(m_ID == itPotionManaRegen && Remove(Value))
	{
		GetPlayer()->GiveEffect("RegenMana", 15);
		GS()->Chat(ClientID, "You used {STR}x{VAL}", Info()->GetName(), Value);
		return true;
	}
	// potion resurrection
	if(m_ID == itPotionResurrection && Remove(Value))
	{
		GetPlayer()->GetTempData().m_TempSafeSpawn = false;
		GetPlayer()->GetTempData().m_TempHealth = GetPlayer()->GetStartHealth();
		GS()->Chat(ClientID, "You used {STR}x{VAL}", Info()->GetName(), Value);
		return true;
	}
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
		GS()->Chat(-1, "{STR} used {STR}x{VAL} and got {VAL} survival experience.", GS()->Server()->ClientName(ClientID), Info()->GetName(), Value, Getting);
		GetPlayer()->Acc()->AddExperience(Getting);
		return true;
	}
	// little bag gold
	if(m_ID == itLittleBagGold && Remove(Value))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "{STR} used {STR}x{VAL} and got {VAL} gold.", GS()->Server()->ClientName(ClientID), Info()->GetName(), Value, Getting);
		GetPlayer()->AddMoney(Getting);
		return true;
	}
	// ticket reset for class stats
	if(m_ID == itTicketResetClassStats && Remove(Value))
	{
		int BackUpgrades = 0;
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasDatabaseField() && GetPlayer()->Acc()->m_aStats[ID] > 0)
			{
				// skip weapon spreading
				if(pAttribute->IsType(AttributeType::Weapon))
					continue;

				BackUpgrades += GetPlayer()->Acc()->m_aStats[ID] * pAttribute->GetUpgradePrice();
				GetPlayer()->Acc()->m_aStats[ID] = 0;
			}
		}

		GS()->Chat(-1, "{STR} used {STR} returned {INT} upgrades.", GS()->Server()->ClientName(ClientID), Info()->GetName(), BackUpgrades);
		GetPlayer()->Acc()->m_Upgrade += BackUpgrades;
		GS()->Mmo()->SaveAccount(GetPlayer(), SAVE_UPGRADES);
		return true;
	}
	// ticket reset for weapons stats
	if(m_ID == itTicketResetWeaponStats && Remove(Value))
	{
		int BackUpgrades = 0;
		for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
		{
			if(pAttribute->HasDatabaseField() && GetPlayer()->Acc()->m_aStats[ID] > 0)
			{
				// skip all stats allow only weapons
				if(pAttribute->GetType() != AttributeType::Weapon)
					continue;

				int UpgradeValue = GetPlayer()->Acc()->m_aStats[ID];
				if(ID == AttributeIdentifier::SpreadShotgun)
					UpgradeValue = GetPlayer()->Acc()->m_aStats[ID] - 3;
				else if(ID == AttributeIdentifier::SpreadGrenade || ID == AttributeIdentifier::SpreadRifle)
					UpgradeValue = GetPlayer()->Acc()->m_aStats[ID] - 1;

				if(UpgradeValue <= 0)
					continue;

				BackUpgrades += UpgradeValue * pAttribute->GetUpgradePrice();
				GetPlayer()->Acc()->m_aStats[ID] -= UpgradeValue;
			}
		}

		GS()->Chat(-1, "{STR} used {STR} returned {INT} upgrades.", GS()->Server()->ClientName(ClientID), Info()->GetName(), BackUpgrades);
		GetPlayer()->Acc()->m_Upgrade += BackUpgrades;
		GS()->Mmo()->SaveAccount(GetPlayer(), SAVE_UPGRADES);
		return true;
	}

	// potion health regen
	if(const PotionTools::Heal* pHeal = PotionTools::Heal::getHealInfo(m_ID); pHeal)
	{
		if(GetPlayer()->m_aPlayerTick[PotionRecast] >= Server()->Tick())
			return true;

		if(Remove(Value))
		{
			int PotionTime = pHeal->getTime();
			GetPlayer()->GiveEffect(pHeal->getEffect(), PotionTime);
			GetPlayer()->m_aPlayerTick[PotionRecast] = Server()->Tick() + ((PotionTime + POTION_RECAST_APPEND_TIME) * Server()->TickSpeed());

			GS()->Chat(ClientID, "You used {STR}x{VAL}", Info()->GetName(), Value);
			GS()->CreateText(nullptr, false, vec2(GetPlayer()->m_ViewPos.x, GetPlayer()->m_ViewPos.y - 140.0f), vec2(), 70, pHeal->getEffect());
		}
		return true;
	}

	// or if it random box
	if(Info()->GetRandomBox())
	{
		Info()->GetRandomBox()->Start(GetPlayer(), 5, this, Value);
		return true;
	}

	return true;
}

bool CPlayerItem::Drop(int Value)
{
	Value = minimum(Value, m_Value);
	if(Value <= 0 || !GetPlayer() || !GetPlayer()->IsAuthed() || !GetPlayer()->GetCharacter())
		return false;

	CCharacter* m_pCharacter = GetPlayer()->GetCharacter();
	vec2 Force = vec2(m_pCharacter->m_Core.m_Input.m_TargetX, m_pCharacter->m_Core.m_Input.m_TargetY);
	if(length(Force) > 8.0f)
		Force = normalize(Force) * 8.0f;

	CPlayerItem DropItem = *this;
	if(Remove(Value))
	{
		DropItem.m_Value = Value;
		GS()->CreateDropItem(m_pCharacter->m_Core.m_Pos, -1, static_cast<CItem>(DropItem), Force);
		return true;
	}
	return false;
}

bool CPlayerItem::Save()
{
	if(GetPlayer() && GetPlayer()->IsAuthed())
	{
		int UserID = GetPlayer()->Acc()->GetID();
		const auto pResCheck = Database->Prepare<DB::SELECT>("ItemID, UserID", "tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", m_ID, UserID);
		pResCheck->AtExecute([this, UserID](ResultPtr pRes)
		{
			// check database value
			if(pRes->next())
			{
				// remove item
				if(!m_Value)
				{
					Database->Execute<DB::REMOVE>("tw_accounts_items", "WHERE ItemID = '%d' AND UserID = '%d'", m_ID, UserID);
					return;
				}

				// update an item
				Database->Execute<DB::UPDATE>("tw_accounts_items", "Value = '%d', Settings = '%d', Enchant = '%d', Durability = '%d' WHERE UserID = '%d' AND ItemID = '%d'",
					m_Value, m_Settings, m_Enchant, m_Durability, GetPlayer()->Acc()->GetID(), m_ID);
				return;
			}

			// insert item
			if(m_Value)
			{
				m_Durability = 100;
				Database->Execute<DB::INSERT>("tw_accounts_items", "(ItemID, UserID, Value, Settings, Enchant) VALUES ('%d', '%d', '%d', '%d', '%d')", m_ID, UserID, m_Value, m_Settings, m_Enchant);
			}
		});
		return true;
	}
	return false;
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
