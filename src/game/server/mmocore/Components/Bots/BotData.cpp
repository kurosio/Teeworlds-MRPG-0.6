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
				const vec2 Position = { p.value("x", -1.f), p.value("y", -1.f) };
				const int WorldID = p.value("world_id", m_WorldID);
				const int Step = p.value("step", 1);
				const int CollectItemID = p.value("collect_item_id", -1);
				const bool PathNavigator = p.value("navigator", true);

				if(Step > LatestBiggerStep)
					LatestBiggerStep = Step;

				if(Position.x > 0.f && Position.y > 0.f)
				{
					m_RequiredMoveTo.push_back({ Position, WorldID, LatestBiggerStep, CollectItemID, PathNavigator });
				}
			}
		}
	});
}
