/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Dungeons/DungeonManager.h>
#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "Entities/quest_mob_path_finder.h"

CGS* CQuest::GS() const
{
	return (CGS*)Server()->GameServerPlayer(m_ClientID);
}

CPlayer* CQuest::GetPlayer() const
{
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		return GS()->m_apPlayers[m_ClientID];
	}
	return nullptr;
}

CQuest::~CQuest()
{
	for(auto p : m_apEntityMobNavigator)
		delete p;
	for(auto& p : m_aPlayerSteps)
		p.second.Clear();

	m_apEntityMobNavigator.clear();
	m_aPlayerSteps.clear();
}

CQuestDescription* CQuest::Info() const { return &CQuestDescription::Data()[m_ID]; }
std::string CQuest::GetJsonFileName() const { return Info()->GetJsonFileName(GetPlayer()->Acc().m_UserID); }

void CQuest::InitSteps()
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
			JsonQuestData["steps"].push_back(
			{
				{ "subbotid", pStep.second.m_Bot.m_SubBotID },
				{ "state", pStep.second.m_StepComplete }
			});

			if(!pStep.second.m_Bot.m_RequiredDefeat.empty())
			{
				for(auto& p : pStep.second.m_Bot.m_RequiredDefeat)
				{
					JsonQuestData["steps"].back()["defeat"].push_back(
						{
							{ "id", p.m_BotID },
							{ "count", 0 },
						});
				}
			}

			if(!pStep.second.m_Bot.m_RequiredMoveTo.empty())
			{
				for(size_t i = 0; i < pStep.second.m_Bot.m_RequiredMoveTo.size(); i++)
				{
					JsonQuestData["steps"].back()["move_to"].push_back(
						{
							{ "complete", false },
						});
				}
			}
		}

	}

	// update step bot's and pre init
	for(auto& pStep : m_aPlayerSteps)
	{
		if(!pStep.second.m_Bot.m_RequiredDefeat.empty())
		{
			for(auto& p : pStep.second.m_Bot.m_RequiredDefeat)
				pStep.second.m_aMobProgress[p.m_BotID] = 0;
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

void CQuest::LoadSteps()
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
				m_aPlayerSteps[SubBotID].m_aMobProgress[p.value("id", 0)] = p.value("count", 0);
		}

		if(pStep.find("move_to") != pStep.end())
		{
			for(auto& p : pStep["move_to"])
				m_aPlayerSteps[SubBotID].m_aMoveToProgress.push_back(p.value("complete", false));
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

bool CQuest::SaveSteps()
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
			JsonQuestData["steps"].push_back(
				{
					{ "subbotid", pStep.second.m_Bot.m_SubBotID },
					{ "state", pStep.second.m_StepComplete }
				});

			for(auto& p : pStep.second.m_aMobProgress)
			{
				JsonQuestData["steps"].back()["defeat"].push_back(
					{
						{ "id", p.first },
						{ "count", p.second },
					});
			}

			for(auto& p : pStep.second.m_aMoveToProgress)
			{
				JsonQuestData["steps"].back()["move_to"].push_back(
					{
						{ "complete", p },
					});
			}
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

void CQuest::ClearSteps()
{
	// update status for bots
	for(auto& pStep : m_aPlayerSteps)
	{
		pStep.second.Update();
	}

	// clear and remove temp user quest data
	m_aPlayerSteps.clear();
	fs_remove(GetJsonFileName().c_str());
}

bool CQuest::Accept()
{
	if(m_State != QuestState::NO_ACCEPT || !GetPlayer())
		return false;

	int ClientID = GetPlayer()->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// init quest
	m_State = QuestState::ACCEPT;
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_ID, GetPlayer()->Acc().m_UserID, m_State);

	// init steps
	InitSteps();

	// information
	const int QuestsSize = Info()->GetQuestStorySize();
	const int QuestPosition = Info()->GetQuestStoryPosition();
	pGS->Chat(ClientID, "--- Quest story [{STR}] ({INT}/{INT})", Info()->GetStory(), QuestPosition, QuestsSize);
	pGS->Chat(ClientID, "Name: \"{STR}\"", Info()->GetName());
	pGS->Chat(ClientID, "Reward: \"Gold {VAL}, Experience {INT}\".", Info()->GetRewardGold(), Info()->GetRewardExp());
	pGS->Broadcast(ClientID, BroadcastPriority::TITLE_INFORMATION, 100, "Quest Accepted");
	pGS->CreatePlayerSound(ClientID, SOUND_CTF_GRAB_EN);
	return true;
}

void CQuest::Finish()
{
	CPlayer* pPlayer = GetPlayer();
	if(m_State != QuestState::ACCEPT || !pPlayer)
		return;

	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(m_ClientID);

	// finish quest
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_ID, GetPlayer()->Acc().m_UserID);

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

void CQuest::CheckAvailableNewStep()
{
	// check whether the active steps is complete
	if(std::find_if(m_aPlayerSteps.begin(), m_aPlayerSteps.end(), [this](std::pair <const int, CPlayerQuestStep> &p)
	{ return (p.second.m_Bot.m_Step == m_Step && !p.second.m_StepComplete && p.second.m_Bot.m_HasAction); }) != m_aPlayerSteps.end())
		return;

	m_Step++;

	// check if all steps have been completed
	bool FinalStep = true;
	for(auto& pStepBot : m_aPlayerSteps)
	{
		if(!pStepBot.second.m_StepComplete && pStepBot.second.m_Bot.m_HasAction)
			FinalStep = false;

		pStepBot.second.Update();
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

CStepPathFinder* CQuest::FoundEntityMobNavigator(int SubBotID) const
{
	for(const auto& pEnt : m_apEntityMobNavigator)
	{
		if(pEnt && pEnt->m_SubBotID == SubBotID)
			return pEnt;
	}

	return nullptr;
}

CStepPathFinder* CQuest::AddEntityMobNavigator(QuestBotInfo* pBot)
{
	if(!pBot)
		return nullptr;

	CStepPathFinder* pPathFinder = FoundEntityMobNavigator(pBot->m_SubBotID);
	if(!pPathFinder)
		pPathFinder = m_apEntityMobNavigator.emplace_back(new CStepPathFinder(&GS()->m_World, pBot->m_Position, m_ClientID, *pBot, &m_apEntityMobNavigator));

	return pPathFinder;
}