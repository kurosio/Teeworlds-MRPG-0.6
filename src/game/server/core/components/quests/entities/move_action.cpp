#include "game/server/core/components/Bots/BotData.h"
#include "move_action.h"

#include <game/server/gamecontext.h>
#include "game/server/core/components/quests/quest_manager.h"

constexpr unsigned int s_Particles = 4;

CEntityQuestAction::CEntityQuestAction(CGameWorld* pGameWorld, int ClientID, int MoveToIndex,
	const std::weak_ptr<CQuestStep>& pStep, bool MoveToAutoCompletesStep, std::optional<int> optDefeatBotCID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVE_TO_POINT, {}, 32.f, ClientID), m_MoveToIndex(MoveToIndex)
{
	// initialize base
	m_pStep = pStep;
	m_optDefeatBotCID = optDefeatBotCID;
	m_MoveToAutoCompletesStep = MoveToAutoCompletesStep;
	if(const auto* pTaskData = GetTaskMoveTo())
	{
		m_Pos = pTaskData->m_Position;
		m_Radius = (pTaskData->m_TypeFlags & QuestBotInfo::TaskAction::Types::TFDEFEAT_MOB) ? 400.f : 32.f;
	}
	GameWorld()->InsertEntity(this);

	// initialize snap ids
	m_IDs.set_size(s_Particles);
	for(int i = 0; i < m_IDs.size(); i++)
		m_IDs[i] = Server()->SnapNewID();
}

CEntityQuestAction::~CEntityQuestAction()
{
	const auto& pStep = GetQuestStep();
	if(pStep)
	{
		std::erase_if(pStep->m_vpEntitiesAction, [this](const auto* pEntPtr)
		{
			return pEntPtr == this;
		});
	}

	// update quest player progress
	CPlayer* pPlayer = GetPlayer();
	if(pPlayer && pStep)
	{
		// try auto finish step
		if(m_MoveToAutoCompletesStep)
		{
			const bool LastElement = (pStep->GetCompletedMoveActionCount() == pStep->GetMoveActionNum());
			if(LastElement && pStep->IsComplete())
			{
				pStep->Finish();
			}
		}

		GS()->Core()->QuestManager()->Update(pPlayer);
	}

	// mark whether or not we need to remove the mob from the game
	CPlayerBot* pDefeatBotPlayer = GetDefeatPlayerBot();
	if(pDefeatBotPlayer)
	{
		auto& QuestBotInfo = pDefeatBotPlayer->GetQuestBotMobInfo();
		QuestBotInfo.m_ActiveForClient[m_ClientID] = false;
		QuestBotInfo.m_CompleteClient[m_ClientID] = false;

		bool ClearDefeatMobPlayer = true;
		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			if(QuestBotInfo.m_ActiveForClient[i])
			{
				ClearDefeatMobPlayer = false;
				break;
			}
		}

		if(ClearDefeatMobPlayer)
		{
			GS()->DestroyPlayer(pDefeatBotPlayer->GetCID());
			dbg_msg(PRINT_QUEST_PREFIX, "Deleted questing mob");
		}
	}

	// free ids
	for(int i = 0; i < m_IDs.size(); i++)
		Server()->SnapFreeID(m_IDs[i]);
}

void CEntityQuestAction::Tick()
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(!GetPlayerQuest() || !GetQuestStep())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	const auto* pTaskData = GetTaskMoveTo();
	if(!pTaskData)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	const bool IsComplected = GetQuestStep()->m_aMoveActionProgress[m_MoveToIndex];
	if(IsComplected)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// check distance
	if(distance(pPlayer->GetCharacter()->GetPos(), m_Pos) > m_Radius)
		return;

	// handlers
	HandleTaskType(pTaskData);
	HandleBroadcastInformation(pTaskData);
}

bool CEntityQuestAction::PressedFire() const
{
	return Server()->Input()->IsKeyClicked(m_ClientID, KEY_EVENT_FIRE_HAMMER);
}

void CEntityQuestAction::Handler(const std::function<bool()>& pCallbackSuccesful)
{
	if(!pCallbackSuccesful())
		return;

	CPlayer* pPlayer = GetPlayer();
	const auto* pTaskData = GetTaskMoveTo();
	if(pTaskData->m_Cooldown > 0)
	{
		pPlayer->m_Cooldown.Start(pTaskData->m_Cooldown, pTaskData->m_TaskName, std::bind(&CEntityQuestAction::TryFinish, this));
	}
	else
	{
		TryFinish();
	}
}

void CEntityQuestAction::TryFinish()
{
	auto* pPlayer = GetPlayer();
	auto* pQuest = GetPlayerQuest();
	auto* pQuestStep = GetQuestStep();
	const auto* pTaskData = GetTaskMoveTo();

	// required item
	if(pTaskData->m_TypeFlags & QuestBotInfo::TaskAction::Types::TFREQUIRED_ITEM && pTaskData->m_RequiredItem.IsValid())
	{
		const auto ItemID = pTaskData->m_RequiredItem.GetID();
		const auto RequiredValue = pTaskData->m_RequiredItem.GetValue();

		if(!pPlayer->Account()->SpendCurrency(RequiredValue, ItemID))
			return;

		auto* pPlayerItem = pPlayer->GetItem(ItemID);
		GS()->Chat(m_ClientID, "You've used on the point '{} x{}'.", pPlayerItem->Info()->GetName(), RequiredValue);
	}

	// pickup item
	if(pTaskData->m_TypeFlags & QuestBotInfo::TaskAction::Types::TFPICKUP_ITEM && pTaskData->m_PickupItem.IsValid())
	{
		const auto ItemID = pTaskData->m_PickupItem.GetID();
		const auto PickupValue = pTaskData->m_PickupItem.GetValue();

		auto* pPlayerItem = pPlayer->GetItem(ItemID);
		pPlayerItem->Add(PickupValue);
		GS()->Chat(m_ClientID, "You've picked up '{} x{}'.", pPlayerItem->Info()->GetName(), PickupValue);
	}

	// Completion text
	if(!pTaskData->m_CompletionText.empty())
	{
		GS()->Chat(m_ClientID, pTaskData->m_CompletionText.c_str());
	}

	// Set the complete flag to true
	pQuestStep->m_aMoveActionProgress[m_MoveToIndex] = true;
	pQuest->Datafile().Save();

	// Create a death entity at the current position and destroy this entity
	GS()->CreateDeath(m_Pos, m_ClientID);
	GameWorld()->DestroyEntity(this);
}

CPlayer* CEntityQuestAction::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CPlayerBot* CEntityQuestAction::GetDefeatPlayerBot() const
{
	if(m_optDefeatBotCID.has_value())
		return dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_optDefeatBotCID.value()));
	return nullptr;
}

CPlayerQuest* CEntityQuestAction::GetPlayerQuest() const
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return nullptr;

	if(const auto pStep = m_pStep.lock())
	{
		return pPlayer->GetQuest(pStep->m_Bot.m_QuestID);
	}
	return nullptr;
}

CQuestStep* CEntityQuestAction::GetQuestStep() const
{
	if(const auto pStep = m_pStep.lock())
	{
		return pStep.get();
	}
	return nullptr;
}

QuestBotInfo::TaskAction* CEntityQuestAction::GetTaskMoveTo() const
{
	if(const auto pStep = m_pStep.lock())
	{
		return &pStep->m_Bot.m_vRequiredMoveAction[m_MoveToIndex];
	}
	return nullptr;
}

void CEntityQuestAction::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	for(int i = 0; i < m_IDs.size(); i++)
	{
		vec2 randomRangePos = random_range_pos(m_Pos, m_Radius);
		GS()->SnapProjectile(SnappingClient, m_IDs[i], randomRangePos, { }, Server()->Tick() - 3, WEAPON_HAMMER, m_ClientID);
	}
}

void CEntityQuestAction::HandleTaskType(const QuestBotInfo::TaskAction* pTaskData)
{
	const unsigned TypeFlags = pTaskData->m_TypeFlags;

	// move flag
	if(TypeFlags & QuestBotInfo::TaskAction::Types::TFMOVING)
	{
		Handler([] { return true; });
	}

	// move press flag
	else if(TypeFlags & QuestBotInfo::TaskAction::Types::TFMOVING_PRESS)
	{
		Handler([this] { return PressedFire(); });
	}

	// move follow press flag
	else if(TypeFlags & QuestBotInfo::TaskAction::Types::TFMOVING_FOLLOW_PRESS)
	{
		const auto Position = pTaskData->m_Interaction.m_Position;
		if(Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
			GS()->CreateHammerHit(Position, CmaskOne(m_ClientID));

		Handler([this, Position]
		{
			return PressedFire() && distance(GetPlayer()->GetCharacter()->GetMousePos(), Position) < 48.f;
		});
	}

	// defeat bot flag
	else if(TypeFlags & QuestBotInfo::TaskAction::Types::TFDEFEAT_MOB)
	{
		Handler([this]
		{
			CPlayerBot* pDefeatMobPlayer = GetDefeatPlayerBot();
			if(!pDefeatMobPlayer)
				return true;
			return pDefeatMobPlayer->GetQuestBotMobInfo().m_CompleteClient[m_ClientID];
		});
	}
}

void CEntityQuestAction::HandleBroadcastInformation(const QuestBotInfo::TaskAction* pTaskData) const
{
	CPlayer* pPlayer = GetPlayer();
	const auto& pPickupItem = pTaskData->m_PickupItem;
	const auto& pRequireItem = pTaskData->m_RequiredItem;
	const auto Type = pTaskData->m_TypeFlags;

	// skip defeat mob
	if(Type & QuestBotInfo::TaskAction::Types::TFDEFEAT_MOB)
		return;

	// formating
	std::string strBuffer;
	if(pRequireItem.IsValid())
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequireItem.GetID());
		strBuffer += fmt_localize(m_ClientID, "- Required: {} x{}({})\n",
			pPlayerItem->Info()->GetName(), pRequireItem.GetValue(), pPlayerItem->GetValue());
	}

	if(pPickupItem.IsValid())
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPickupItem.GetID());
		strBuffer += fmt_localize(m_ClientID, "- Pick up: {} x{}({})\n",
			pPlayerItem->Info()->GetName(), pPickupItem.GetValue(), pPlayerItem->GetValue());
	}

	// send broadcast
	if(Type & QuestBotInfo::TaskAction::Types::TFMOVING_PRESS)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::MainInformation, 10,
			"Click with the hammer to interact.\n{}", strBuffer.c_str());
	}
	else if(Type & QuestBotInfo::TaskAction::Types::TFMOVING_FOLLOW_PRESS)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::MainInformation, 10,
			"Click the highlighted area with the hammer to interact.\n{}", strBuffer.c_str());
	}
}