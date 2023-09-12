/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotData.h"

std::map< int, DataBotInfo > DataBotInfo::ms_aDataBot;
std::map< int, NpcBotInfo > NpcBotInfo::ms_aNpcBot;
std::map< int, QuestBotInfo > QuestBotInfo::ms_aQuestBot;
std::map< int, MobBotInfo > MobBotInfo::ms_aMobBot;

/************************************************************************/
/*  Global data mob bot                                               */
/************************************************************************/
void MobBotInfo::InitDebuffs(int Seconds, int Range, float Chance, std::string& buffSets)
{
	if(!buffSets.empty())
	{
		size_t start;
		size_t end = 0;
		std::string delim = ",";

		while((start = buffSets.find_first_not_of(delim, end)) != std::string::npos)
		{
			end = buffSets.find(delim, start);
			m_Effects.push_back({ Chance, buffSets.substr(start, end - start), { Seconds, Range } });
		}
	}
}

/************************************************************************/
/*  Global data quest bot                                               */
/************************************************************************/
void QuestBotInfo::InitTasks(std::string JsonData)
{
	JsonTools::parseFromString(JsonData, [&](nlohmann::json& pJson)
	{
		// initilize required items
		if(pJson.find("required_items") != pJson.end())
		{
			for(auto& p : pJson["required_items"])
			{
				TaskRequiredItems::Type ParsedType = TaskRequiredItems::Type::DEFAULT;

				const int ItemID = p.value("id", -1);
				const int Count = p.value("count", -1);
				if(ItemID > 0 && Count > 0)
				{
					if(std::string Type = p.value("type", "default"); Type == "pickup")
					{
						ParsedType = TaskRequiredItems::Type::PICKUP;
					}
					else if(Type == "show")
					{
						ParsedType = TaskRequiredItems::Type::SHOW;
					}

					m_RequiredItems.push_back({ ItemID, Count, ParsedType });
				}
			}
		}

		// initilize reward items
		if(pJson.find("reward_items") != pJson.end())
		{
			for(auto& p : pJson["reward_items"])
			{
				const int ItemID = p.value("id", -1);
				const int Count = p.value("count", -1);
				if(ItemID > 0 && Count > 0)
				{
					m_RewardItems.push_back({ ItemID, Count });
				}
			}
		}

		// initilize defeat bots
		if(pJson.find("defeat_bots") != pJson.end())
		{
			for(auto& p : pJson["defeat_bots"])
			{
				const int BotID = p.value("id", -1);
				const int Count = p.value("count", -1);
				if(BotID > 0 && Count > 0)
				{
					m_RequiredDefeat.push_back({ BotID, Count });
				}
			}
		}

		// initilize move to
		if(pJson.find("move_to") != pJson.end())
		{
			int LatestBiggerStep = 1;

			for(auto& p : pJson["move_to"])
			{
				// initilize default
				const vec2 Position = { p.value("x", -1.f), p.value("y", -1.f) };
				const int WorldID = p.value("world_id", m_WorldID);
				const int Step = p.value("step", 1);
				const bool Navigator = p.value("navigator", true);
				const std::string TextChat = p.value("text", "\0").c_str();
				TaskRequiredMoveTo::Types Type = TaskRequiredMoveTo::Types::MOVE_ONLY;

				// initilize pick_up_item object
				TaskRequiredMoveTo::PickupItem PickUpItem{};
				if(p.find("pick_up_item") != p.end())
				{
					auto& PickUpItemJson = p["pick_up_item"];
					PickUpItem.m_ID = PickUpItemJson.value("id", -1);
					PickUpItem.m_Count = PickUpItemJson.value("count", 1);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}

				// initilize required_item object
				TaskRequiredMoveTo::RequiredItem RequiredItem{};
				if(p.find("required_item") != p.end())
				{
					auto& RequiredItemJson = p["required_item"];
					RequiredItem.m_ID = RequiredItemJson.value("id", -1);
					RequiredItem.m_Count = RequiredItemJson.value("count", 1);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}

				// initilize use chat type
				const std::string TextUseInChat = p.value("use_in_chat", "\0").c_str();
				if(!TextUseInChat.empty())
				{
					Type = TaskRequiredMoveTo::Types::USE_CHAT_MODE;
				}

				// steps can only be taken to increase the orderly 
				if(Step > LatestBiggerStep)
				{
					LatestBiggerStep = Step;
				}

				// add element to container
				if(total_size_vec2(Position) > 0.f)
				{
					TaskRequiredMoveTo Move;
					Move.m_WorldID = WorldID;
					Move.m_Step = Step;
					Move.m_Navigator = Navigator;
					Move.m_PickupItem = PickUpItem;
					Move.m_RequiredItem = RequiredItem;
					Move.m_Position = Position;
					Move.m_aTextChat = TextChat;
					Move.m_aTextUseInChat = TextUseInChat;
					Move.m_Type = Type;
					m_RequiredMoveTo.push_back(Move);
				}
			}
		}
	});
}
