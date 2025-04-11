#include "game/server/core/components/Bots/BotData.h"
#include "move_action.h"
#include "dir_navigator.h"

#include <game/server/gamecontext.h>
#include <game/server/entity_manager.h>
#include "game/server/core/components/quests/quest_manager.h"

CEntityQuestAction::CEntityQuestAction(CGameWorld* pGameWorld, int ClientID, int MoveToIndex, CQuestStep* pStep)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_MOVE_TO_POINT, {}, 32.f, ClientID), m_MoveToIndex(MoveToIndex)
{
	m_pStep = pStep;
	if(m_pStep)
		Initialize();
	else
		MarkForDestroy();

	AddSnappingGroupIds(VISUAL_GROUP, VISTUAL_IDS_NUM);
	GameWorld()->InsertEntity(this);
}

CEntityQuestAction::~CEntityQuestAction()
{
	// clear data
	if(m_pEntDirNavigator)
	{
		m_pEntDirNavigator->MarkForDestroy();
		m_pEntDirNavigator = nullptr;
	}

	// disable cooldown on erase action
	auto* pPlayer = GetOwner();
	if(pPlayer && pPlayer->m_Cooldown.IsActive())
		pPlayer->m_Cooldown.Reset();

	// mark for destroy is non active clients
	auto* pDefeatBotPlayer = GetDefeatPlayerBot();
	if(pDefeatBotPlayer)
	{
		auto& QuestBotInfo = pDefeatBotPlayer->GetQuestBotMobInfo();
		QuestBotInfo.m_ActiveForClient[m_ClientID] = false;

		bool MarkForDestroy = std::ranges::none_of(QuestBotInfo.m_ActiveForClient, [](bool active) { return active; });
		if(MarkForDestroy)
		{
			dbg_msg(PRINT_QUEST_PREFIX, "Marked for destroy objective quest mob!");
			pDefeatBotPlayer->MarkForDestroy();
		}
	}
}

void CEntityQuestAction::Initialize()
{
	const auto* pTaskData = GetTaskMoveTo();
	m_Pos = pTaskData->m_Position;
	m_Radius = pTaskData->m_TypeFlags & QuestBotInfo::TaskAction::Types::TFDEFEAT_MOB ? 400.f : 32.f;

	// try create defeat mob
	if(pTaskData->IsHasDefeatMob())
	{
		CPlayerBot* pPlayerBot = nullptr;
		for(int c = MAX_PLAYERS; c < MAX_CLIENTS; ++c)
		{
			auto* pPotentialBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(c));
			if(pPotentialBot && pPotentialBot->GetQuestBotMobInfo().m_QuestID == m_pStep->m_Bot.m_QuestID &&
				pPotentialBot->GetQuestBotMobInfo().m_QuestStep == m_pStep->m_Bot.m_StepPos &&
				pPotentialBot->GetQuestBotMobInfo().m_MoveToStep == m_MoveToIndex)
			{
				pPlayerBot = pPotentialBot;
				break;
			}
		}

		if(!pPlayerBot)
		{
			const auto& DefeatMobInfo = pTaskData->m_DefeatMobInfo;
			const int MobClientID = GS()->CreateBot(TYPE_BOT_QUEST_MOB, DefeatMobInfo.m_BotID, -1);
			pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(MobClientID));

			CQuestBotMobInfo data;
			data.m_QuestID = m_pStep->m_Bot.m_QuestID;
			data.m_QuestStep = m_pStep->m_Bot.m_StepPos;
			data.m_MoveToStep = m_MoveToIndex;
			data.m_AttributePower = DefeatMobInfo.m_AttributePower;
			data.m_WorldID = DefeatMobInfo.m_WorldID;
			data.m_Position = pTaskData->m_Position;
			pPlayerBot->InitQuestBotMobInfo(data);
			dbg_msg(PRINT_QUEST_PREFIX, "Creating a objective quest mob!");
		}

		pPlayerBot->GetQuestBotMobInfo().m_ActiveForClient[m_ClientID] = true;
		m_DefeatBotCID = pPlayerBot->GetCID();
	}

	// initialize support entities
	if(pTaskData->m_TypeFlags & QuestBotInfo::TaskAction::Types::TFDEFEAT_MOB)
	{
		constexpr auto Radius = 400.f;
		m_pEntDirNavigator = new CEntityDirNavigator(&GS()->m_World, POWERUP_ARMOR, 0, false, m_ClientID, Radius, m_Pos, pTaskData->m_WorldID);
		GS()->EntityManager()->LaserOrbite(this, (int)(Radius / 50.f), LaserOrbiteType::InsideOrbite, 0.f, Radius, LASERTYPE_FREEZE, CmaskOne(m_ClientID));
	}
	else if(!pTaskData->m_Navigator)
	{
		const auto Radius = 1000.f + random_float(2000.f);
		m_pEntDirNavigator = new CEntityDirNavigator(&GS()->m_World, POWERUP_ARMOR, 0, false, m_ClientID, Radius, m_Pos, pTaskData->m_WorldID);
		GS()->EntityManager()->LaserOrbite(this, (int)(Radius / 50.f), LaserOrbiteType::InsideOrbiteRandom, 0.f, Radius, LASERTYPE_FREEZE, CmaskOne(m_ClientID));

	}
	else
	{
		m_pEntDirNavigator = new CEntityDirNavigator(&GS()->m_World, POWERUP_ARMOR, 0, false, m_ClientID, 0.f, m_Pos, pTaskData->m_WorldID);
	}

	if(m_pEntDirNavigator && m_pEntDirNavigator->IsMarkedForDestroy())
		m_pEntDirNavigator = nullptr;
}

void CEntityQuestAction::Tick()
{
	auto* pPlayer = GetOwner();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	// update step status
	const bool IsComplected = m_pStep->m_aMoveActionProgress[m_MoveToIndex];
	if(IsComplected)
	{
		if(!m_pStep->TryAutoFinish())
			m_pStep->Update();
		return;
	}

	// check distance
	if(distance(pPlayer->GetCharacter()->GetPos(), m_Pos) > m_Radius)
		return;

	// handlers
	const auto* pTaskData = GetTaskMoveTo();
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

	CPlayer* pPlayer = GetOwner();
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
	auto* pPlayer = GetOwner();
	auto* pQuest = GetPlayerQuest();
	auto* pQuestStep = m_pStep;
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
		GS()->Chat(m_ClientID, pTaskData->m_CompletionText.c_str());

	// Set the complete flag to true
	pQuestStep->m_aMoveActionProgress[m_MoveToIndex] = true;
	pQuest->Datafile().Save();
	GS()->CreateDeath(m_Pos, m_ClientID);
}

CPlayerBot* CEntityQuestAction::GetDefeatPlayerBot() const
{
	if(m_DefeatBotCID.has_value())
		return dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_DefeatBotCID.value()));
	return nullptr;
}

CPlayerQuest* CEntityQuestAction::GetPlayerQuest() const
{
	CPlayer* pPlayer = GetOwner();
	if(!pPlayer)
		return nullptr;

	return pPlayer->GetQuest(m_pStep->m_Bot.m_QuestID);
}

QuestBotInfo::TaskAction* CEntityQuestAction::GetTaskMoveTo() const
{
	return &m_pStep->m_Bot.m_vRequiredMoveAction[m_MoveToIndex];
}

void CEntityQuestAction::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !GetOwnerChar())
		return;

	auto& visualIdsGroup = GetSnappingGroupIds(VISUAL_GROUP);
	for(auto& id : visualIdsGroup)
	{
		vec2 randomRangePos = random_range_pos(m_Pos, m_Radius);
		GS()->SnapProjectile(SnappingClient, id, randomRangePos, { }, Server()->Tick() - 3, WEAPON_HAMMER, m_ClientID);
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
			return PressedFire() && distance(GetOwner()->GetCharacter()->GetMousePos(), Position) < 48.f;
		});
	}

	// defeat bot flag
	else if(TypeFlags & QuestBotInfo::TaskAction::Types::TFDEFEAT_MOB)
	{
		Handler([this]
		{
			CPlayerBot* pDefeatMobPlayer = GetDefeatPlayerBot();
			return !pDefeatMobPlayer || !pDefeatMobPlayer->GetQuestBotMobInfo().m_ActiveForClient[m_ClientID];
		});
	}
}

void CEntityQuestAction::HandleBroadcastInformation(const QuestBotInfo::TaskAction* pTaskData) const
{
	CPlayer* pPlayer = GetOwner();
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
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequireItem);
		strBuffer += fmt_localize(m_ClientID, "- Required: {} x{}({})\n",
			pPlayerItem->Info()->GetName(), pRequireItem.GetValue(), pPlayerItem->GetValue());
	}

	if(pPickupItem.IsValid())
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pPickupItem);
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