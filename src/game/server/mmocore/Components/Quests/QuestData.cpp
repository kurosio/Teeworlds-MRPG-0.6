/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestCore.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Dungeons/DungeonCore.h>
#include <game/server/mmocore/Components/Worlds/WorldCore.h>

CQuestDataInfo& CQuestData::Info() const { return CQuestDataInfo::ms_aDataQuests[m_QuestID]; }
std::string CQuestData::GetJsonFileName() const { return Info().GetJsonFileName(m_pPlayer->Acc().m_UserID); }

std::map < int, std::map <int, CQuestData > > CQuestData::ms_aPlayerQuests;
void CQuestData::InitSteps()
{
	if(m_State != QuestState::ACCEPT || !m_pPlayer)
		return;

	// checking dir
	if(!fs_is_dir("server_data/quest_tmp"))
	{
		fs_makedir("server_data");
		fs_makedir("server_data/quest_tmp");
	}

	// initialized quest steps
	m_Step = 1;
	m_StepsQuestBot = Info().CopyBasicSteps();
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_StepsQuestBot)
	{
		pStep.second.m_MobProgress[0] = 0;
		pStep.second.m_MobProgress[1] = 0;
		pStep.second.m_StepComplete = false;
		pStep.second.m_ClientQuitting = false;
		pStep.second.UpdateBot();
		pStep.second.CreateStepArrow(m_pPlayer->GetCID());

		JsonQuestData["steps"].push_back(
		{
			{ "subbotid", pStep.second.m_Bot.m_SubBotID },
			{ "mobprogress1", pStep.second.m_MobProgress[0] },
			{ "mobprogress2", pStep.second.m_MobProgress[1] },
			{ "state", pStep.second.m_StepComplete }
		});
	}

	// save file
	IOHANDLE File = io_open(GetJsonFileName().c_str(), IOFLAG_WRITE);
	if(!File)
		return;

	std::string Data = JsonQuestData.dump();
	io_write(File, Data.c_str(), Data.length());
	io_close(File);
}

void CQuestData::LoadSteps()
{
	if(m_State != QuestState::ACCEPT || !m_pPlayer)
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
	m_StepsQuestBot = Info().CopyBasicSteps();
	m_Step = JsonQuestData.value("current_step", 1);
	auto Steps = JsonQuestData["steps"];
	for(auto& pStep : Steps)
	{
		const int SubBotID = pStep.value("subbotid", 0);
		m_StepsQuestBot[SubBotID].m_StepComplete = pStep.value("state", false);
		m_StepsQuestBot[SubBotID].m_MobProgress[0] = pStep.value("mobprogress1", 0);
		m_StepsQuestBot[SubBotID].m_MobProgress[1] = pStep.value("mobprogress2", 0);
		m_StepsQuestBot[SubBotID].m_ClientQuitting = false;
		m_StepsQuestBot[SubBotID].UpdateBot();
		m_StepsQuestBot[SubBotID].CreateStepArrow(m_pPlayer->GetCID());
	}
}

void CQuestData::SaveSteps()
{
	if(m_State != QuestState::ACCEPT)
		return;

	// set json structure
	nlohmann::json JsonQuestData;
	JsonQuestData["current_step"] = m_Step;
	for(auto& pStep : m_StepsQuestBot)
	{
		JsonQuestData["steps"].push_back(
		{
			{ "subbotid", pStep.second.m_Bot.m_SubBotID },
			{ "mobprogress1", pStep.second.m_MobProgress[0] },
			{ "mobprogress2", pStep.second.m_MobProgress[1] },
			{ "state", pStep.second.m_StepComplete }
		});
	}

	// replace file
	IOHANDLE File = io_open(GetJsonFileName().c_str(), IOFLAG_WRITE);
	if(!File)
		return;

	std::string Data = JsonQuestData.dump();
	io_write(File, Data.c_str(), Data.length());
	io_close(File);
}

void CQuestData::ClearSteps()
{
	for(auto& pStepBot : m_StepsQuestBot)
	{
		pStepBot.second.UpdateBot();
		pStepBot.second.CreateStepArrow(m_pPlayer->GetCID());
	}

	m_StepsQuestBot.clear();
	fs_remove(GetJsonFileName().c_str());
}

bool CQuestData::Accept()
{
	if(m_State != QuestState::NO_ACCEPT)
		return false;

	int ClientID = m_pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// init quest
	m_State = QuestState::ACCEPT;
	Database->Execute<DB::INSERT>("tw_accounts_quests", "(QuestID, UserID, Type) VALUES ('%d', '%d', '%d')", m_QuestID, m_pPlayer->Acc().m_UserID, m_State);

	// init steps
	InitSteps();

	// information
	const int QuestsSize = Info().GetQuestStorySize();
	const int QuestPosition = Info().GetQuestStoryPosition();
	pGS->Chat(ClientID, "------ Quest story [{STR}] ({INT}/{INT}) ------", Info().GetStory(), QuestPosition, QuestsSize);
	pGS->Chat(ClientID, "Name: \"{STR}\"", Info().GetName());
	pGS->Chat(ClientID, "Reward: \"Gold {VAL}, Experience {INT}\".", Info().m_Gold, Info().m_Exp);
	pGS->CreatePlayerSound(ClientID, SOUND_CTF_CAPTURE);
	return true;
}

void CQuestData::Finish()
{
	if(m_State != QuestState::ACCEPT)
		return;

	int ClientID = m_pPlayer->GetCID();
	CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

	// finish quest
	m_State = QuestState::FINISHED;
	Database->Execute<DB::UPDATE>("tw_accounts_quests", "Type = '%d' WHERE QuestID = '%d' AND UserID = '%d'", m_State, m_QuestID, m_pPlayer->Acc().m_UserID);

	// clear steps
	ClearSteps();

	// awards and write about completion
	m_pPlayer->AddMoney(Info().m_Gold);
	m_pPlayer->AddExp(Info().m_Exp);
	pGS->Chat(-1, "{STR} completed the \"{STR} - {STR}\".", pGS->Server()->ClientName(ClientID), Info().m_aStoryLine, Info().m_aName);
	pGS->ChatDiscord(DC_SERVER_INFO, pGS->Server()->ClientName(ClientID), "Completed ({STR} - {STR})", Info().m_aStoryLine, Info().m_aName);

	// check whether the after quest has opened something new
	pGS->Mmo()->WorldSwap()->CheckQuestingOpened(m_pPlayer, m_QuestID);
	pGS->Mmo()->Dungeon()->CheckQuestingOpened(m_pPlayer, m_QuestID);

	// save player stats and accept next story quest
	pGS->Mmo()->SaveAccount(m_pPlayer, SAVE_STATS);
	pGS->Mmo()->Quest()->AcceptNextStoryQuestStep(m_pPlayer, m_QuestID);
	pGS->CreateText(nullptr, false, vec2(m_pPlayer->m_ViewPos.x, m_pPlayer->m_ViewPos.y - 70), vec2(0, -0.5), 30, "COMPLECTED");
}

void CQuestData::CheckAvailableNewStep()
{
	// check whether the active steps is complete
	if(std::find_if(m_StepsQuestBot.begin(), m_StepsQuestBot.end(), [this](std::pair <const int, CPlayerQuestStepDataInfo> &p)
	{ return (p.second.m_Bot.m_Step == m_Step && !p.second.m_StepComplete && p.second.m_Bot.m_HasAction); }) != m_StepsQuestBot.end())
		return;

	// update steps
	m_Step++;
	SaveSteps();

	// check if all steps have been completed
	bool FinalStep = true;
	for(auto& pStepBot : m_StepsQuestBot)
	{
		if(!pStepBot.second.m_StepComplete && pStepBot.second.m_Bot.m_HasAction)
			FinalStep = false;

		pStepBot.second.UpdateBot();
		pStepBot.second.CreateStepArrow(m_pPlayer->GetCID());
	}

	// finish the quest or update the step
	if(FinalStep)
	{
		Finish();

		int ClientID = m_pPlayer->GetCID();
		CGS* pGS = (CGS*)Instance::GetServer()->GameServerPlayer(ClientID);

		pGS->StrongUpdateVotes(ClientID, MENU_JOURNAL_MAIN);
		pGS->StrongUpdateVotes(ClientID, MENU_MAIN);
	}
}
