/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Dungeons/DungeonManager.h>

CGS* CPlayerQuest::GS() const { return (CGS*)Instance::GameServerPlayer(m_ClientID); }
CPlayer* CPlayerQuest::GetPlayer() const { return GS()->GetPlayer(m_ClientID); }
CQuestDescription* CPlayerQuest::Info() const { return CQuestDescription::Data()[m_ID]; }
std::string CPlayerQuest::GetDataFilename() const { return Info()->GetDataFilename(GetPlayer()->Account()->GetID()); }

CPlayerQuest::~CPlayerQuest()
{
	// Clear steps
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
	if(!pPlayer || m_State != QuestState::NO_ACCEPT)
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
		GS()->Chat(ClientID, "{STR} Quest {STR}({INT} of {INT}) accepted {STR}",
			Tools::Aesthetic::B_PILLAR(3, false), Info()->GetStory(), QuestCurrentPos, StoryQuestsNum, Tools::Aesthetic::B_PILLAR(3, true));
		GS()->Chat(ClientID, "Name: \"{STR}\"", Info()->GetName());
		GS()->Chat(ClientID, "Reward: \"Gold {VAL}, Experience {INT}\".", Info()->GetRewardGold(), Info()->GetRewardExp());
	}
	else
	{
		GS()->Chat(ClientID, "Daily quest: '{STR}' accepted!", Info()->GetName());
	}

	// effect's
	GS()->Broadcast(ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Accepted");
	GS()->CreatePlayerSound(ClientID, SOUND_CTF_GRAB_EN);
	return true;
}

void CPlayerQuest::Refuse()
{
	if(m_State != QuestState::ACCEPT || !GetPlayer())
		return;

	m_State = QuestState::NO_ACCEPT;
	Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID = '%d' AND UserID = '%d'", m_ID, GetPlayer()->Account()->GetID());
	m_Datafile.Delete();
}

void CPlayerQuest::Reset()
{
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

	// Finish quest
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, pPlayer->Account()->GetID());
	m_Datafile.Delete();

	// Add the reward gold to the player's money and experience
	pPlayer->Account()->AddGold(Info()->GetRewardGold());
	pPlayer->Account()->AddExperience(Info()->GetRewardExp());

	// Check if indicating a daily quest
	if(Info()->IsDaily())
	{
		// Add the maximum number of Allied Seals that can be obtained from a daily quest to the player's item inventory
		pPlayer->GetItem(itAlliedSeals)->Add(g_Config.m_SvDailyQuestAlliedSealsReward);

		// Send a chat message to all players informing that the player has completed a daily quest
		GS()->Chat(-1, "{STR} completed daily quest \"{STR}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed daily quest ({STR})", Info()->GetName());
	}
	else
	{
		// Send a chat message to all players informing that the player has completed a regular quest
		GS()->Chat(-1, "{STR} completed the \"{STR} - {STR}\".", GS()->Server()->ClientName(m_ClientID), Info()->GetStory(), Info()->GetName());
		GS()->ChatDiscord(DC_SERVER_INFO, GS()->Server()->ClientName(m_ClientID), "Completed ({STR} - {STR})", Info()->GetStory(), Info()->GetName());

		// Notify the opened new zones and dungeons after completing the quest
		GS()->Core()->DungeonManager()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);
	}

	// save player stats and accept next story quest
	GS()->Core()->SaveAccount(pPlayer, SAVE_STATS);
	GS()->Core()->QuestManager()->AcceptNextStoryQuest(pPlayer, m_ID);

	// effect's
	GS()->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Complete");
	GS()->CreateText(nullptr, false, vec2(pPlayer->m_ViewPos.x, pPlayer->m_ViewPos.y - 70), vec2(0, -0.5), 30, "QUEST COMPLETE");
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

/*
 * QuestDatafile
 */
void QuestDatafile::Create()
{
	// check if the quest state is not ACCEPT or if the player does not exist
	if(!m_pQuest || m_pQuest->m_State != QuestState::ACCEPT || !m_pQuest->GetPlayer())
		return;

	// check if the "directories" does not exist
	if(!fs_is_dir("server_data/quest_tmp"))
	{
		fs_makedir("server_data");
		fs_makedir("server_data/quest_tmp");
	}

	// json structuring
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_pQuest->m_Step;
	m_pQuest->Info()->PreparePlayerSteps(m_pQuest->m_Step, m_pQuest->m_ClientID, &m_pQuest->m_vSteps);
	for(auto& Step : m_pQuest->m_vSteps)
	{
		nlohmann::json Append;
		Append["quest_bot_id"] = Step.m_Bot.m_ID;
		Append["state"] = false;

		for(auto& p : Step.m_Bot.m_vRequiredDefeat)
		{
			Step.m_aMobProgress[p.m_BotID].m_Count = 0;
			Append["defeat"].push_back({ { "id", p.m_BotID }, { "count", 0 }, { "complete", false } });
		}

		Step.m_aMoveToProgress.resize(Step.m_Bot.m_vRequiredMoveAction.size(), false);
		for(auto& p : Step.m_Bot.m_vRequiredMoveAction)
			Append["move_to"].push_back({ { "complete", false } });

		JsonQuestData["steps"].push_back(Append);
	}

	// Loop through each player step
	for(auto& Step : m_pQuest->m_vSteps)
	{
		int MoveToElementsSize = Step.m_Bot.m_vRequiredMoveAction.size();
		Step.m_aMoveToProgress.resize(MoveToElementsSize, false);
		Step.Update();
	}

	// save file
	std::string Data = JsonQuestData.dump();
	Tools::Files::saveFile(m_pQuest->GetDataFilename().c_str(), Data.data(), (unsigned)Data.size());
}

void QuestDatafile::Load()
{
	// only for accept state
	if(!m_pQuest || m_pQuest->m_State != QuestState::ACCEPT)
		return;

	// loading file is not open pereinitilized steps
	ByteArray RawData;
	if(!Tools::Files::loadFile(m_pQuest->GetDataFilename().c_str(), &RawData))
	{
		Create();
		return;
	}

	// loading steps
	nlohmann::json JsonQuestData = nlohmann::json::parse((char*)RawData.data());
	m_pQuest->m_Step = JsonQuestData.value("current_step", 1);
	m_pQuest->Info()->PreparePlayerSteps(m_pQuest->m_Step, m_pQuest->m_ClientID, &m_pQuest->m_vSteps);

	// Check defferent size of steps
	if(JsonQuestData["steps"].size() != m_pQuest->m_vSteps.size())
	{
		dbg_msg("quest system", "Reinitialization... Player save file has a different size of steps!");
		Create();
		return;
	}

	// iterate through each element in the "steps" array of JsonQuestData
	int dequePos = 0;
	for(auto& Step : JsonQuestData["steps"])
	{
		auto& WorkedNode = m_pQuest->m_vSteps[dequePos++];
		WorkedNode.m_StepComplete = Step.value("state", false);
		if(WorkedNode.m_StepComplete)
			continue;

		// If "defeat" key exists in pStep
		if(Step.contains("defeat"))
		{
			// If the size of the "defeat" array in pStep is not equal to the size of the m_aMobProgress map of the corresponding player step
			if(Step["defeat"].size() != WorkedNode.m_Bot.m_vRequiredDefeat.size())
			{
				dbg_msg("quest system", "Reinitialization... Player save file has a defeat value, but it is not present in the data!");
				Create();
				return;
			}

			// Iterate through each element in the "defeat" array
			for(auto& p : Step["defeat"])
			{
				int ID = p.value("id", 0);
				WorkedNode.m_aMobProgress[ID].m_Count = p.value("count", 0);
				WorkedNode.m_aMobProgress[ID].m_Complete = p.value("complete", 0);
			}
		}

		// If "move_to" key exists in pStep
		if(Step.contains("move_to"))
		{
			// Check if the size of the "move_to" array of pStep is not equal to the size of the m_aPlayerSteps[SubBotID].m_Bot.m_RequiredMoveTo vector
			size_t TotalAction = WorkedNode.m_Bot.m_vRequiredMoveAction.size();
			if(Step["move_to"].size() != TotalAction)
			{
				dbg_msg("quest system", "Reinitialization... Player save file has a move_to value, but it is not present in the data!");
				Create();
				return;
			}

			// Initialize the size of the MoveToProgress array based on the number of required move-to elements
			WorkedNode.m_aMoveToProgress.resize(TotalAction, false);
			for(int p = 0; p < (int)Step["move_to"].size(); p++)
				WorkedNode.m_aMoveToProgress[p] = Step["move_to"][p].value("complete", false);
		}

		// Set ClientQuitting value of the corresponding player step to false
		WorkedNode.m_ClientQuitting = false;
	}

	// Update the steps of the bot
	for(auto& pStep : m_pQuest->m_vSteps)
	{
		// If the current step is not complete
		if(!pStep.m_StepComplete)
			pStep.Update();
	}

	// save file
	std::string Data = JsonQuestData.dump();
	Tools::Files::saveFile(m_pQuest->GetDataFilename().c_str(), Data.data(), (unsigned)Data.size());
}

bool QuestDatafile::Save()
{
	// Check if the current state of the quest is not "ACCEPT"
	if(!m_pQuest || m_pQuest->m_State != QuestState::ACCEPT)
		return false;

	// json structuring
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_pQuest->m_Step;
	for(auto& Step : m_pQuest->m_vSteps)
	{
		nlohmann::json Append;
		Append["quest_bot_id"] = Step.m_Bot.m_ID;
		Append["state"] = Step.m_StepComplete;
		for(auto& p : Step.m_aMobProgress)
			Append["defeat"].push_back({ { "id", p.first }, { "count", p.second.m_Count }, { "complete", p.second.m_Complete } });
		for(auto& p : Step.m_aMoveToProgress)
			Append["move_to"].push_back({ { "complete", p } });

		JsonQuestData["steps"].push_back(Append);
	}

	// replace file
	std::string Data = JsonQuestData.dump();
	auto Result = Tools::Files::saveFile(m_pQuest->GetDataFilename().c_str(), Data.data(), (unsigned)Data.size());
	return (Result == Tools::Files::Result::SUCCESSFUL);
}

void QuestDatafile::Delete()
{
	if(!m_pQuest)
		return;

	m_pQuest->m_vSteps.clear();

	// Remove the temporary user quest data file
	Tools::Files::deleteFile(m_pQuest->GetDataFilename().c_str());
	fs_remove(m_pQuest->GetDataFilename().c_str());
}
