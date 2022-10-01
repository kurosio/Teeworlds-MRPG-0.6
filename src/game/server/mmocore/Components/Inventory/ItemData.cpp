/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ItemData.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

#include "RandomBox.h"

std::map < int, std::map < int, CItemData > > CItemData::ms_aItems;
inline int randomRangecount(int startrandom, int endrandom, int count)
{
	int result = 0;
	for(int i = 0; i < count; i++)
	{
		int random = startrandom + random_int() % (endrandom - startrandom);
		result += random;
	}
	return result;
}

void CItemData::SetItemOwner(CPlayer* pPlayer)
{
	m_pPlayer = pPlayer;
	m_pGS = m_pPlayer->GS();
}

bool CItemData::SetEnchant(int Enchant)
{
	if(m_Value < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	m_Enchant = Enchant;
	return Save();
}

bool CItemData::SetSettings(int Settings)
{
	if(m_Value < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	m_Settings = Settings;
	return Save();
}

bool CItemData::SetDurability(int Durability)
{
	if(m_Value < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	m_Durability = Durability;
	return Save();
}

bool CItemData::Add(int Value, int Settings, int Enchant, bool Message)
{
	if(Value < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	const int ClientID = m_pPlayer->GetCID();
	if(Info().IsEnchantable())
	{
		if(m_Value > 0)
		{
			GS()->Chat(ClientID, "This item cannot have more than 1 item");
			return false;
		}
		Value = 1;
	}

	GS()->Mmo()->Item()->GiveItem(m_pPlayer, m_ItemID, Value, Settings, Enchant);

	// check the empty slot if yes then put the item on
	if((Info().IsType(ItemType::TYPE_EQUIP) && m_pPlayer->GetEquippedItemID(Info().m_Function) <= 0) || Info().IsType(ItemType::TYPE_MODULE))
	{
		if(!IsEquipped())
			Equip();

		char aAttributes[128];
		Info().FormatAttributes(m_pPlayer, aAttributes, sizeof(aAttributes), Enchant);
		GS()->Chat(ClientID, "Auto equip {STR} - {STR}", Info().GetName(), aAttributes);
	}

	if(!Message || Info().IsType(ItemType::TYPE_SETTINGS))
		return true;

	if(Info().IsType(ItemType::TYPE_EQUIP) || Info().IsType(ItemType::TYPE_MODULE))
		GS()->Chat(-1, "{STR} got of the {STR}x{VAL}.", GS()->Server()->ClientName(ClientID), Info().GetName(), Value);
	else if(Info().GetType() != ItemType::TYPE_INVISIBLE)
		GS()->Chat(ClientID, "You got of the {STR}x{VAL}.", Info().GetName(), Value);

	return true;
}

bool CItemData::Remove(int Value, int Settings)
{
	Value = min(Value, m_Value);
	if(Value <= 0 || !m_pPlayer)
		return false;

	if(IsEquipped())
		Equip();

	const int Code = GS()->Mmo()->Item()->RemoveItem(m_pPlayer, m_ItemID, Value, Settings);
	return Code > 0;
}

bool CItemData::Equip()
{
	if(m_Value < 1 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	m_Settings ^= true;

	if(Info().IsType(ItemType::TYPE_EQUIP))
	{
		const ItemFunctional EquipID = Info().m_Function;
		int EquipItemID = m_pPlayer->GetEquippedItemID(EquipID, m_ItemID);
		while(EquipItemID >= 1)
		{
			CItemData& EquipItem = m_pPlayer->GetItem(EquipItemID);
			EquipItem.SetSettings(0);
			EquipItemID = m_pPlayer->GetEquippedItemID(EquipID, m_ItemID);
		}
	}

	if(m_pPlayer->GetCharacter())
		m_pPlayer->GetCharacter()->UpdateEquipingStats(m_ItemID);

	m_pPlayer->ShowInformationStats(BroadcastPriority::GAME_INFORMATION);
	return Save();
}

bool CItemData::Use(int Value)
{
	Value = Info().m_Function == FUNCTION_ONE_USED ? 1 : min(Value, m_Value);
	if(Value <= 0 || !m_pPlayer || !m_pPlayer->IsAuthed())
		return false;

	const int ClientID = m_pPlayer->GetCID();
	// potion health regen
	if(m_ItemID == itPotionHealthRegen && Remove(Value, 0))
	{
		m_pPlayer->GiveEffect("RegenHealth", 15);
		GS()->Chat(ClientID, "You used {STR}x{VAL}", Info().GetName(), Value);
	}
	// potion mana regen
	else if(m_ItemID == itPotionManaRegen && Remove(Value, 0))
	{
		m_pPlayer->GiveEffect("RegenMana", 15);
		GS()->Chat(ClientID, "You used {STR}x{VAL}", Info().GetName(), Value);
	}
	// potion resurrection
	else if(m_ItemID == itPotionResurrection && Remove(Value, 0))
	{
		m_pPlayer->GetTempData().m_TempSafeSpawn = false;
		m_pPlayer->GetTempData().m_TempHealth = m_pPlayer->GetStartHealth();
		GS()->Chat(ClientID, "You used {STR}x{VAL}", Info().GetName(), Value);
	}
	// ticket discount craft
	else if(m_ItemID == itTicketDiscountCraft)
	{
		GS()->Chat(ClientID, "This item can only be used (Auto Use, and then craft).");
	}
	// survial capsule experience
	else if(m_ItemID == itCapsuleSurvivalExperience && Remove(Value, 0))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "{STR} used {STR}x{VAL} and got {VAL} survival experience.", GS()->Server()->ClientName(ClientID), Info().GetName(), Value, Getting);
		m_pPlayer->AddExp(Getting);
	}
	// little bag gold
	else if(m_ItemID == itLittleBagGold && Remove(Value, 0))
	{
		int Getting = randomRangecount(10, 50, Value);
		GS()->Chat(-1, "{STR} used {STR}x{VAL} and got {VAL} gold.", GS()->Server()->ClientName(ClientID), Info().GetName(), Value, Getting);
		m_pPlayer->AddMoney(Getting);
	}
	// ticket reset for class stats
	else if(m_ItemID == itTicketResetClassStats && Remove(Value, 0))
	{
		int BackUpgrades = 0;
		for(const auto& pAttribute : CGS::ms_aAttributsInfo)
		{
			if(str_comp_nocase(pAttribute.second.m_aFieldName, "unfield") == 0 || pAttribute.second.m_UpgradePrice <= 0 || m_pPlayer->Acc().m_aStats[pAttribute.first] <= 0)
				continue;

			// skip weapon spreading
			if(pAttribute.second.m_Type == AtWeapon)
				continue;

			BackUpgrades += m_pPlayer->Acc().m_aStats[pAttribute.first] * pAttribute.second.m_UpgradePrice;
			m_pPlayer->Acc().m_aStats[pAttribute.first] = 0;
		}

		GS()->Chat(-1, "{STR} used {STR} returned {INT} upgrades.", GS()->Server()->ClientName(ClientID), Info().GetName(), BackUpgrades);
		m_pPlayer->Acc().m_Upgrade += BackUpgrades;
		GS()->Mmo()->SaveAccount(m_pPlayer, SAVE_UPGRADES);
	}
	// ticket reset for weapons stats
	else if(m_ItemID == itTicketResetWeaponStats && Remove(Value, 0))
	{
		int BackUpgrades = 0;
		for(const auto& pAttribute : CGS::ms_aAttributsInfo)
		{
			if(str_comp_nocase(pAttribute.second.m_aFieldName, "unfield") == 0 || pAttribute.second.m_UpgradePrice <= 0 || m_pPlayer->Acc().m_aStats[pAttribute.first] <= 0)
				continue;

			// skip all stats allow only weapons
			if(pAttribute.second.m_Type != AtWeapon)
				continue;

			int UpgradeValue = m_pPlayer->Acc().m_aStats[pAttribute.first];
			if(pAttribute.first == StSpreadShotgun)
				UpgradeValue = m_pPlayer->Acc().m_aStats[pAttribute.first] - 3;
			else if(pAttribute.first == StSpreadGrenade || pAttribute.first == StSpreadRifle)
				UpgradeValue = m_pPlayer->Acc().m_aStats[pAttribute.first] - 1;

			if(UpgradeValue <= 0)
				continue;

			BackUpgrades += UpgradeValue * pAttribute.second.m_UpgradePrice;
			m_pPlayer->Acc().m_aStats[pAttribute.first] -= UpgradeValue;
		}

		GS()->Chat(-1, "{STR} used {STR} returned {INT} upgrades.", GS()->Server()->ClientName(ClientID), Info().GetName(), BackUpgrades);
		m_pPlayer->Acc().m_Upgrade += BackUpgrades;
		GS()->Mmo()->SaveAccount(m_pPlayer, SAVE_UPGRADES);
	}
	// Random home decor
	else if(m_ItemID == itRandomHomeDecoration)
	{
		CRandomBox RandomHomeDecor;
		RandomHomeDecor.Add(itDecoArmor, 1, 50.0f);
		RandomHomeDecor.Add(itDecoHealth, 1, 50.0f);
		RandomHomeDecor.Add(itEliteDecoHealth, 1, 10.0f);
		RandomHomeDecor.Add(itEliteDecoNinja, 1, 10.0f);
		RandomHomeDecor.Start(m_pPlayer, 5, this, Value);
	}
	return true;
}

bool CItemData::Drop(int Value)
{
	Value = min(Value, m_Value);
	if(Value <= 0 || !m_pPlayer || !m_pPlayer->IsAuthed() || !m_pPlayer->GetCharacter())
		return false;

	CCharacter* m_pCharacter = m_pPlayer->GetCharacter();
	vec2 Force = vec2(m_pCharacter->m_Core.m_Input.m_TargetX, m_pCharacter->m_Core.m_Input.m_TargetY);
	if(length(Force) > 8.0f)
		Force = normalize(Force) * 8.0f;

	CItemData DropItem = *this;
	if(Remove(Value))
	{
		DropItem.m_Value = Value;
		GS()->CreateDropItem(m_pCharacter->m_Core.m_Pos, -1, DropItem, Force);
		return true;
	}
	return false;
}

bool CItemData::Save() const
{
	if(m_pPlayer && m_pPlayer->IsAuthed())
	{
		Sqlpool.Execute<DB::UPDATE>("tw_accounts_items", "Value = '%d', Settings = '%d', Enchant = '%d', Durability = '%d' WHERE UserID = '%d' AND ItemID = '%d'",
			m_Value, m_Settings, m_Enchant, m_Durability, m_pPlayer->Acc().m_UserID, m_ItemID);
		return true;
	}
	return false;
}
