/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/external/xml/pugixml.hpp>

#include "QuestManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Dungeons/DungeonManager.h>

#include "Entities/quest_mob_path_finder.h"

// Destructor for CPlayerQuest
CPlayerQuest::~CPlayerQuest()
{
	// Loop through each element in m_apEntityNPCNavigator and delete each pointer
	for(auto p : m_apEntityNPCNavigator)
		delete p;

	// Loop through each element in m_vSteps and clear each element
	for(auto& p : m_vSteps)
		p.second.Clear();

	// Clear the m_apEntityNPCNavigator vector
	m_apEntityNPCNavigator.clear();

	// Clear the m_vSteps map
	m_vSteps.clear();
}

CGS* CPlayerQuest::GS() const { return (CGS*)Instance::GameServerPlayer(m_ClientID); }
CPlayer* CPlayerQuest::GetPlayer() const { return GS()->GetPlayer(m_ClientID); }
CQuestDescription* CPlayerQuest::Info() const { return CQuestDescription::Data()[m_ID]; }
std::string CPlayerQuest::GetDataFilename() const { return Info()->GetDataFilename(GetPlayer()->Account()->GetID()); }

// Function to handle accepting a quest by the player
bool CPlayerQuest::Accept()
{
	// Check if the quest state is not NO_ACCEPT and if there is a player
	if(m_State != QuestState::NO_ACCEPT || !GetPlayer())
		return false;

	// Get the ID of the player
	int ClientID = GetPlayer()->GetCID();

	// Get the Game Server instance and cast it to CGS
	CGS* pGS = (CGS*)Instance::GameServerPlayer(ClientID);

	// Set the quest state to ACCEPT and insert the quest into the database
	m_State = QuestState::ACCEPT;
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_ID, GetPlayer()->Account()->GetID(), m_State);

	// Initialize the quest steps
	m_Step = 1;
	Info()->PreparePlayerQuestSteps(ClientID, &m_vSteps);
	m_Datafile.Create();

	// Retrieve information about the quest
	const int QuestsSize = Info()->GetQuestStorySize();
	const int QuestPosition = Info()->GetQuestStoryPosition();

	// Send quest information to the player
	if(Info()->IsDaily())
	{
		pGS->Chat(ClientID, "Daily quest: '{STR}' accepted!", Info()->GetName());
	}
	else
	{
		pGS->Chat(ClientID, "--- Quest story [{STR}] ({INT}/{INT})", Info()->GetStory(), QuestPosition, QuestsSize);
		pGS->Chat(ClientID, "Name: \"{STR}\"", Info()->GetName());
		pGS->Chat(ClientID, "Reward: \"Gold {VAL}, Experience {INT}\".", Info()->GetRewardGold(), Info()->GetRewardExp());
	}

	// effect's
	pGS->Broadcast(ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Accepted");
	pGS->CreatePlayerSound(ClientID, SOUND_CTF_GRAB_EN);
	return true;
}

void CPlayerQuest::Refuse()
{
	// Check if the quest is in ACCEPT state and the player exists
	if(m_State != QuestState::ACCEPT || !GetPlayer())
		return;

	// Clear the steps of the quest
	m_Datafile.Delete();

	// Set the state of the quest to NO_ACCEPT and erase from database
	m_State = QuestState::NO_ACCEPT;
	Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID = '%d' AND UserID = '%d'", m_ID, GetPlayer()->Account()->GetID());
}

void CPlayerQuest::Reset()
{
	// Set the quest state to NO_ACCEPT
	m_State = QuestState::NO_ACCEPT;

	// Clear the steps of the quest
	m_Datafile.Delete();
}

void CPlayerQuest::Finish()
{
	CPlayer* pPlayer = GetPlayer();
	CGS* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);

	// check if the quest state is not ACCEPT or if the player does not exist
	if(m_State != QuestState::ACCEPT || !pPlayer)
		return;

	// finish quest
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, pPlayer->Account()->GetID());

	// clear steps
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
		pGS->Chat(-1, "{STR} completed daily quest \"{STR}\".", pGS->Server()->ClientName(m_ClientID), Info()->GetName());
		pGS->ChatDiscord(DC_SERVER_INFO, pGS->Server()->ClientName(m_ClientID), "Completed daily quest ({STR})", Info()->GetName());
	}
	else
	{
		// Send a chat message to all players informing that the player has completed a regular quest
		pGS->Chat(-1, "{STR} completed the \"{STR} - {STR}\".", pGS->Server()->ClientName(m_ClientID), Info()->GetStory(), Info()->GetName());
		pGS->ChatDiscord(DC_SERVER_INFO, pGS->Server()->ClientName(m_ClientID), "Completed ({STR} - {STR})", Info()->GetStory(), Info()->GetName());

		// Notify the opened new zones and dungeons after completing the quest
		pGS->Core()->DungeonManager()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);
	}

	// save player stats and accept next story quest
	pGS->Core()->SaveAccount(pPlayer, SAVE_STATS);
	pGS->Core()->QuestManager()->AcceptNextStoryQuest(pPlayer, m_ID);

	// effect's
	pGS->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Complete");
	pGS->CreateText(nullptr, false, vec2(pPlayer->m_ViewPos.x, pPlayer->m_ViewPos.y - 70), vec2(0, -0.5), 30, "QUEST COMPLETE");
	pGS->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);
}

void CPlayerQuest::CheckAvailableNewStep()
{
	// check whether the active steps is complete
	if(std::any_of(m_vSteps.begin(), m_vSteps.end(), [this](std::pair <const int, CQuestStep>& p)
	{ return (p.second.m_Bot.m_Step == m_Step && !p.second.m_StepComplete && p.second.m_Bot.m_HasAction); }))
		return;

	m_Step++;

	// check if all steps have been completed
	bool FinalStep = true;
	for(auto& pStepBot : m_vSteps)
	{
		if(!pStepBot.second.m_StepComplete)
		{
			if(pStepBot.second.m_Bot.m_HasAction)
				FinalStep = false;

			pStepBot.second.Update();
		}
	}

	// finish the quest or update the step
	if(FinalStep)
	{
		Finish();

		CGS* pGS = (CGS*)Instance::GameServerPlayer(m_ClientID);
		CPlayer* pPlayer = pGS->GetPlayer(m_ClientID);
		if(pPlayer)
		{
			pPlayer->m_VotesData.UpdateVotesIf(MENU_JOURNAL_MAIN);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_MAIN);
		}
	}
	else
	{
		m_Datafile.Save();
	}
}

// This function searches the array of EntityNPCNavigator for a specific SubBotID
// It returns a pointer to the EntityNPCNavigator if found, otherwise it returns nullptr
CStepPathFinder* CPlayerQuest::FoundEntityNPCNavigator(int SubBotID) const
{
	// Iterate through each EntityNPCNavigator pointer in the array
	for(const auto& pEnt : m_apEntityNPCNavigator)
	{
		// Check if the pointer is not null and if its SubBotID matches the target SubBotID
		if(pEnt && pEnt->m_SubBotID == SubBotID)
			return pEnt;
	}

	return nullptr;
}

// This method adds an NPC navigator to the player's quest
CStepPathFinder* CPlayerQuest::AddEntityNPCNavigator(QuestBotInfo* pBot)
{
	// If the given bot information is invalid, return nullptr
	if(!pBot)
		return nullptr;

	// Check if an NPC navigator for the given subBotID already exists
	CStepPathFinder* pPathFinder = FoundEntityNPCNavigator(pBot->m_SubBotID);

	// If an NPC navigator does not exist for the given subBotID, create a new one and add it to the navigator list
	if(!pPathFinder)
	{
		pPathFinder = m_apEntityNPCNavigator.emplace_back(new CStepPathFinder(&GS()->m_World, pBot->m_Position, m_ClientID, *pBot, &m_apEntityNPCNavigator));
	}

	// Return the NPC navigator
	return pPathFinder;
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
	for(auto& [MobID, Step] : m_pQuest->m_vSteps)
	{
		if(Step.m_Bot.m_HasAction)
		{
			// Add values to the StepData object
			auto& StepData = JsonQuestData["steps"];
			StepData["subbotid"] = Step.m_Bot.m_SubBotID;
			StepData["state"] = Step.m_StepComplete;

			// Check if the RequiredDefeat vector is not empty
			if(!Step.m_Bot.m_RequiredDefeat.empty())
			{
				for(auto& p : Step.m_Bot.m_RequiredDefeat)
					StepData["defeat"].push_back({ {"id", p.m_BotID}, {"count", 0}, {"complete", false} });
			}

			// Check if the RequiredMoveTo vector is not empty
			if(!Step.m_Bot.m_RequiredMoveTo.empty())
			{
				for(size_t i = 0; i < Step.m_Bot.m_RequiredMoveTo.size(); i++)
					StepData["move_to"].push_back({ {"complete", false} });
			}
		}
	}

	// Loop through each player step
	for(auto& StepPr : m_pQuest->m_vSteps)
	{
		int MoveToElementsSize = StepPr.second.m_Bot.m_RequiredMoveTo.size();
		StepPr.second.m_aMoveToProgress.resize(MoveToElementsSize, false);

		// Update the player step
		StepPr.second.Update();
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

	// Check defferent size of steps
	if(JsonQuestData["steps"].size() != m_pQuest->m_vSteps.size())
	{
		dbg_msg("quest system", "Reinitialization... Player save file has a different size of steps!");
		Create();
		return;
	}

	// iterate through each element in the "steps" array of JsonQuestData
	for(auto& pStep : JsonQuestData["steps"])
	{
		// Get the value of SubBotID from pStep and assign it to the constant SubBotID
		const int SubBotID = pStep.value("subbotid", 0);
		m_pQuest->m_vSteps[SubBotID].m_StepComplete = pStep.value("state", false);
		if(m_pQuest->m_vSteps[SubBotID].m_StepComplete)
			continue;

		// If "defeat" key exists in pStep
		if(pStep.contains("defeat"))
		{
			// If the size of the "defeat" array in pStep is not equal to the size of the m_aMobProgress map of the corresponding player step
			if(pStep["defeat"].size() != m_pQuest->m_vSteps[SubBotID].m_Bot.m_RequiredDefeat.size())
			{
				dbg_msg("quest system", "Reinitialization... Player save file has a defeat value, but it is not present in the data!");
				Create();
				return;
			}

			// Iterate through each element in the "defeat" array
			for(auto& p : pStep["defeat"])
			{
				int ID = p.value("id", 0);
				m_pQuest->m_vSteps[SubBotID].m_aMobProgress[ID].m_Count = p.value("count", 0);
				m_pQuest->m_vSteps[SubBotID].m_aMobProgress[ID].m_Complete = p.value("complete", 0);
			}
		}

		// If "move_to" key exists in pStep
		if(pStep.contains("move_to"))
		{
			// Check if the size of the "move_to" array of pStep is not equal to the size of the m_aPlayerSteps[SubBotID].m_Bot.m_RequiredMoveTo vector
			size_t TotalMoveToSize = m_pQuest->m_vSteps[SubBotID].m_Bot.m_RequiredMoveTo.size();
			if(pStep["move_to"].size() != TotalMoveToSize)
			{
				dbg_msg("quest system", "Reinitialization... Player save file has a move_to value, but it is not present in the data!");
				Create();
				return;
			}

			// Initialize the size of the MoveToProgress array based on the number of required move-to elements
			m_pQuest->m_vSteps[SubBotID].m_aMoveToProgress.resize(TotalMoveToSize, false);
			for(int p = 0; p < (int)pStep["move_to"].size(); p++)
				m_pQuest->m_vSteps[SubBotID].m_aMoveToProgress[p] = pStep["move_to"][p].value("complete", false);
		}

		// Set ClientQuitting value of the corresponding player step to false
		m_pQuest->m_vSteps[SubBotID].m_ClientQuitting = false;
	}

	// Update the steps of the bot
	for(auto& pStep : m_pQuest->m_vSteps)
	{
		// If the current step is not complete
		if(!pStep.second.m_StepComplete)
			pStep.second.Update();
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
	for(auto& pStep : m_pQuest->m_vSteps)
	{
		if(pStep.second.m_Bot.m_HasAction)
		{
			auto& StepData = JsonQuestData["steps"];
			StepData["subbotid"] = pStep.second.m_Bot.m_SubBotID;
			StepData["state"] = pStep.second.m_StepComplete;

			// Looping through each key-value pair in the m_aMobProgress map of the pStep.second object
			for(auto& p : pStep.second.m_aMobProgress)
				StepData["defeat"].push_back({ { "id", p.first }, { "count", p.second.m_Count }, { "complete", p.second.m_Complete } });

			// Looping through each value in the m_aMoveToProgress vector of the pStep.second object
			for(auto& p : pStep.second.m_aMoveToProgress)
				StepData["move_to"].push_back({ { "complete", p } });
		}
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

	for(auto& p : m_pQuest->m_vSteps)
		p.second.Clear();
	m_pQuest->m_vSteps.clear();

	// Remove the temporary user quest data file
	Tools::Files::deleteFile(m_pQuest->GetDataFilename().c_str());
	fs_remove(m_pQuest->GetDataFilename().c_str());
}
