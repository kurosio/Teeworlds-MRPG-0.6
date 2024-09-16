/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tutorial.h"

#include <game/server/core/components/tutorial/tutorial_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/event/entitiy_group.h>
#include <game/server/entities/projectile.h>

// TODO character is not safety
class CTutorialScenario : public ScenarioBase, public IEventListener
{
	vec2 m_MovementPos {};
	void MovementTask(int Delay, const vec2& Pos, const char* pLockMsg, const char* pMoveMsg, bool LockView = true)
	{
		// is has lockview
		if(LockView)
		{
			FreezeMovements(true);
			AddStep(Delay).WhenStarted([this, Pos](auto*)
			{
				m_MovementPos = Pos;
			})
			.WhenActive([this, Delay, pLockMsg](auto*)
			{
				if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
				{
					GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
				}
				GetPlayer()->LockedView().ViewLock(m_MovementPos, true);
				SendBroadcast(Delay, pLockMsg);
			});
			FreezeMovements(false);
		}

		// movements
		AddStep().WhenStarted([this](auto*)
		{
			GetCharacter()->MovingDisable(false);
		})
		.WhenActive([this, pMoveMsg, Delay](auto*)
		{
			if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
				GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
			SendBroadcast(Delay, pMoveMsg);
		})
		.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
		{
			return distance(GetCharacter()->GetPos(), m_MovementPos) < 32.f;
		});
	}

	void FixedCam(int Delay, const vec2& Pos, const char* pStartMsg, const char* pEndMsg)
	{
		auto& step = AddStep(Delay);
		step.WhenActive([this, Delay, Pos, pStartMsg](auto*)
		{
			GetPlayer()->LockedView().ViewLock(Pos, true);
			SendBroadcast(Delay, pStartMsg);
		});

		if(pEndMsg != nullptr)
		{
			step.WhenFinished([this, Delay, pEndMsg](auto*)
			{
				SendBroadcast(Delay, pEndMsg);
			});
		}
	}

	void Message(int delay, const char* message)
	{
		AddStep(delay).WhenStarted([this, delay, message](auto*)
		{
			SendBroadcast(delay, message);
		});
	}

	void EmoteMessage(int delay, int emoteType, int emoticonType, const char* message)
	{
		AddStep(delay).WhenStarted([this, delay, emoteType, emoticonType, message](auto*)
		{
			GetCharacter()->SetEmote(emoteType, 1, false);
			GS()->SendEmoticon(GetClientID(), emoticonType);
			SendBroadcast(delay, message);
		});
	}

	void Teleport(int delay, const vec2& position, const char* message)
	{
		AddStep(delay).WhenStarted([this, position, message](auto*)
		{
			GetCharacter()->ChangePosition(position);
			SendBroadcast(200, message);
		});
	}

	void FreezeMovements(bool State)
	{
		AddStep().WhenStarted([this, State](auto*)
		{
			GetCharacter()->MovingDisable(State);
		});
	}

	std::vector <std::weak_ptr<CEntityGroup>> m_vShootingPracticles {};
	void ShootmarkersTask(const vec2& position, int Health)
	{
		// initialize group
		auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, GetClientID());
		groupPtr->SetConfig("health", Health);

		// initialize effect
		const auto pEntity = groupPtr->CreatePickup(position);
		pEntity->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
		{
			// health
			int& Health = pBase->GetGroup()->GetRefConfig("health", 0);
			if(Health <= 0)
			{
				pBase->GS()->CreateCyrcleExplosion(6, 64.f, pBase->GetPos(), -1, WEAPON_WORLD, 0);
				pBase->MarkForDestroy();
				return;
			}

			// destroy projectiles
			for(auto* pProj = (CProjectile*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile*)pProj->TypeNext())
			{
				if(distance(pBase->GetPos(), pProj->GetCurrentPos()) < 48.f)
				{
					Health -= pProj->GetDamage();
					pBase->GS()->CreateDamage(pBase->GetPos(), pBase->GetClientID(), pProj->GetDamage(), false, random_angle(), CmaskOne(pBase->GetClientID()));
					pProj->MarkForDestroy();
				}
			}
		});
		m_vShootingPracticles.emplace_back(groupPtr);
	}

	void Shootmarkers(const std::vector<std::pair<vec2, int>>& vShotmarkers, const char* pText)
	{
		for(const auto& [position, health] : vShotmarkers)
		{
			ShootmarkersTask(position, health);
			FixedCam(100, position, "You can shoot with the left mouse button.", "");
		}
		AddStep().WhenActive([this, pText](auto*)
		{
			GS()->Broadcast(GetClientID(), BroadcastPriority::VERY_IMPORTANT, 100, pText);
		}
		).CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
		{
			return IsShootingComplete();
		});
	}

	bool IsShootingComplete() const
	{
		return std::ranges::all_of(m_vShootingPracticles, [](const std::weak_ptr<CEntityGroup>& weakPtr)
		{
			return weakPtr.expired();
		});
	}

	void SetupScenario() override
	{
		// reset quest for tutorial
		GetPlayer()->GetQuest(1)->Reset();

		// welcome messages
		FreezeMovements(true);
		Message(100, "Welcome to the tutorial!");
		EmoteMessage(150, EMOTE_SURPRISE, EMOTICON_QUESTION, "You're wondering what this is and what I'm doing here?");
		Message(100, "I'm here to help you learn the basics of the game.");
		FreezeMovements(false);

		// move to point
		MovementTask(150, vec2(1749, 849), "Can you see the dot?", "Move to the dot!");

		// teleport
		FreezeMovements(true);
		Message(100, "Great job!");
		Teleport(100, vec2(640, 2752), "Let's test your hook skills!");
		FreezeMovements(false);

		// move to next point
		MovementTask(150, vec2(5580, 2800), "Try to get to the courts", "Move to the dot!");

		// teleport
		FreezeMovements(true);
		Message(100, "Great job!");
		Teleport(100, vec2(3072, 1408), "Now let's get to the basics of the gamemode!");
		FixedCam(150, vec2(3964, 1455), 
			"These are NPCs that you can usually interact with.\nBut there are also some that will only share information.", 
			"Try talking to him!");
		FreezeMovements(false);

		// first quest
		AddStep().CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
		{
			return GetPlayer()->GetQuest(1)->IsAccepted();
		});
		FreezeMovements(true);
		Message(50, "You accept your first quest pretty well!");
		AddStep(100).WhenActive([this](auto*)
		{
			GetPlayer()->LockedView().ViewLock(GetCharacter()->GetPos() - vec2(48, 0));
			SendBroadcast(100, "There is a movement indicator for the NPC next to you!");
		});
		FixedCam(100, vec2(3242, 1457), "As you can see it points in the direction of the NPC.", "Go up to him and talk to him!");
		FreezeMovements(false);
		AddStep().CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
		{
			return GetPlayer()->GetQuest(1)->IsCompleted();
		});

		// shootmark
		FreezeMovements(true);
		Message(100, "You already know how to do the quests.");
		Teleport(50, vec2(5280, 900), "Let's test your shooting skills.");
		Shootmarkers({
			{vec2(5480, 800), 5 }, 
			{vec2(5580, 750), 5 },
			{vec2(5380, 680), 5 },
		}, "Shot the markers!");
		Message(100, "You're doing pretty good!");
		FreezeMovements(false);
	}

	void SendBroadcast(int tick, const char* message) const
	{
		GS()->Broadcast(GetClientID(), BroadcastPriority::VERY_IMPORTANT, tick, message);
	}
};


CGameControllerTutorial::CGameControllerTutorial(class CGS* pGS)
	: IGameController(pGS)
{
	m_GameFlags = 0;
}

void CGameControllerTutorial::Tick()
{
	IGameController::Tick();

	// handle tutorial world
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(GS()->IsPlayerInWorld(i))
		{
			if(CPlayer* pPlayer = GS()->GetPlayer(i, true, true); pPlayer)
			{
				CTutorialManager* pTutorialManager = GS()->Core()->TutorialManager();
				pTutorialManager->ProcessTutorialStep(pPlayer);

				if(pPlayer->m_TutorialStep >= pTutorialManager->GetTutorialCount() && !pPlayer->GetItem(itAdventurersBadge)->HasItem())
					pPlayer->GetItem(itAdventurersBadge)->Add(1);
			}
		}
	}
}

bool CGameControllerTutorial::OnCharacterSpawn(CCharacter* pChr)
{
	pChr->GetPlayer()->Scenarios().Start(std::make_unique<CTutorialScenario>());

	return IGameController::OnCharacterSpawn(pChr);
}

void CGameControllerTutorial::Snap()
{
	// vanilla snap
	CNetObj_GameInfo* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	pGameInfoObj->m_RoundStartTick = 0;
	pGameInfoObj->m_WarmupTimer = 0;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	// ddnet snap
	CNetObj_GameInfoEx* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}
