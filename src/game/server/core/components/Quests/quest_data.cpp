/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Dungeons/DungeonManager.h>

void CQuestReward::ApplyReward(CPlayer* pPlayer) const
{
	pPlayer->Account()->AddExperience(m_Experience);
	pPlayer->Account()->AddGold(m_Gold);
}

int CQuestDescription::GetStoryCurrentPos() const
{
	return (int)std::ranges::count_if(Data(), [this](std::pair< const int, CQuestDescription*>& pItem)
	{
		return pItem.second->m_Story == m_Story && m_ID >= pItem.first;
	});
}

int CQuestDescription::GetStoryQuestsNum() const
{
	return (int)std::ranges::count_if(Data(), [this](std::pair< const int, CQuestDescription*>& pItem)
	{
		return pItem.second->m_Story == m_Story;
	});
}

void CQuestDescription::PreparePlayerSteps(int StepPos, int ClientID, std::deque<CQuestStep>* pElem)
{
	// clear old steps
	if(!(*pElem).empty())
		(*pElem).clear();

	// prepare new steps
	for(const auto& Step : m_vSteps[StepPos])
	{
		CQuestStep Base;
		Base.m_ClientID = ClientID;
		Base.m_Bot = Step.m_Bot;
		Base.m_StepComplete = false;
		Base.m_ClientQuitting = false;
		Base.m_aMobProgress.clear();
		(*pElem).push_back(std::move(Base));
	}
}

CGS* CPlayerQuest::GS() const { return dynamic_cast<CGS*>(Instance::GameServerPlayer(m_ClientID)); }
CPlayer* CPlayerQuest::GetPlayer() const { return GS()->GetPlayer(m_ClientID); }
CQuestDescription* CPlayerQuest::Info() const { return CQuestDescription::Data()[m_ID]; }

CPlayerQuest::~CPlayerQuest()
{
	m_vSteps.clear();
}

bool CPlayerQuest::HasUnfinishedSteps() const
{
	return std::any_of(m_vSteps.begin(), m_vSteps.end(), [](const CQuestStep& p)
	{
		return !p.m_StepComplete && p.m_Bot.m_HasAction;
	});
}

bool CPlayerQuest::Accept()
{
	// check valid player and quest state
	CPlayer* pPlayer = GetPlayer();
	if(m_State != QuestState::NO_ACCEPT || !pPlayer)
		return false;

	// initialize
	m_State = QuestState::ACCEPT;
	m_Step = 1;
	m_Datafile.Create();
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_ID, GetPlayer()->Account()->GetID(), m_State);

	// send information
	int ClientID = GetPlayer()->GetCID();
	if(Info()->IsHasFlag(QUEST_FLAG_DAILY))
	{
		GS()->Chat(ClientID, "Daily quest: '{}' accepted!", Info()->GetName());
	}
	else if(Info()->IsHasFlag(QUEST_FLAG_WEEKLY))
	{
		GS()->Chat(ClientID, "Weekly quest: '{}' accepted!", Info()->GetName());
	}
	else
	{
		const int StoryQuestsNum = Info()->GetStoryQuestsNum();
		const int QuestCurrentPos = Info()->GetStoryCurrentPos();
		GS()->Chat(ClientID, "{} Quest {} accepted {}",
			Utils::Aesthetic::B_PILLAR(3, false), Info()->GetStory(), QuestCurrentPos, StoryQuestsNum, Utils::Aesthetic::B_PILLAR(3, true));
		GS()->Chat(ClientID, "Name: \"{}\"", Info()->GetName());
		GS()->Chat(ClientID, "Reward: \"Gold {}, Experience {}\".", Info()->Reward().GetGold(), Info()->Reward().GetExperience());
	}

	GS()->Broadcast(ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Accepted");
	GS()->CreatePlayerSound(ClientID, SOUND_CTF_GRAB_EN);
	return true;
}

void CPlayerQuest::Refuse()
{
	// check valid player and quest state
	CPlayer* pPlayer = GetPlayer();
	if(m_State != QuestState::ACCEPT || !pPlayer)
		return;

	// refuse quest
	Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID = '%d' AND UserID = '%d'", m_ID, GetPlayer()->Account()->GetID());
	Reset();
}

void CPlayerQuest::Reset()
{
	m_Step = 0;
	m_vSteps.clear();
	m_State = QuestState::NO_ACCEPT;
	m_Datafile.Delete();
}

void CPlayerQuest::UpdateStepPosition()
{
	// check player valid and unfinished steps
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || HasUnfinishedSteps())
		return;

	// update
	m_Step++;
	Info()->PreparePlayerSteps(m_Step, m_ClientID, &m_vSteps);
	if(!m_vSteps.empty())
	{
		m_Datafile.Create();
		Update();
		return;
	}

	// finish quest
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, pPlayer->Account()->GetID());
	m_Datafile.Delete();

	// apply reward for player
	Info()->Reward().ApplyReward(pPlayer);

	// send information
	if(Info()->IsHasFlag(QUEST_FLAG_DAILY))
	{
		pPlayer->GetItem(itAlliedSeals)->Add(g_Config.m_SvDailyQuestAlliedSealsReward);
		GS()->Chat(-1, "{} completed daily quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed daily quest ({})", Info()->GetName());
	}
	else if(Info()->IsHasFlag(QUEST_FLAG_WEEKLY))
	{
		GS()->Chat(-1, "{} completed weekly quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed weekly quest ({})", Info()->GetName());
	}
	else
	{
		GS()->Chat(-1, "{} completed the \"{} - {}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetStory(), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed ({} - {})", Info()->GetStory(), Info()->GetName());
		GS()->Core()->DungeonManager()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);
	}

	GS()->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Complete");
	GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -70), 30, "QUEST COMPLETE");
	GS()->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);

	// save player stats and accept next story quest
	GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	GS()->Core()->QuestManager()->TryAcceptNextQuestChain(pPlayer, m_ID);
}

void CPlayerQuest::Update()
{
	for(auto& pStep : m_vSteps)
		pStep.Update();

	UpdateStepPosition();
}

CQuestStep* CPlayerQuest::GetStepByMob(int MobID)
{
	auto iter = std::find_if(m_vSteps.begin(), m_vSteps.end(), [MobID](const CQuestStep& Step) { return Step.m_Bot.m_ID == MobID; });
	return iter != m_vSteps.end() ? &(*iter) : nullptr;
}