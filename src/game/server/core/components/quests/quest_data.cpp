/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_manager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

void CQuestDescription::CReward::ApplyReward(CPlayer* pPlayer) const
{
	pPlayer->Account()->AddExperience(m_Experience);
	pPlayer->Account()->AddGold(m_Gold);
}

int CQuestDescription::GetChainLength() const
{
	int Value = 1; // include current quest

	auto* pPrevious = GetPreviousQuest();
	while(pPrevious != nullptr)
	{
		pPrevious = pPrevious->GetPreviousQuest();
		Value++;
	}

	auto* pNext = GetNextQuest();
	while(pNext != nullptr)
	{
		pNext = pNext->GetNextQuest();
		Value++;
	}

	return Value;
}

int CQuestDescription::GetCurrentChainPos() const
{
	int Value = 1; // include current quest

	auto* pPrevious = GetPreviousQuest();
	while(pPrevious != nullptr)
	{
		pPrevious = pPrevious->GetPreviousQuest();
		Value++;
	}

	return Value;
}

CQuestDescription* CQuestDescription::GetNextQuest() const
{
	if(m_NextQuestID && m_pData.contains(*m_NextQuestID))
		return m_pData.at(*m_NextQuestID);

	return nullptr;
}

CQuestDescription* CQuestDescription::GetPreviousQuest() const
{
	if(m_PreviousQuestID && m_pData.contains(*m_PreviousQuestID))
		return m_pData.at(*m_PreviousQuestID);

	return nullptr;
}

bool CQuestDescription::HasObjectives(int Step)
{
	return m_vObjectives.contains(Step) && m_vObjectives[Step].size() > 0;
}

void CQuestDescription::PreparePlayerObjectives(int Step, int ClientID, std::deque<CQuestStep*>& pElem)
{
	ResetPlayerObjectives(pElem);
	for(const auto& StepDesc : m_vObjectives[Step])
		pElem.emplace_back(new CQuestStep(ClientID, StepDesc.m_Bot));

}

void CQuestDescription::ResetPlayerObjectives(std::deque<CQuestStep*>& pElem)
{
	if(!pElem.empty())
	{
		for(auto* pPtr : pElem)
		{
			pPtr->MarkForDestroy();
			pPtr->Update();
		}

		for(auto*& pPtr : pElem)
			delete pPtr;

		pElem.clear();
	}
}

CGS* CPlayerQuest::GS() const
{
	return dynamic_cast<CGS*>(Instance::GameServerPlayer(m_ClientID));
}

CPlayer* CPlayerQuest::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CQuestDescription* CPlayerQuest::Info() const
{
	return CQuestDescription::Data()[m_ID];
}

CPlayerQuest::~CPlayerQuest()
{
	Info()->ResetPlayerObjectives(m_vObjectives);
}

bool CPlayerQuest::HasUnfinishedObjectives() const
{
	return std::ranges::any_of(m_vObjectives, [](CQuestStep* pPtr)
	{
		return !pPtr->m_StepComplete && pPtr->m_Bot.m_HasAction;
	});
}

bool CPlayerQuest::Accept()
{
	// check valid player and quest state
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return false;

	// check acceptable state
	if(m_State != QuestState::NoAccepted)
		return false;

	// initialize
	SetNewState(QuestState::Accepted);
	m_Step = 1;
	m_Datafile.Create();
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('{}', '{}', '{}')", m_ID, pPlayer->Account()->GetID(), (int)m_State);

	// message about quest accept state
	if(Info()->HasFlag(QUEST_FLAG_TYPE_MAIN))
		GS()->Chat(m_ClientID, "Main quest: '{}' accepted!", Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_DAILY))
		GS()->Chat(m_ClientID, "Daily quest: '{}' accepted!", Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_WEEKLY))
		GS()->Chat(m_ClientID, "Weekly quest: '{}' accepted!", Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_REPEATABLE))
		GS()->Chat(m_ClientID, "Repeatable quest: '{}' accepted!", Info()->GetName());
	else
		GS()->Chat(m_ClientID, "Side quest: '{}' accepted!", Info()->GetName());

	// accepted effects
	GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 100, "Quest Accepted");
	GS()->CreatePlayerSound(m_ClientID, SOUND_GAME_ACCEPT);
	return true;
}

void CPlayerQuest::Refuse()
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	if(m_State != QuestState::Accepted)
		return;

	if(Info()->HasFlag(QUEST_FLAG_CANT_REFUSE))
	{
		GS()->Chat(m_ClientID, "This quest cannot be refused!");
		return;
	}

	Reset();
	GS()->Chat(m_ClientID, "You refused the quest '{}'.", Info()->GetName());
}

void CPlayerQuest::Reset()
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	Info()->ResetPlayerObjectives(m_vObjectives);
	m_Step = 1;
	SetNewState(QuestState::NoAccepted);
	m_Datafile.Delete();
	Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID = '{}' AND UserID = '{}'", m_ID, pPlayer->Account()->GetID());
}

void CPlayerQuest::UpdateStepProgress()
{
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer)
		return;

	if(HasUnfinishedObjectives())
		return;

	// update step progress
	Info()->ResetPlayerObjectives(m_vObjectives);
	m_Step++;
	if(Info()->HasObjectives(m_Step))
	{
		m_Datafile.Create();
		return;
	}

	// apply reward for player
	Info()->Reward().ApplyReward(pPlayer);

	// completion effects
	GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 100, "Quest Complete");
	GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -70), 30, "QUEST COMPLETE");
	GS()->CreatePlayerSound(m_ClientID, SOUND_GAME_DONE);

	// repeatable quest can't got finished state
	bool CanGotActivityPoints = !Info()->HasFlag(QUEST_FLAG_NO_ACTIVITY_POINT);
	if(Info()->HasFlag(QUEST_FLAG_TYPE_REPEATABLE))
	{
		if(CanGotActivityPoints)
			pPlayer->GetItem(itActivityCoin)->Add(g_Config.m_SvRepeatableActivityCoin);

		GS()->Chat(-1, "{} completed repeatable quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
		Reset();
		return;
	}

	// daily quest
	int ActivityPoints = 0;
	if(Info()->HasFlag(QUEST_FLAG_TYPE_DAILY))
	{
		ActivityPoints = g_Config.m_SvDailyActivityCoin;
		GS()->Chat(-1, "{} completed daily quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	}
	// weekly quest
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_WEEKLY))
	{
		ActivityPoints = g_Config.m_SvWeeklyActivityCoin;
		GS()->Chat(-1, "{} completed weekly quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	}
	// main quest
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_MAIN))
	{
		ActivityPoints = g_Config.m_SvMainQuestActivityCoin;
		GS()->Chat(-1, "{} completed main quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	}
	// side quest
	else
	{
		ActivityPoints = g_Config.m_SvSideQuestActivityCoin;
		GS()->Chat(-1, "{} completed side quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	}

	// add activity points
	if(CanGotActivityPoints && ActivityPoints > 0)
		pPlayer->GetItem(itActivityCoin)->Add(ActivityPoints);

	// update quest state in database
	SetNewState(QuestState::Finished);
	m_Datafile.Delete();
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '{}' WHERE QuestID = '{}' AND UserID = '{}'", (int)m_State, m_ID, pPlayer->Account()->GetID());

	// save player stats and accept next story quest
	GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	GS()->Core()->QuestManager()->TryAcceptNextQuestChain(pPlayer, m_ID);
}

void CPlayerQuest::Update()
{
	for(const auto& pStep : m_vObjectives)
		pStep->Update();

	UpdateStepProgress();
}

CQuestStep* CPlayerQuest::GetStepByMob(int MobID)
{
	auto iter = std::ranges::find_if(m_vObjectives, [MobID](const CQuestStep* pPtr)
	{
		return pPtr->m_Bot.m_ID == MobID;
	});
    return iter != m_vObjectives.end() ? *iter : nullptr;
}

void CPlayerQuest::SetNewState(QuestState State)
{
	if(m_State != State)
	{
		m_State = State;
		g_EventListenerManager.Notify<IEventListener::PlayerQuestChangeState>(GetPlayer(), this, State);
	}
}