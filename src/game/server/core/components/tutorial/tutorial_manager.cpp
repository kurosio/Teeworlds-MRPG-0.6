/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tutorial_manager.h"
#include "tutorial_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

constexpr auto TUTORIAL_FILE_PATH = "server_data/tutorial_data.json";

void CTutorialManager::OnInit()
{
	// load file
	ByteArray RawData;
	Utils::Files::Result Result = Utils::Files::loadFile(TUTORIAL_FILE_PATH, &RawData);
	dbg_assert(Result != Utils::Files::Result::ERROR_FILE, "Tutorial file not found (\"server_data/tutorial_data.json\")");

	// parsing tutorial data from file
	Utils::Json::parseFromString((char*)RawData.data(), [&](nlohmann::json& pJson)
	{
		for(auto& pStep : pJson)
		{
			const int Type = pStep.value("type", 0);
			std::string Info = pStep.value("info", "\0");

			// initialize tutorial based on its type
			if((1 << Type) & (int)TutorialType::TUTORIAL_VECTOR_ONE)
			{
				vec2 Position(pStep.value("posX", 0), pStep.value("posY", 0));
				TutorialBase::Init(Type, Info.c_str(), TutorialData(Position));
			}
			else if((1 << Type) & (int)TutorialType::TUTORIAL_INTEGER_ONE)
			{
				int IntegerData = pStep.value("integer", 0);
				TutorialBase::Init(Type, Info.c_str(), TutorialData(IntegerData));
			}
			else if((1 << Type) & (int)TutorialType::TUTORIAL_STRING_ONE)
			{
				std::string StringData = pStep.value("string", "\0");
				TutorialBase::Init(Type, Info.c_str(), TutorialData(StringData));
			}
		}

		dbg_msg("tutorial", "Tutorial data loaded (size: '%lu')", TutorialBase::Data().size());
	});
}

template <typename TDataType>
void ProcessEvent(std::deque<TutorialBase*>& pItems, CPlayer* pPlayer, int Step, std::function<bool(TDataType*)> ConditionCheck)
{
	CGS* pGS = pPlayer->GS();

	// check if tutorial step satisfies condition
	if(TDataType* pTutorial = dynamic_cast<TDataType*>(pItems[Step]); ConditionCheck(pTutorial))
	{
		pGS->CreateDeath(pPlayer->m_ViewPos, pPlayer->GetCID());
		pGS->CreatePlayerSound(pPlayer->GetCID(), SOUND_NINJA_HIT);
		pGS->EntityManager()->Text(pPlayer->m_ViewPos + vec2(0, -50), 100, "Good");

		pPlayer->m_TutorialStep++;
	}
	else if(pGS->Server()->Tick() % (pGS->Server()->TickSpeed() * 1) == 0)
	{
		pGS->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100,
			"- Tutorial {} of {}.\n\n{}", Step + 1, pItems.size(), pTutorial->GetText());
	}
}

void CTutorialManager::ProcessTutorialStep(CPlayer* pPlayer) const
{
	const int PlayerStep = pPlayer->m_TutorialStep - 1;
	if(PlayerStep < 0 || PlayerStep >= static_cast<int>(TutorialBase::Data().size()))
		return;

	TutorialType Type = GetTutorial(PlayerStep)->GetTutorialType();
	switch(Type)
	{
		case TutorialType::TUTORIAL_MOVE_TO:
			ProcessEvent<TutorialData<vec2>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<vec2>* pTutorial)
			{
				return distance(pTutorial->m_Data, pPlayer->m_ViewPos) < 100;
			});
			break;

		case TutorialType::TUTORIAL_EQUIP:
			ProcessEvent<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
			{
				return pPlayer->GetItem(pTutorial->m_Data)->IsEquipped();
			});
			break;

		case TutorialType::TUTORIAL_PLAYER_FLAG:
			ProcessEvent<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
			{
				return pPlayer->m_PlayerFlags & pTutorial->m_Data;
			});
			break;

		case TutorialType::TUTORIAL_OPEN_VOTE_MENU:
			ProcessEvent<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
			{
				return pPlayer->m_VotesData.GetCurrentMenuID() == pTutorial->m_Data;
			});
			break;

		case TutorialType::TUTORIAL_ACCEPT_QUEST:
			ProcessEvent<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
			{
				return pPlayer->GetQuest(pTutorial->m_Data)->GetState() >= QuestState::ACCEPT;
			});
			break;

		case TutorialType::TUTORIAL_FINISHED_QUEST:
			ProcessEvent<TutorialData<int>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<int>* pTutorial)
			{
				return pPlayer->GetQuest(pTutorial->m_Data)->GetState() == QuestState::FINISHED;
			});
			break;

		case TutorialType::TUTORIAL_CHAT_MSG:
			ProcessEvent<TutorialData<std::string>>(TutorialBase::Data(), pPlayer, PlayerStep, [&](const TutorialData<std::string>* pTutorial)
			{
				return str_comp(pPlayer->m_aLastMsg, pTutorial->m_Data.c_str()) == 0;
			});
			break;

		default:
			break;
	}
}
