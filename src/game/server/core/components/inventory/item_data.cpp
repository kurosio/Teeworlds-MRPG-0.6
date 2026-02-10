/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "item_data.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>
#include <generated/server_data.h>

#include <components/Eidolons/EidolonManager.h>
#include <components/mails/mail_wrapper.h>
#include <game/server/core/tools/db_async_context.h>

namespace
{
	class DbInventorySave
	{
		inline static std::unordered_map<uint64_t, uint64_t> ms_PendingSerials {};
		inline static std::mutex ms_PendingSerialsLock {};

		struct CSavePayload
		{
			int m_UserID{};
			int m_ItemID{};
			int m_Value{};
			int m_Settings{};
			int m_Enchant{};
			int m_Durability{};
			uint64_t m_Key{};
			uint64_t m_Serial{};
		};
		using CSaveContextPtr = std::shared_ptr<DbAsync::CContext<CSavePayload>>;
		static uint64_t MakeKey(int UserID, int ItemID)
		{
			return (static_cast<uint64_t>(static_cast<uint32_t>(UserID)) << 32) | static_cast<uint32_t>(ItemID);
		}

		static bool IsLatest(const CSaveContextPtr& pContext)
		{
			std::lock_guard<std::mutex> Guard(ms_PendingSerialsLock);
			const auto& Data = pContext->Data();
			const auto it = ms_PendingSerials.find(Data.m_Key);
			return it != ms_PendingSerials.end() && it->second == Data.m_Serial;
		}

		static uint64_t NextSerial(uint64_t Key)
		{
			std::lock_guard<std::mutex> Guard(ms_PendingSerialsLock);
			return ++ms_PendingSerials[Key];
		}

		static bool ConsumeLatest(const CSaveContextPtr& pContext)
		{
			std::lock_guard<std::mutex> Guard(ms_PendingSerialsLock);
			const auto& Data = pContext->Data();
			const auto it = ms_PendingSerials.find(Data.m_Key);
			if(it == ms_PendingSerials.end() || it->second != Data.m_Serial)
				return false;

			ms_PendingSerials.erase(it);
			return true;
		}

		static void OnFinalize(const CSaveContextPtr& pContext, bool)
		{
			ConsumeLatest(pContext);
		}

		static void OnWrite(const CSaveContextPtr& pContext)
		{
			if(!IsLatest(pContext))
				return;

			const auto& Data = pContext->Data();
			if(Data.m_Value <= 0)
			{
				Database->Execute<DB::REMOVE>([pContext](bool Updated) { OnFinalize(pContext, Updated); },
					"tw_accounts_items", "WHERE ItemID = '{}' AND UserID = '{}'", Data.m_ItemID, Data.m_UserID);
				return;
			}

			Database->Execute<DB::INSERT>([pContext](bool Updated) { OnFinalize(pContext, Updated); },
				"tw_accounts_items",
				"(ItemID, UserID, Value, Settings, Enchant, Durability) VALUES ('{}', '{}', '{}', '{}', '{}', '{}') "
				"ON DUPLICATE KEY UPDATE Value = '{}', Settings = '{}', Enchant = '{}', Durability = '{}'",
				Data.m_ItemID, Data.m_UserID, Data.m_Value, Data.m_Settings, Data.m_Enchant, Data.m_Durability,
				Data.m_Value, Data.m_Settings, Data.m_Enchant, Data.m_Durability);
		}

	public:
		static void Start(CPlayer* pPlayer, int ItemID, int Value, int Settings, int Enchant, int Durability)
		{
			const int UserID = pPlayer->Account()->GetID();
			const uint64_t Key = MakeKey(UserID, ItemID);
			const uint64_t Serial = NextSerial(Key);
			auto pContext = DbAsync::MakeContext<CSavePayload>(pPlayer->GetCID(), CSavePayload {
				UserID, ItemID, Value, Settings, Enchant, Durability > 0 ? Durability : 100, Key, Serial,
			});

			OnWrite(pContext);
		}
	};
}

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

	// is equipment modules
	if(Info()->IsEquipmentModules())
	{
		if((Info()->HasAttributes() && !pPlayer->Account()->GetFreeSlotsAttributedModules()) ||
			(!Info()->HasAttributes() && !pPlayer->Account()->GetFreeSlotsFunctionalModules()))
			return false;

		return true;
	}

	return false;
}

bool CPlayerItem::SetDurability(int Durability)
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->IsAuthed() || m_Value < 1)
		return false;

	int oldDurability = m_Durability;
	m_Durability = Durability;

	if(Save())
	{
		g_EventListenerManager.Notify<IEventListener::PlayerDurabilityItem>(pPlayer, this, oldDurability);
		return true;
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
	if(m_Value <= 0)
		return false;

	// is settings or module
	if(Info()->IsEquipmentModules() || Info()->IsGameSetting())
		return m_Settings > 0;

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
		GS()->Broadcast(m_ClientID, BroadcastPriority::GamePriority, 100, "You received '{} x{} ({})'.", Info()->GetName(), Value, m_Value);
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
	if(Info()->IsGameSetting() || Info()->IsEquipmentModules())
	{
		if(m_Settings)
			return false;

		if(Info()->IsEquipmentModules())
		{
			if((Info()->HasAttributes() && !pPlayer->Account()->GetFreeSlotsAttributedModules()) ||
				(!Info()->HasAttributes() && !pPlayer->Account()->GetFreeSlotsFunctionalModules()))
			{
				GS()->Chat(m_ClientID, "You have no available equipment slots for modules.");
				return false;
			}
		}

		m_Settings = true;
		g_EventListenerManager.Notify<IEventListener::PlayerEquipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_EQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_ITEM_EQUIP);
		return Save();
	}

	// by slots
	auto* pAccount = pPlayer->Account();
	if(pAccount->EquipItem(m_ID))
	{
		g_EventListenerManager.Notify<IEventListener::PlayerEquipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_EQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_ITEM_EQUIP);
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
	if(Info()->IsGameSetting() || Info()->IsEquipmentModules())
	{
		if(!m_Settings)
			return false;

		m_Settings = false;
		g_EventListenerManager.Notify<IEventListener::PlayerUnequipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_UNEQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_ITEM_EQUIP);
		return Save();
	}

	// by slots
	auto* pAccount = pPlayer->Account();
	if(pAccount->UnequipItem(m_ID))
	{
		g_EventListenerManager.Notify<IEventListener::PlayerUnequipItem>(pPlayer, this);
		pPlayer->StartUniversalScenario(Info()->GetScenarioData(), EScenarios::SCENARIO_ON_ITEM_UNEQUIP);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_SFX_ITEM_EQUIP);
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

	// tome upgr-reset Tank
	if(m_ID == itTomeOfUpgrResetTank && Remove(Value))
	{
		pPlayer->Account()->GetProfession(ProfessionIdentifier::Tank)->ResetUpgrades();
		return true;
	}

	// tome upgr-reset Dps
	if(m_ID == itTomeOfUpgrResetDps && Remove(Value))
	{
		pPlayer->Account()->GetProfession(ProfessionIdentifier::Dps)->ResetUpgrades();
		return true;
	}

	// tome upgr-reset Healer
	if(m_ID == itTomeOfUpgrResetHealer && Remove(Value))
	{
		pPlayer->Account()->GetProfession(ProfessionIdentifier::Healer)->ResetUpgrades();
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
			const auto Effect = optPotionContext->Effect;

			pPlayer->m_Effects.Add(Effect, PotionTime * Server()->TickSpeed());
			GS()->Chat(m_ClientID, "You used '{} x{}'.", Info()->GetName(), Value);
			GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -140.0f), 70, EffectName(Effect));
			GS()->CreatePlayerSound(m_ClientID, SOUND_SFX_POTION);

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
	if(Value <= 0 || Info()->HasFlag(ITEMFLAG_CANT_DROP))
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

	DbInventorySave::Start(pPlayer, m_ID, m_Value, m_Settings, m_Enchant, m_Durability);

	return true;
}
