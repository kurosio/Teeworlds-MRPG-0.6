/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tutorial_manager.h"
#include "tutorial_data.h"

#include <game/server/gamecontext.h>

constexpr auto FILE_NAME_INITILIZER = "server_data/tutorial_data.json";

void CTutorialManager::OnInit()
{
	// checking dir
	if(!fs_is_dir("server_data/tutorial_tmp"))
	{
		fs_makedir("server_data");
		fs_makedir("server_data/tutorial_tmp");
	}

	// load file
	ByteArray RawData;
	Tools::Files::Result Result = Tools::Files::loadFile(FILE_NAME_INITILIZER, &RawData);
	dbg_assert(Result != Tools::Files::Result::ERROR_FILE, "tutorial file not found (\"server_data/tutorial_data.json\")");

	Tools::Json::parseFromString((char*)RawData.data(), [&](nlohmann::json& pJson)
	{
		for(auto& pStep : pJson)
		{
			if(const int Type = pStep.value("type", 0); (1 << Type) & (int)TutorialType::TUTORIAL_VECTOR_ONE)
			{
				TutorialData Tutorial(vec2(pStep.value("posX", 0), pStep.value("posY", 0)));
				TutorialBase::Init(Type, pStep.value("info", "\0").c_str(), Tutorial);
			}
			else if((1 << Type) & (int)TutorialType::TUTORIAL_INTEGER_ONE)
			{
				TutorialData Tutorial(pStep.value("integer", 0));
				TutorialBase::Init(Type, pStep.value("info", "\0").c_str(), Tutorial);
			}
			else if((1 << Type) & (int)TutorialType::TUTORIAL_STRING_ONE)
			{
				TutorialData Tutorial(pStep.value("string", "\0"));
				TutorialBase::Init(Type, pStep.value("info", "\0").c_str(), Tutorial);
			}
		}
		dbg_msg("tutorial", "tutorial data loaded (size: '%lu')", TutorialBase::Data().size());
	});
}

// Function to check events in the tutorial
template <typename CAST>
void EventChecker(std::deque<TutorialBase*>& pItems, CPlayer* pPlayer, int Step, std::function<bool(CAST*)> Value)
{
	CGS* pGS = pPlayer->GS();
	if(CAST* pTutorial = dynamic_cast<CAST*>(pItems[Step]); Value(pTutorial)) // Check if the current tutorial item is of type CAST and satisfies the specified condition
	{
		// Perform actions when the condition is true
		pGS->CreateDeath(pPlayer->m_ViewPos, pPlayer->GetCID());
		pGS->CreatePlayerSound(pPlayer->GetCID(), SOUND_NINJA_HIT);
		pGS->CreateText(nullptr, false, vec2(pPlayer->m_ViewPos.x, pPlayer->m_ViewPos.y - 50.0f), vec2(0, 0), 100, "Good");

		// Increment the tutorial step
		pPlayer->m_TutorialStep++;
	}
	else if(pGS->Server()->Tick() % (pGS->Server()->TickSpeed() * 1) == 0) // Check if it is time to display a tutorial message
	{
		pGS->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "- Tutorial {} of {}.\n\n{}", Step + 1, pItems.size(), pTutorial->GetText()); // Display the tutorial message
	}
}

// Function to handle tutorials for a player
void CTutorialManager::HandleTutorial(CPlayer* pPlayer) const
{
	// Get the tutorial step of the player
	int PlayerStep = pPlayer->m_TutorialStep - 1;

	// Check if the player step is within the valid range
	if(PlayerStep < 0 || PlayerStep >= static_cast<int>(TutorialBase::Data().size()))
		return;

	// Get the type of the tutorial
	TutorialType Type = GetTutorial(PlayerStep)->GetTutorialType();

	// Check if the tutorial type is TUTORIAL_MOVE_TO
	if(Type == TutorialType::TUTORIAL_MOVE_TO)
	{
		// Define a lambda function to check if the player has moved to the specified position
		EventChecker<TutorialData<vec2>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<vec2>* pTutorial)
		{
			return distance(pTutorial->m_Data, pPlayer->m_ViewPos) < 100;
		});
	}

	// Check if the tutorial type is TUTORIAL_EQUIP
	if(Type == TutorialType::TUTORIAL_EQUIP)
	{
		// Define a lambda function to check if the player has equipped the specified item
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->GetItem(pTutorial->m_Data)->IsEquipped();
		});
	}

	// Check if the tutorial type is TUTORIAL_PLAYER_FLAG
	if(Type == TutorialType::TUTORIAL_PLAYER_FLAG)
	{
		// Define a lambda function to check if the player has the specified player flag
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->m_PlayerFlags & pTutorial->m_Data;
		});
	}

	// Check if the tutorial type is TUTORIAL_OPEN_VOTE_MENU
	if(Type == TutorialType::TUTORIAL_OPEN_VOTE_MENU)
	{
		// Define a lambda function to check if the player has the specified vote menu open
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->m_VotesData.GetCurrentMenuID() == pTutorial->m_Data;
		});
	}

	// Check if the tutorial type is TUTORIAL_ACCEPT_QUEST
	if(Type == TutorialType::TUTORIAL_ACCEPT_QUEST)
	{
		// Define a lambda function to check if the player has accepted the specified quest
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->GetQuest(pTutorial->m_Data)->GetState() >= QuestState::ACCEPT;
		});
	}

	// Check if the tutorial type is TUTORIAL_FINISHED_QUEST
	if(Type == TutorialType::TUTORIAL_FINISHED_QUEST)
	{
		// Define a lambda function to check if the player has finished the specified quest
		EventChecker<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
		{
			return pPlayer->GetQuest(pTutorial->m_Data)->GetState() == QuestState::FINISHED;
		});
	}

	// Check if the tutorial type is TUTORIAL_CHAT_MSG
	if(Type == TutorialType::TUTORIAL_CHAT_MSG)
	{
		// Define a lambda function to check if the player has received the specified chat message
		EventChecker<TutorialData<std::string>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<std::string>* pTutorial)
		{
			return str_comp(pPlayer->m_aLastMsg, pTutorial->m_Data.c_str()) == 0;
		});
	}
}