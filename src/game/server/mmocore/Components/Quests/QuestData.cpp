/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Dungeons/DungeonManager.h>
#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "Entities/quest_mob_path_finder.h"

CGS* CPlayerQuest::GS() const
{
	return (CGS*)Server()->GameServerPlayer(m_ClientID);
}

CPlayer* CPlayerQuest::GetPlayer() const
{
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		return GS()->m_apPlayers[m_ClientID];
	}
	return nullptr;
}

CPlayerQuest::~CPlayerQuest()
{
	for(auto p : m_apEntityNPCNavigator)
		delete p;
	for(auto& p : m_aPlayerSteps)
		p.second.Clear();

	m_apEntityNPCNavigator.clear();
	m_aPlayerSteps.clear();
}

CQuestDescription* CPlayerQuest::Info() const { return &CQuestDescription::Data()[m_ID]; }
std::string CPlayerQuest::GetJsonFileName() const { return Info()->GetJsonFileName(GetPlayer()->Acc().m_ID); }

void CPlayerQuest::InitSteps()
{
	if(m_State != QuestState::ACCEPT || !GetPlayer())
		return;

	// checking dir
	if(!fs_is_dir("server_data/quest_tmp"))
	{
		fs_makedir("server_data");
		fs_makedir("server_data/quest_tmp");
	}

	// initialized quest steps
	m_Step = 1;
	Info()->InitPlayerDefaultSteps(m_ClientID, m_aPlayerSteps);

	// json structuring
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_aPlayerSteps)
	{
		if(pStep.second.m_Bot.m_HasAction)
		{
			nlohmann::json StepData;
			StepData["subbotid"] = pStep.second.m_Bot.m_SubBotID;
			StepData["state"] = pStep.second.m_StepComplete;

			if(!pStep.second.m_Bot.m_RequiredDefeat.empty())
			{
				for(auto& p : pStep.second.m_Bot.m_RequiredDefeat)
				{
					StepData["defeat"].push_back(
						{
							{ "id", p.m_BotID },
							{ "count", 0 },
							{"complete", false }
						});
				}
			}

			if(!pStep.second.m_Bot.m_RequiredMoveTo.empty())
			{
				for(size_t i = 0; i < pStep.second.m_Bot.m_RequiredMoveTo.size(); i++)
				{
					StepData["move_to"].push_back(
						{
							{ "complete", false },
						});
				}
			}

			JsonQuestData["steps"].push_back(StepData);
		}

	}

	// update step bot's and pre init
	for(auto& pStep : m_aPlayerSteps)
	{
		if(!pStep.second.m_Bot.m_RequiredDefeat.empty())
		{
			for(auto& p : pStep.second.m_Bot.m_RequiredDefeat)
			{
				pStep.second.m_aMobProgress[p.m_BotID].m_Count = 0;
				pStep.second.m_aMobProgress[p.m_BotID].m_Complete = false;
			}
		}

		int MoveToElementsSize = pStep.second.m_Bot.m_RequiredMoveTo.size();
		pStep.second.m_aMoveToProgress.resize(MoveToElementsSize, false);

		pStep.second.Update();
	}

	// save file
	IOHANDLE File = io_open(GetJsonFileName().c_str(), IOFLAG_WRITE);
	if(!File)
		return;

	std::string Data = JsonQuestData.dump();
	io_write(File, Data.c_str(), (unsigned)Data.length());
	io_close(File);
}

void CPlayerQuest::LoadSteps()
{
	// only for accept state
	if(m_State != QuestState::ACCEPT)
		return;

	// loading file is not open pereinitilized steps
	IOHANDLE File = io_open(GetJsonFileName().c_str(), IOFLAG_READ);
	if(!File)
	{
		InitSteps();
		return;
	}

	const int FileSize = (int)io_length(File) + 1;
	char* pFileData = (char*)malloc(FileSize);
	mem_zero(pFileData, FileSize);
	io_read(File, pFileData, FileSize);

	// close and clear
	nlohmann::json JsonQuestData = nlohmann::json::parse(pFileData);
	mem_free(pFileData);
	io_close(File);

	// loading steps
	Info()->InitPlayerDefaultSteps(m_ClientID, m_aPlayerSteps);
	m_Step = JsonQuestData.value("current_step", 1);
	for(auto& pStep : JsonQuestData["steps"])
	{
		const int SubBotID = pStep.value("subbotid", 0);
		m_aPlayerSteps[SubBotID].m_StepComplete = pStep.value("state", false);
		if(m_aPlayerSteps[SubBotID].m_StepComplete)
			continue;

		if(pStep.find("defeat") != pStep.end())
		{
			for(auto& p : pStep["defeat"])
			{
				int ID = p.value("id", 0);
				m_aPlayerSteps[SubBotID].m_aMobProgress[ID].m_Count = p.value("count", 0);
				m_aPlayerSteps[SubBotID].m_aMobProgress[ID].m_Complete = p.value("complete", 0);
			}
		}

		if(pStep.find("move_to") != pStep.end())
		{
			for(auto& p : pStep["move_to"])
			{
				m_aPlayerSteps[SubBotID].m_aMoveToProgress.push_back(p.value("complete", false));
			}
		}

		m_aPlayerSteps[SubBotID].m_ClientQuitting = false;
	}

	// update step bot's
	for(auto& pStep : m_aPlayerSteps)
	{
		if(!pStep.second.m_StepComplete)
		{
			pStep.second.Update();
		}
	}
}

bool CPlayerQuest::SaveSteps()
{
	// only for accept state
	if(m_State != QuestState::ACCEPT)
		return false;

	// json structuring
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_aPlayerSteps)
	{
		if(pStep.second.m_Bot.m_HasAction)
		{
			nlohmann::json stepData;
			stepData["subbotid"] = pStep.second.m_Bot.m_SubBotID;
			stepData["state"] = pStep.second.m_StepComplete;

			for(auto& p : pStep.second.m_aMobProgress)
			{
				stepData["defeat"].push_back(
					{
						{ "id", p.first },
						{ "count", p.second.m_Count },
						{"complete", p.second.m_Complete }
					});
			}

			for(auto& p : pStep.second.m_aMoveToProgress)
			{
				stepData["move_to"].push_back(
					{
						{ "complete", p },
					});
			}

			JsonQuestData["steps"].push_back(stepData);
		}
	}

	// replace file
	IOHANDLE File = io_open(GetJsonFileName().c_str(), IOFLAG_WRITE);
	if(!File)
		return false;

	std::string Data = JsonQuestData.dump();
	io_write(File, Data.c_str(), (unsigned)Data.length());
	io_close(File);
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
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_ID, GetPlayer()->Acc().m_ID, m_State);

	// Initialize the quest steps
	InitSteps();

	// Retrieve information about the quest
	const int QuestsSize = Info()->GetQuestStorySize();
	const int QuestPosition = Info()->GetQuestStoryPosition();

	// Send quest information to the player
	pGS->Chat(ClientID, "--- Quest story [{STR}] ({INT}/{INT})", Info()->GetStory(), QuestPosition, QuestsSize);
	pGS->Chat(ClientID, "Name: \"{STR}\"", Info()->GetName());
	pGS->Chat(ClientID, "Reward: \"Gold {VAL}, Experience {INT}\".", Info()->GetRewardGold(), Info()->GetRewardExp());

	// Broadcast quest accepted message to all players
	pGS->Broadcast(ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Accepted");

	// Create a sound effect for the player
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
	Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE ID = '%d'", m_ID);
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
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, GetPlayer()->Acc().m_ID);

	// clear steps
	ClearSteps();

	// awards and write about completion
	GetPlayer()->AddMoney(Info()->GetRewardGold());
	GetPlayer()->AddExp(Info()->GetRewardExp());
	pGS->Chat(-1, "{STR} completed the \"{STR} - {STR}\".", pGS->Server()->ClientName(m_ClientID), Info()->GetStory(), Info()->GetName());
	pGS->ChatDiscord(DC_SERVER_INFO, pGS->Server()->ClientName(m_ClientID), "Completed ({STR} - {STR})", Info()->GetStory(), Info()->GetName());

	// notify whether the after quest has opened something new
	pGS->Mmo()->WorldSwap()->NotifyUnlockedZonesByQuest(pPlayer, m_ID);
	pGS->Mmo()->Dungeon()->NotifyUnlockedDungeonsByQuest(pPlayer, m_ID);

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

CStepPathFinder* CPlayerQuest::FoundEntityNPCNavigator(int SubBotID) const
{
	for(const auto& pEnt : m_apEntityNPCNavigator)
	{
		if(pEnt && pEnt->m_SubBotID == SubBotID)
			return pEnt;
	}

	return nullptr;
}

CStepPathFinder* CPlayerQuest::AddEntityNPCNavigator(QuestBotInfo* pBot)
{
	if(!pBot)
		return nullptr;

	CStepPathFinder* pPathFinder = FoundEntityNPCNavigator(pBot->m_SubBotID);
	if(!pPathFinder)
	{
		pPathFinder = m_apEntityNPCNavigator.emplace_back(new CStepPathFinder(&GS()->m_World, pBot->m_Position, m_ClientID, *pBot, &m_apEntityNPCNavigator));
	}
	return pPathFinder;
}