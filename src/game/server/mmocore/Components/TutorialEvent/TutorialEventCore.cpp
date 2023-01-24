/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "TutorialEventCore.h"

#include "TutorialData.h"

#include <game/server/gamecontext.h>

inline std::string GetAccountFile(int AccountID) { return "server_data/tutorial_tmp/tutuid" +  std::to_string(AccountID) + ".tut"; }
constexpr auto FILE_NAME_INITILIZER = "server_data/tutorial_data.json";

void CTutorialEventCore::OnInit()
{
	// checking dir
	if(!fs_is_dir("server_data/tutorial_tmp"))
	{
		fs_makedir("server_data");
		fs_makedir("server_data/tutorial_tmp");
	}

	// load file
	IOHANDLE File = io_open(FILE_NAME_INITILIZER, IOFLAG_READ);
	dbg_assert(File != nullptr, "tutorial file not found (\"server_data/tutorial_data.json\")");

	const int FileSize = (int)io_length(File) + 1;
	char* pFileData = (char*)malloc(FileSize);
	mem_zero(pFileData, FileSize);
	io_read(File, pFileData, FileSize);

	JsonTools::parseFromString(pFileData, [&](nlohmann::json& pJson)
	{
		for(auto& pStep : pJson)
		{
			if(const int Type = pStep.value("type", 0); (1 << Type) & TUTORIAL_VECTOR_ONE)
			{
				TutorialData<vec2> Tutorial;
				Tutorial.m_Data = std::make_tuple(vec2(pStep.value("posX", 0), pStep.value("posY", 0)));
				TutorialBase::Init(Type, pStep.value("info", "\0").c_str(), Tutorial);
			}
			else if((1 << Type) & TUTORIAL_INTEGER_ONE)
			{
				TutorialData<int> Tutorial;
				Tutorial.m_Data = pStep.value("integer", 0);
				TutorialBase::Init(Type, pStep.value("info", "\0").c_str(), Tutorial);
			}
			else if((1 << Type) & TUTORIAL_STRING_ONE)
			{
				TutorialData<std::string> Tutorial;
				Tutorial.m_Data = pStep.value("string", "\0");
				TutorialBase::Init(Type, pStep.value("info", "\0").c_str(), Tutorial);
			}
		}
	});

	mem_free(pFileData);
	io_close(File);
}

void CTutorialEventCore::OnInitAccount(CPlayer* pPlayer)
{
	Load(pPlayer);
}

void CTutorialEventCore::Load(CPlayer* pPlayer)
{
	// loading file is not open pereinitilized steps
	std::string FileName = GetAccountFile(pPlayer->Acc().m_UserID);
	IOHANDLE File = io_open(FileName.c_str(), IOFLAG_READ);
	if(!File)
	{
		pPlayer->m_TutorialStep = 1;
		Save(pPlayer);
		return;
	}

	// load file
	const int FileSize = (int)io_length(File) + 1;
	char* pFileData = (char*)malloc(FileSize);
	mem_zero(pFileData, FileSize);
	io_read(File, pFileData, FileSize);

	// close and clear
	nlohmann::json JsonQuestData = nlohmann::json::parse(pFileData);
	mem_free(pFileData);
	io_close(File);

	// loading steps
	pPlayer->m_TutorialStep = JsonQuestData.value("tutorial_step", 1);
}

void CTutorialEventCore::Save(CPlayer* pPlayer)
{
	// initialized tutorial step
	nlohmann::json JsonQuestData;
	JsonQuestData["tutorial_step"] = pPlayer->m_TutorialStep;

	// save file
	if(const IOHANDLE File = io_open(GetAccountFile(pPlayer->Acc().m_UserID).c_str(), IOFLAG_WRITE))
	{
		const std::string Data = JsonQuestData.dump();
		io_write(File, Data.c_str(), Data.length());
		io_close(File);
	}
}

template <typename CAST>
void EventChecker(std::deque<TutorialBase*>& pItems, CPlayer* pPlayer, int Step, std::function<bool(CAST*)> Value)
{
	CGS* pGS = pPlayer->GS();
	if(CAST* pTutorial = dynamic_cast<CAST*>(pItems[Step]); Value(pTutorial))
	{
		pGS->CreateDeath(pPlayer->m_ViewPos, pPlayer->GetCID());
		pGS->CreatePlayerSound(pPlayer->GetCID(), SOUND_NINJA_HIT);
		pGS->CreateText(nullptr, false, vec2(pPlayer->m_ViewPos.x, pPlayer->m_ViewPos.y - 50.0f), vec2(0, 0), 100, "Good");
		pPlayer->m_TutorialStep++;
	}
	else if(pGS->Server()->Tick() % (pGS->Server()->TickSpeed() * 1) == 0)
	{
		pGS->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "[Tutorial {INT}/{INT}]\n{STR}", Step + 1, pItems.size(), pTutorial->m_aTextBuf);
	}
}

void CTutorialEventCore::TryCheckNextTutorialStep(CPlayer* pPlayer) const
{
	int PlayerStep = pPlayer->m_TutorialStep - 1;
	if(PlayerStep < 0 || PlayerStep >= static_cast<int>(TutorialBase::Data().size()))
		return;

	const int Type = GetTutorial(PlayerStep)->m_TutorialType;

	if(Type == TUTORIAL_MOVE_TO)
	{
		EventChecker<TutorialData<vec2>>(TutorialBase::Data(), pPlayer, PlayerStep,[&](const TutorialData<vec2>* pTutorial)
		{
			return distance(std::get<0>(pTutorial->m_Data), pPlayer->m_ViewPos) < 100;
		});
	}

	if(Type == TUTORIAL_EQUIP)
	{
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep,[&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->GetItem(std::get<0>(pTutorial->m_Data))->IsEquipped();
		});
	}

	if(Type == TUTORIAL_PLAYER_FLAG)
	{
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep,[&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->m_PlayerFlags & std::get<0>(pTutorial->m_Data);
		});
	}

	if(Type == TUTORIAL_OPEN_VOTE_MENU)
	{
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep,[&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->m_OpenVoteMenu == std::get<0>(pTutorial->m_Data);
		});
	}

	if(Type == TUTORIAL_CHAT_MSG)
	{
		EventChecker<TutorialData<std::string>>(TutorialBase::Data(), pPlayer, PlayerStep,[&](const TutorialData<std::string>* pTutorial)
		{
			return str_comp(pPlayer->m_aLastMsg, std::get<0>(pTutorial->m_Data).c_str()) == 0;
		});
	}
}
