/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_manager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Dungeons/DungeonManager.h>

void CQuestDescription::CReward::ApplyReward(CPlayer* pPlayer) const
{
	pPlayer->Account()->AddExperience(m_Experience);
	pPlayer->Account()->AddGold(m_Gold);
}

CQuestDescription* CQuestDescription::GetNextQuest() const
{
	if(m_NextQuestID.has_value())
	{
		if(const auto it = m_pData.find(m_NextQuestID.value()); it != m_pData.end())
			return it->second;
	}
	return nullptr;
}

CQuestDescription* CQuestDescription::GetPreviousQuest() const
{
	if(m_PreviousQuestID.has_value())
	{
		if(const auto it = m_pData.find(m_PreviousQuestID.value()); it != m_pData.end())
			return it->second;
	}
	return nullptr;
}

bool CQuestDescription::HasObjectives(int Step)
{
	return m_vObjectives.contains(Step) && m_vObjectives[Step].size() > 0;
}

void CQuestDescription::PreparePlayerObjectives(int Step, int ClientID, std::deque<std::shared_ptr<CQuestStep>>& pElem)
{
	for(const auto& Step : m_vObjectives[Step])
	{
		pElem.emplace_back(std::make_shared<CQuestStep>(ClientID, Step.m_Bot));
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
	m_vObjectives.clear();
}

bool CPlayerQuest::HasUnfinishedObjectives() const
{
	return std::ranges::any_of(m_vObjectives, [](const std::shared_ptr<CQuestStep>& pPtr) 
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
	m_Step = 1;
	m_State = QuestState::Accepted;
	m_Datafile.Create();
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('{}', '{}', '{}')", m_ID, pPlayer->Account()->GetID(), (int)m_State);

	// message about quest accept state
	if(Info()->HasFlag(QUEST_FLAG_TYPE_REPEATABLE))
		GS()->Chat(m_ClientID, "Repeatable quest: '{}' accepted!", Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_DAILY))
		GS()->Chat(m_ClientID, "Daily quest: '{}' accepted!", Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_WEEKLY))
		GS()->Chat(m_ClientID, "Weekly quest: '{}' accepted!", Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_MAIN))
		GS()->Chat(m_ClientID, "Main quest: '{}' accepted!", Info()->GetName());
	else
		GS()->Chat(m_ClientID, "Side quest: '{}' accepted!", Info()->GetName());

	// accepted effects
	GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 100, "Quest Accepted");
	GS()->CreatePlayerSound(m_ClientID, SOUND_CTF_GRAB_EN);
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

	m_Step = 1;
	m_State = QuestState::NoAccepted;
	for(const auto& pStep : m_vObjectives)
	{
		pStep->Update();
	}
	m_vObjectives.clear();
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
	m_Step++;
	m_vObjectives.clear();

	if(Info()->HasObjectives(m_Step))
	{
		m_Datafile.Create();
		Update();
		return;
	}
	
	// apply reward for player
	Info()->Reward().ApplyReward(pPlayer);

	// completion effects
	GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, 100, "Quest Complete");
	GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -70), 30, "QUEST COMPLETE");
	GS()->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);

	// handle quest by flags
	if(Info()->HasFlag(QUEST_FLAG_TYPE_REPEATABLE))
	{
		GS()->Chat(-1, "{} completed repeatable quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
		Refuse();
		return;
	}
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_DAILY))
	{
		pPlayer->GetItem(itAlliedSeals)->Add(g_Config.m_SvDailyQuestSealReward);
		GS()->Chat(-1, "{} completed daily quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	}
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_WEEKLY))
		GS()->Chat(-1, "{} completed weekly quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	else if(Info()->HasFlag(QUEST_FLAG_TYPE_MAIN))
		GS()->Chat(-1, "{} completed main quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
	else
		GS()->Chat(-1, "{} completed side quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());

	// update quest state in database
	m_State = QuestState::Finished;
	m_Datafile.Delete();
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '{}' WHERE QuestID = '{}' AND UserID = '{}'", (int)m_State, m_ID, pPlayer->Account()->GetID());

	// save player stats and accept next story quest
	GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	GS()->Core()->QuestManager()->TryAcceptNextQuestChain(pPlayer, m_ID);
	GS()->Core()->DungeonManager()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);
}

void CPlayerQuest::Update()
{
	for(const auto& pStep : m_vObjectives)
	{
		pStep->Update();
	}
	UpdateStepProgress();
}

CQuestStep* CPlayerQuest::GetStepByMob(int MobID)
{
	const auto iter = std::ranges::find_if(m_vObjectives, [MobID](const std::shared_ptr<CQuestStep>& Step) 
	{
		return Step->m_Bot.m_ID == MobID;
	});
    return iter != m_vObjectives.end() ? iter->get() : nullptr;
}