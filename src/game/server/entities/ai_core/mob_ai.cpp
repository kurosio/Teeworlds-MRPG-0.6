#include "mob_ai.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>
#include <generated/server_data.h>

#include <game/server/entities/character_bot.h>
#include <game/server/playerbot.h>
#include <game/server/core/components/quests/quest_manager.h>
#include <game/server/core/components/tunes/tune_zone_manager.h>
#include <game/server/core/components/skills/entities/attack_teleport/attack_teleport.h>

CMobAI::CMobAI(MobBotInfo* pNpcInfo, CPlayerBot* pPlayer, CCharacterBotAI* pCharacter)
	: CBaseAI(pPlayer, pCharacter), m_pMobInfo(pNpcInfo) { }

bool CMobAI::CanDamage(CPlayer* pFrom)
{
	if(!pFrom->IsBot())
		return true;

	const auto* pFromBot = static_cast<CPlayerBot*>(pFrom);
	if(pFromBot && (pFromBot->GetBotType() == TYPE_BOT_EIDOLON || pFromBot->GetBotType() == TYPE_BOT_NPC))
		return true;

	return false;
}

void CMobAI::OnSpawn()
{
	m_EmotionStyle = EMOTE_ANGRY;

	if(m_pMobInfo->m_Boss)
	{
		EnableBotIndicator(POWERUP_WEAPON, WEAPON_HAMMER);
		GS()->ChatWorld(m_pMobInfo->m_WorldID, nullptr, "In your zone emerging {}!", m_pMobInfo->GetName());
		GS()->CreatePlayerSound(-1, SOUND_GAME_BOSS_RESPAWN);
	}
}

void CMobAI::OnGiveRandomEffect(int ClientID)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	if(const auto* pBuff = m_pMobInfo->GetRandomDebuff())
	{
		pPlayer->m_Effects.Add(pBuff->getEffect(), pBuff->getTime(), pBuff->getChance());
	}
}

void CMobAI::OnHandleTunning(CTuningParams* pTuning)
{
	// behavior slower
	if(m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_SLOWER))
	{
		pTuning->ApplyDiff(CTuneZoneManager::GetInstance().GetParams(ETuneZone::SLOW));
		m_pCharacter->m_TuneZoneOverride = (int)ETuneZone::SLOW;
	}
}

void CMobAI::OnRewardPlayer(CPlayer* pPlayer, vec2 Force) const
{
	const int ClientID = pPlayer->GetCID();
	const int MobLevel = m_pMobInfo->m_Level;
	const int PlayerLevel = pPlayer->Account()->GetLevel();
	const bool showMessages = pPlayer->GetItem(itShowDetailGainMessages)->IsEquipped();

	// grinding gold
	{
		if(pPlayer->Account()->GetGold() < pPlayer->Account()->GetGoldCapacity())
		{
			int goldGain = calculate_loot_gain(MobLevel, 3);
			GS()->m_Multipliers.Apply(Multipliers::GOLD, goldGain);

			if(showMessages)
				GS()->Chat(ClientID, "You gained {} gold.", goldGain);

			pPlayer->Account()->AddGold(goldGain, true);
		}
	}

	// grinding materials
	{
		const int materialGain = calculate_loot_gain(MobLevel, 10);
		pPlayer->GetItem(itMaterial)->Add(materialGain);
	}

	// grinding experience
	{
		int expGain = calculate_exp_gain(PlayerLevel, MobLevel);
		const int expBonusDrop = maximum(expGain / 3, 1);
		GS()->m_Multipliers.Apply(Multipliers::EXPERIENCE, expGain);

		if(showMessages)
			GS()->Chat(ClientID, "You gained {} exp.", expGain);

		pPlayer->Account()->AddExperience(expGain);
		GS()->EntityManager()->DropPickup(m_pCharacter->m_Core.m_Pos, POWERUP_ARMOR, 0, expBonusDrop, (1 + rand() % 2), Force);
	}

	// drop item's
	{
		const auto ActiveLuckyDrop = pPlayer->GetTotalAttributeChance(AttributeIdentifier::LuckyDropItem).value_or(0.f);

		for(int i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
		{
			const int DropID = m_pMobInfo->m_aDropItem[i];
			const int DropValue = m_pMobInfo->m_aValueItem[i];

			if(DropID <= 0 || DropValue <= 0)
				continue;

			const float RandomDrop = clamp(m_pMobInfo->m_aRandomItem[i] + ActiveLuckyDrop, 0.0f, 100.0f);
			const vec2 ForceRandom = random_range_pos(Force, 4.f);

			CItem DropItem;
			DropItem.SetID(DropID);
			DropItem.SetValue(DropValue);

			// currency to inventory or by pickup
			if(!g_Config.m_SvDropsCurrencyFromMobs && DropItem.Info()->IsGroup(ItemGroup::Currency))
				pPlayer->GetItem(DropID)->Add(DropValue);
			else
				GS()->EntityManager()->RandomDropItem(m_pCharacter->m_Core.m_Pos, ClientID, RandomDrop, DropItem, ForceRandom);
		}
	}

	// skill points
	{
		float BaseChance = m_pMobInfo->m_Boss ? g_Config.m_SvSkillPointsDropChanceRareMob : g_Config.m_SvSkillPointsDropChanceMob;
		const int LevelDifference = PlayerLevel - MobLevel;

		// check leveling difference
		if(LevelDifference > 0)
		{
			const int AdjustmentSteps = LevelDifference / 5;
			BaseChance -= BaseChance * (0.10f * AdjustmentSteps);
		}
		BaseChance = maximum(1.0f, BaseChance);

		// try generate chance
		if(random_float(100.f) <= BaseChance)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(itSkillPoint);
			pPlayerItem->Add(1);
			GS()->Chat(ClientID, "Skill points increased. Now you have '{} SP'!", pPlayerItem->GetValue());
		}
	}

	// append defeat mob progress
	GS()->Core()->QuestManager()->TryAppendDefeatProgress(pPlayer, m_pMobInfo->m_BotID);
}

void CMobAI::OnTargetRules(float Radius)
{
	const auto* pTarget = GS()->GetPlayer(m_Target.GetCID(), false, true);
	auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidate)
	{
		const bool DamageDisabled = pCandidate->GetCharacter()->m_Core.m_DamageDisabled;

		if(pTarget)
		{
			const int CurrentTotalAttHP = pTarget->GetTotalAttributeValue(AttributeIdentifier::HP);
			const int CandidateTotalAttHP = pCandidate->GetTotalAttributeValue(AttributeIdentifier::HP);
			return !DamageDisabled && (CurrentTotalAttHP < CandidateTotalAttHP);
		}

		return !DamageDisabled;
	});

	if(!pPlayer)
	{
		pPlayer = SearchPlayerBotCondition(Radius, [&](CPlayerBot* pCandidate)
		{
			return m_pCharacter->IsAllowedPVP(pCandidate->GetCID());
		});
	}

	if(pPlayer)
	{
		m_Target.Set(pPlayer->GetCID(), 100);
	}
}

void CMobAI::Process()
{
	// handle behaviors
	bool Asleep = false;
	HandleBehaviors(&Asleep);
	if(Asleep)
		return;

	HandleSkillBehaviors();

	// update
	if(!m_BehaviorNeutral)
	{
		m_pCharacter->UpdateTarget(1000.f);

		if(const auto* pTargetChar = GS()->GetPlayerChar(m_Target.GetCID()))
		{
			m_pPlayer->m_TargetPos = pTargetChar->GetPos();
			m_pCharacter->Fire();
		}
		else
		{
			m_pPlayer->m_TargetPos.reset();
		}
	}

	m_pCharacter->SelectWeaponAtRandomInterval();
	m_pCharacter->Move();

	if(m_pMobInfo->m_Boss)
	{
		ShowHealth();
	}
}

void CMobAI::HandleBehaviors(bool* pbAsleep)
{
	// behavior poisonous
	if(m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_POISONOUS) &&
		(m_BehaviorPoisonedNextTick < Server()->Tick()))
	{
		const auto Theta = random_float(0.0f, 2.0f * pi);
		const auto Distance = 32.f + sqrt(random_float()) * 128.f;
		const auto RandomPosition = m_pCharacter->m_Core.m_Pos + vec2(Distance * cos(Theta), Distance * sin(Theta));

		// give poison effect
		if(!GS()->Collision()->CheckPoint(RandomPosition))
		{
			const auto vEntities = GS()->m_World.FindEntities(RandomPosition, 64.f, 8, CGameWorld::ENTTYPE_CHARACTER);
			for(auto* pEnt : vEntities)
			{
				auto* pTarget = dynamic_cast<CCharacter*>(pEnt);
				if(pTarget && pTarget->IsAllowedPVP(m_ClientID))
				{
					pTarget->GetPlayer()->m_Effects.Add(ECharacterEffect::POISON, Server()->TickSpeed() * 5);
				}
			}
			GS()->CreateDeath(RandomPosition, m_ClientID);
		}

		m_BehaviorPoisonedNextTick = Server()->Tick() + (5 + rand() % 40);
	}

	// behavior neutral
	m_BehaviorNeutral = false;
	if(m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_NEUTRAL) &&
		(m_pPlayer->m_aPlayerTick[LastDamage] + (Server()->TickSpeed() * 5)) < Server()->Tick())
	{
		m_BehaviorNeutral = true;
	}

	// behavior sleepy
	if(m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_SLEEPY) &&
		(m_pPlayer->m_aPlayerTick[LastDamage] + (Server()->TickSpeed() * 5)) < Server()->Tick())
	{
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			GS()->SendEmoticon(m_ClientID, EMOTICON_ZZZ);
			m_pCharacter->SetEmote(EMOTE_BLINK, 2, false);
		}
		(*pbAsleep) = true;
	}
}

int CMobAI::GetPercentValue(int MaxValue, int Percent)
{
	return maximum(1, translate_to_percent_rest(MaxValue, Percent));
}

void CMobAI::HandleSkillBehaviors()
{
	if(m_BehaviorSkillNextTick > Server()->Tick())
		return;

	const bool UsedSkill = TryUseBaseSkill() || TryUseHealerSkill() || TryUseTankSkill() || TryUseDpsSkill();
	const int CooldownSeconds = UsedSkill ? 2 : 1;
	m_BehaviorSkillNextTick = Server()->Tick() + (Server()->TickSpeed() * CooldownSeconds);
}

bool CMobAI::TryUseBaseSkill()
{
	if(!m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_BASE_SKILLS))
		return false;

	const int MaxHealth = m_pPlayer->GetMaxHealth();
	if(MaxHealth <= 0 || m_pCharacter->Health() > GetPercentValue(MaxHealth, 85))
		return false;

	const int ManaCost = GetPercentValue(m_pPlayer->GetMaxMana(), 8);
	if(!m_pCharacter->TryUseMana(ManaCost))
		return false;

	const int Heal = ManaCost + GetPercentValue(ManaCost, 60);
	m_pCharacter->IncreaseHealth(Heal);
	GS()->EntityManager()->Text(m_pCharacter->GetPos() + vec2(0, -96), 30, "MOB CURE");
	return true;
}

bool CMobAI::TryUseHealerSkill()
{
	if(!m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_HEALER_SKILLS))
		return false;

	const int SelfMaxHealth = m_pPlayer->GetMaxHealth();
	const bool NeedSelfHeal = SelfMaxHealth > 0 && m_pCharacter->Health() <= GetPercentValue(SelfMaxHealth, 70);
	bool NeedPartyHeal = false;

	const auto vEntities = GS()->m_World.FindEntities(m_pCharacter->GetPos(), 280.f, 32, CGameWorld::ENTTYPE_CHARACTER);
	for(auto* pEnt : vEntities)
	{
		auto* pNearby = dynamic_cast<CCharacter*>(pEnt);
		if(!pNearby || pNearby == m_pCharacter || !pNearby->GetPlayer() || !pNearby->GetPlayer()->IsBot())
			continue;

		const auto* pNearbyBot = dynamic_cast<CPlayerBot*>(pNearby->GetPlayer());
		if(!pNearbyBot || pNearbyBot->GetBotType() != TYPE_BOT_MOB)
			continue;

		if(pNearby->Health() <= GetPercentValue(pNearbyBot->GetMaxHealth(), 60))
		{
			NeedPartyHeal = true;
			break;
		}
	}

	if(!NeedSelfHeal && !NeedPartyHeal)
		return false;

	const int ManaCost = GetPercentValue(m_pPlayer->GetMaxMana(), 18);
	if(!m_pCharacter->TryUseMana(ManaCost))
		return false;

	const int Radius = 220;
	const int Lifetime = 3 * Server()->TickSpeed();
	const int HealPerTick = maximum(1, ManaCost / 2);
	GS()->EntityManager()->HealingAura(m_ClientID, m_pCharacter->GetPos(), Radius, Lifetime, HealPerTick);
	GS()->EntityManager()->Text(m_pCharacter->GetPos() + vec2(0, -96), 30, "MOB HEALER");
	return true;
}

bool CMobAI::TryUseTankSkill()
{
	if(!m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_TANK_SKILLS))
		return false;

	const int MaxMana = m_pPlayer->GetMaxMana();
	if(MaxMana <= 0)
		return false;

	const int MaxHealth = m_pPlayer->GetMaxHealth();
	const bool NeedProtection = MaxHealth > 0 && m_pCharacter->Health() <= GetPercentValue(MaxHealth, 65);
	if(NeedProtection)
	{
		const int ManaCost = GetPercentValue(MaxMana, 20);
		if(!m_pCharacter->TryUseMana(ManaCost))
			return false;

		GS()->EntityManager()->LastStand(m_ClientID, m_pCharacter->GetPos(), 192.f, 10);
		GS()->EntityManager()->Text(m_pCharacter->GetPos() + vec2(0, -96), 30, "MOB TANK");
		return true;
	}

	// Provoke-style aggro usage when there is no active target.
	if(m_Target.IsEmpty())
	{
		const int ManaCost = GetPercentValue(MaxMana, 12);
		if(!m_pCharacter->TryUseMana(ManaCost))
			return false;

		GS()->EntityManager()->Text(m_pCharacter->GetPos() + vec2(0, -96), 20, "PROVOKE (U NOOB)");
		return true;
	}

	return false;
}

bool CMobAI::TryUseDpsSkill()
{
	if(!m_pMobInfo->HasBehaviorFlag(MOBFLAG_BEHAVIOR_DPS_SKILLS))
		return false;

	const auto* pTargetChar = GS()->GetPlayerChar(m_Target.GetCID());
	if(!pTargetChar || distance(m_pCharacter->GetPos(), pTargetChar->GetPos()) > 512.f)
		return false;

	const int ManaCost = GetPercentValue(m_pPlayer->GetMaxMana(), 22);
	if(!m_pCharacter->TryUseMana(ManaCost))
		return false;

	if(rand() % 2 == 0)
	{
		const int DamageBonusPct = clamp(m_pMobInfo->m_Power / 8, 1, 30);
		const vec2 Dir = normalize(pTargetChar->GetPos() - m_pCharacter->GetPos());
		new CAttackTeleport(&GS()->m_World, m_pCharacter->GetPos(), m_pPlayer, DamageBonusPct, Dir);
		GS()->EntityManager()->Text(m_pCharacter->GetPos() + vec2(0, -96), 30, "MOB ATTACK TELEPORT");
		return true;
	}

	const float Radius = 180.0f;
	const int Lifetime = 2 * Server()->TickSpeed();
	const int Damage = maximum(1, m_pMobInfo->m_Power / 3);
	GS()->EntityManager()->FlameWall(m_ClientID, m_pCharacter->GetPos(), Radius, Lifetime, Damage, 0.45f);
	GS()->EntityManager()->Text(m_pCharacter->GetPos() + vec2(0, -96), 30, "MOB DPS");
	return true;
}

void CMobAI::ShowHealth() const
{
	const int BotID = m_pPlayer->GetBotID();
	const int Health = m_pPlayer->GetHealth();
	const int StartHealth = m_pPlayer->GetMaxHealth();
	const float Percent = translate_to_percent((float)StartHealth, (float)Health);
	std::string ProgressBar = mystd::string::progressBar(100, (int)Percent, 10, "\u25B0", "\u25B1");

	for(const auto& ClientID : m_pCharacter->GetListDmgPlayers())
	{
		if(GS()->GetPlayer(ClientID, true))
		{
			GS()->Broadcast(ClientID, BroadcastPriority::GamePriority, 100, "{} {}({}/{})",
				DataBotInfo::ms_aDataBot[BotID].m_aNameBot, ProgressBar.c_str(), Health, StartHealth);
		}
	}
}
