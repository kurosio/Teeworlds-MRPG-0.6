/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Dungeons/DungeonManager.h>

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
	CPlayer* pPlayer = GetPlayer();
	if(m_State != QuestState::NO_ACCEPT || !pPlayer)
		return false;

	// Initialize the quest
	m_State = QuestState::ACCEPT;
	m_Step = 1;
	m_Datafile.Create();
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_ID, GetPlayer()->Account()->GetID(), m_State);

	// Send quest information to the player
	int ClientID = GetPlayer()->GetCID();
	if(!Info()->IsDaily())
	{
		const int StoryQuestsNum = Info()->GetStoryQuestsNum();
		const int QuestCurrentPos = Info()->GetStoryQuestPosition();
		GS()->Chat(ClientID, "{} Quest {}({} of {}) accepted {}",
			Utils::Aesthetic::B_PILLAR(3, false), Info()->GetStory(), QuestCurrentPos, StoryQuestsNum, Utils::Aesthetic::B_PILLAR(3, true));
		GS()->Chat(ClientID, "Name: \"{}\"", Info()->GetName());
		GS()->Chat(ClientID, "Reward: \"Gold {}, Experience {}\".", Info()->GetRewardGold(), Info()->GetRewardExp());
	}
	else
	{
		GS()->Chat(ClientID, "Daily quest: '{}' accepted!", Info()->GetName());
	}

	// effect's
	GS()->Broadcast(ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Accepted");
	GS()->CreatePlayerSound(ClientID, SOUND_CTF_GRAB_EN);
	return true;
}

void CPlayerQuest::Refuse()
{
	CPlayer* pPlayer = GetPlayer();
	if(m_State != QuestState::ACCEPT || !pPlayer)
		return;

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
	// check whether the active steps is complete
	CPlayer* pPlayer = GetPlayer();
	if(!pPlayer || HasUnfinishedSteps())
		return;

	// Update step
	m_Step++;
	Info()->PreparePlayerSteps(m_Step, m_ClientID, &m_vSteps);
	if(!m_vSteps.empty())
	{
		m_Datafile.Create();
		Update();
		return;
	}

	// Finish quest because there are no next steps
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, pPlayer->Account()->GetID());
	m_Datafile.Delete();

	// Add the reward gold to the player's money and experience
	pPlayer->Account()->AddGold(Info()->GetRewardGold());
	pPlayer->Account()->AddExperience(Info()->GetRewardExp());

	// Check if indicating a daily quest
	if(Info()->IsDaily())
	{
		pPlayer->GetItem(itAlliedSeals)->Add(g_Config.m_SvDailyQuestAlliedSealsReward);
		GS()->Chat(-1, "{} completed daily quest \"{}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed daily quest ({})", Info()->GetName());
	}
	else
	{
		GS()->Chat(-1, "{} completed the \"{} - {}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetStory(), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed ({} - {})", Info()->GetStory(), Info()->GetName());

		// Notify the opened new zones and dungeons after completing the quest
		GS()->Core()->DungeonManager()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);
	}

	// save player stats and accept next story quest
	GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	GS()->Core()->QuestManager()->TryAcceptNextStoryQuest(pPlayer, m_ID);

	// effect's
	GS()->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Complete");
	GS()->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -70), 30, "QUEST COMPLETE");
	GS()->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);
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