/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Dungeons/DungeonManager.h>
#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "Entities/quest_mob_path_finder.h"

// Return the game server object associated with the player's client ID
CGS* CPlayerQuest::GS() const
{
	// Cast the result of Server()->GameServerPlayer(m_ClientID) to a CGS pointer
	return (CGS*)Server()->GameServerPlayer(m_ClientID);
}

// Get the player object associated with the player's client ID
CPlayer* CPlayerQuest::GetPlayer() const
{
	// Check if the client ID is within the valid range
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		// Return the player object at the corresponding client ID
		return GS()->m_apPlayers[m_ClientID];
	}
	// Return nullptr if the client ID is invalid
	return nullptr;
}

// Destructor for CPlayerQuest
CPlayerQuest::~CPlayerQuest()
{
	// Loop through each element in m_apEntityNPCNavigator and delete each pointer
	for(auto p : m_apEntityNPCNavigator)
		delete p;

	// Loop through each element in m_aPlayerSteps and clear each element
	for(auto& p : m_aPlayerSteps)
		p.second.Clear();

	// Clear the m_apEntityNPCNavigator vector
	m_apEntityNPCNavigator.clear();

	// Clear the m_aPlayerSteps map
	m_aPlayerSteps.clear();
}

// Return a pointer to the CQuestDescription object corresponding to the quest ID of the player
CQuestDescription* CPlayerQuest::Info() const
{
	return &CQuestDescription::Data()[m_ID];
}

// Return the JSON file name for the player quest based on the player's account ID
std::string CPlayerQuest::GetJsonFileName() const
{
	return Info()->GetJsonFileName(GetPlayer()->Account()->GetID());
}

void CPlayerQuest::InitSteps()
{
	// check if the quest state is not ACCEPT or if the player does not exist
	if(m_State != QuestState::ACCEPT || !GetPlayer())
		return;

	// check if the "quest_tmp" directory does not exist
	if(!fs_is_dir("server_data/quest_tmp"))
	{
		// create the directories if it does not exist
		fs_makedir("server_data");
		fs_makedir("server_data/quest_tmp");
	}

	// initialize the quest steps
	m_Step = 1;
	Info()->InitPlayerDefaultSteps(m_ClientID, m_aPlayerSteps);

	// json structuring
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_aPlayerSteps)
	{
		if(pStep.second.m_Bot.m_HasAction)
		{
			// Create a JSON object called StepData
			nlohmann::json StepData;

			// Add values to the StepData object
			StepData["subbotid"] = pStep.second.m_Bot.m_SubBotID;
			StepData["state"] = pStep.second.m_StepComplete;

			// Check if the RequiredDefeat vector is not empty
			if(!pStep.second.m_Bot.m_RequiredDefeat.empty())
			{
				// Iterate through each element in the RequiredDefeat vector
				for(auto& p : pStep.second.m_Bot.m_RequiredDefeat)
				{
					// Create a JSON object for each required defeat
					StepData["defeat"].push_back({
						{"id", p.m_BotID},
						{"count", 0},
						{"complete", false}
						});
				}
			}

			// Check if the RequiredMoveTo vector is not empty
			if(!pStep.second.m_Bot.m_RequiredMoveTo.empty())
			{
				// Iterate through each element in the RequiredMoveTo vector
				for(size_t i = 0; i < pStep.second.m_Bot.m_RequiredMoveTo.size(); i++)
				{
					// Create a JSON object for each required move
					StepData["move_to"].push_back({
						{"complete", false}
						});
				}
			}

			// Add the StepData object to the "steps" array in the JsonQuestData object
			JsonQuestData["steps"].push_back(StepData);
		}

	}

	// Loop through each player step
	for(auto& pStep : m_aPlayerSteps)
	{
		// Check if the step requires defeating certain bots
		if(!pStep.second.m_Bot.m_RequiredDefeat.empty())
		{
			// Loop through the bots that need to be defeated
			for(auto& p : pStep.second.m_Bot.m_RequiredDefeat)
			{
				// Reset the count and completion status for the bot
				pStep.second.m_aMobProgress[p.m_BotID].m_Count = 0;
				pStep.second.m_aMobProgress[p.m_BotID].m_Complete = false;
			}
		}

		// Initialize the size of the MoveToProgress array based on the number of required move-to elements
		int MoveToElementsSize = pStep.second.m_Bot.m_RequiredMoveTo.size();
		pStep.second.m_aMoveToProgress.resize(MoveToElementsSize, false);

		// Update the player step
		pStep.second.Update();
	}

	// save file
	std::string Data = JsonQuestData.dump();
	Tools::Files::saveFile(GetJsonFileName().c_str(), Data.data(), (unsigned)Data.size());
}

void CPlayerQuest::LoadSteps()
{
	// only for accept state
	if(m_State != QuestState::ACCEPT)
		return;

	// loading file is not open pereinitilized steps
	ByteArray RawData;
	if(!Tools::Files::loadFile(GetJsonFileName().c_str(), &RawData))
	{
		InitSteps();
		return;
	}

	// init steps
	Info()->InitPlayerDefaultSteps(m_ClientID, m_aPlayerSteps);

	// loading steps
	nlohmann::json JsonQuestData = nlohmann::json::parse((char*)RawData.data());
	m_Step = JsonQuestData.value("current_step", 1);
	for(auto& pStep : JsonQuestData["steps"])
	{
		// Get the value of SubBotID from pStep and assign it to the constant SubBotID
		const int SubBotID = pStep.value("subbotid", 0);

		// Set the StepComplete value of the corresponding player step in the m_aPlayerSteps array to the value of "state" in pStep
		m_aPlayerSteps[SubBotID].m_StepComplete = pStep.value("state", false);

		// If the StepComplete is true, skip the rest of the code and continue to the next iteration
		if(m_aPlayerSteps[SubBotID].m_StepComplete)
			continue;

		// If "defeat" key exists in pStep
		if(pStep.find("defeat") != pStep.end())
		{
			// Iterate through each element in the "defeat" array
			for(auto& p : pStep["defeat"])
			{
				// Get the ID value from each element and assign it to ID
				int ID = p.value("id", 0);
				// Set the Count value of the corresponding mob progress in the m_aMobProgress array to the value of "count" in p
				m_aPlayerSteps[SubBotID].m_aMobProgress[ID].m_Count = p.value("count", 0);
				// Set the Complete value of the corresponding mob progress in the m_aMobProgress array to the value of "complete" in p
				m_aPlayerSteps[SubBotID].m_aMobProgress[ID].m_Complete = p.value("complete", 0);
			}
		}

		// If "move_to" key exists in pStep
		if(pStep.find("move_to") != pStep.end())
		{
			// Check if the size of the m_RequiredMoveTo vector in the m_Bot member of the m_aPlayerSteps vector at the index SubBotID is less than or equal to 0
			int MoveToElementsSize = (int)m_aPlayerSteps[SubBotID].m_Bot.m_RequiredMoveTo.size();
			if(MoveToElementsSize <= 0 || (MoveToElementsSize < (int)pStep["move_to"].size()))
			{
				// Print a debug message andd call InitSteps
				dbg_msg("quest system", "Reinitialization called... Player save file has a MoveTo value, but it is not present in the data!");
				InitSteps();
				return;
			}

			// Initialize the size of the MoveToProgress array based on the number of required move-to elements
			m_aPlayerSteps[SubBotID].m_aMoveToProgress.resize(MoveToElementsSize, false);

			// Iterate through each element in the "move_to" array of pStep
			for(int p = 0; p < (int)pStep["move_to"].size(); p++)
			{
				// Set the corresponding index in m_aMoveToProgress array of m_aPlayerSteps to the "complete" value of pStep["move_to"][p]
				m_aPlayerSteps[SubBotID].m_aMoveToProgress[p] = pStep["move_to"][p].value("complete", false);
			}
		}

		// Set ClientQuitting value of the corresponding player step to false
		m_aPlayerSteps[SubBotID].m_ClientQuitting = false;
	}

	// Update the steps of the bot
	for(auto& pStep : m_aPlayerSteps)
	{
		// If the current step is not complete
		if(!pStep.second.m_StepComplete)
		{
			// Update the current step
			pStep.second.Update();
		}
	}
}

bool CPlayerQuest::SaveSteps()
{
	// Check if the current state of the quest is not "ACCEPT"
	if(m_State != QuestState::ACCEPT)
		return false;

	// json structuring
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_aPlayerSteps)
	{
		if(pStep.second.m_Bot.m_HasAction)
		{
			// Creating a json object called stepData
			nlohmann::json stepData;

			// Assigning the value of m_SubBotID to the key "subbotid" in the stepData object
			stepData["subbotid"] = pStep.second.m_Bot.m_SubBotID;

			// Assigning the value of m_StepComplete to the key "state" in the stepData object
			stepData["state"] = pStep.second.m_StepComplete;

			// Looping through each key-value pair in the m_aMobProgress map of the pStep.second object
			for(auto& p : pStep.second.m_aMobProgress)
			{
				// Creating a json object for each map entry in the m_aMobProgress map
				// and adding it to the "defeat" array in the stepData object
				stepData["defeat"].push_back(
					{
						{ "id", p.first }, // Assigning the key "id" with the value of p.first (key from the map)
						{ "count", p.second.m_Count }, // Assigning the key "count" with the value of p.second.m_Count (value from the map value)
						{ "complete", p.second.m_Complete } // Assigning the key "complete" with the value of p.second.m_Complete (value from the map value)
					});
			}

			// Looping through each value in the m_aMoveToProgress vector of the pStep.second object
			for(auto& p : pStep.second.m_aMoveToProgress)
			{
				// Creating a json object for each value in the m_aMoveToProgress vector
				// and adding it to the "move_to" array in the stepData object
				stepData["move_to"].push_back(
					{
						{ "complete", p }, // Assigning the key "complete" with the value of p (value from the vector element)
					});
			}

			// Adding the stepData object to the "steps" array in the JsonQuestData object
			JsonQuestData["steps"].push_back(stepData);
		}
	}

	// replace file
	std::string Data = JsonQuestData.dump();
	Tools::Files::saveFile(GetJsonFileName().c_str(), Data.data(), (unsigned)Data.size());
	return true;
}

void CPlayerQuest::ClearSteps()
{
	// Iterate through all player steps
	for(auto& p : m_aPlayerSteps)
	{
		// Clear the data of the player step
		p.second.Clear();
	}

	// Clear the player steps map
	m_aPlayerSteps.clear();

	// Remove the temporary user quest data file
	Tools::Files::deleteFile(GetJsonFileName().c_str());
	fs_remove(GetJsonFileName().c_str());
}

// Function to handle accepting a quest by the player
bool CPlayerQuest::Accept()
{
	// Check if the quest state is not NO_ACCEPT and if there is a player
	if(m_State != QuestState::NO_ACCEPT || !GetPlayer())
		return false;

	// Get the ID of the player
	int ClientID = GetPlayer()->GetCID();

	// Get the Game Server instance and cast it to CGS
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// Set the quest state to ACCEPT and insert the quest into the database
	m_State = QuestState::ACCEPT;
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_ID, GetPlayer()->Account()->GetID(), m_State);

	// Initialize the quest steps
	InitSteps();

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
	ClearSteps();

	// Set the state of the quest to NO_ACCEPT
	m_State = QuestState::NO_ACCEPT;

	// Remove the quest record from the database table "tw_accounts_quests"
	Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID = '%d' AND UserID = '%d'", m_ID, GetPlayer()->Account()->GetID());
}

void CPlayerQuest::Reset()
{
	// Set the quest state to NO_ACCEPT
	m_State = QuestState::NO_ACCEPT;

	// Clear the steps of the quest
	ClearSteps();
}

void CPlayerQuest::Finish()
{
	CPlayer* pPlayer = GetPlayer();
	if(m_State != QuestState::ACCEPT || !pPlayer)
		return;

	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(m_ClientID);

	// finish quest
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, pPlayer->Account()->GetID());

	// clear steps
	ClearSteps();

	// Add the reward gold to the player's money and experience
	pPlayer->AddMoney(Info()->GetRewardGold());
	pPlayer->Account()->AddExperience(Info()->GetRewardExp());

	// Check if indicating a daily quest
	if(Info()->IsDaily())
	{
		// Add the maximum number of Allied Seals that can be obtained from a daily quest to the player's item inventory
		pPlayer->GetItem(itAlliedSeals)->Add(MAX_ALLIED_SEALS_BY_DAILY_QUEST);

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
		pGS->Mmo()->WorldSwap()->NotifyUnlockedZonesByQuest(pPlayer, m_ID);
		pGS->Mmo()->Dungeon()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);
	}

	// save player stats and accept next story quest
	pGS->Mmo()->SaveAccount(pPlayer, SAVE_STATS);
	pGS->Mmo()->Quest()->AcceptNextStoryQuest(pPlayer, m_ID);

	// effect's
	pGS->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Complete");
	pGS->CreateText(nullptr, false, vec2(pPlayer->m_ViewPos.x, pPlayer->m_ViewPos.y - 70), vec2(0, -0.5), 30, "QUEST COMPLETE");
	pGS->CreatePlayerSound(m_ClientID, SOUND_CTF_CAPTURE);
}

void CPlayerQuest::CheckAvailableNewStep()
{
	// check whether the active steps is complete
	if(std::any_of(m_aPlayerSteps.begin(), m_aPlayerSteps.end(), [this](std::pair <const int, CPlayerQuestStep>& p)
	{ return (p.second.m_Bot.m_Step == m_Step && !p.second.m_StepComplete && p.second.m_Bot.m_HasAction); }))
		return;

	m_Step++;

	// check if all steps have been completed
	bool FinalStep = true;
	for(auto& pStepBot : m_aPlayerSteps)
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

		CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(m_ClientID);
		pGS->StrongUpdateVotes(m_ClientID, MENU_JOURNAL_MAIN);
		pGS->StrongUpdateVotes(m_ClientID, MENU_MAIN);
	}
	else
	{
		SaveSteps();
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