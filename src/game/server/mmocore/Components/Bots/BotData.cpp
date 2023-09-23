/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotData.h"

std::map< int, DataBotInfo > DataBotInfo::ms_aDataBot;
std::map< int, NpcBotInfo > NpcBotInfo::ms_aNpcBot;
std::map< int, QuestBotInfo > QuestBotInfo::ms_aQuestBot;
std::map< int, MobBotInfo > MobBotInfo::ms_aMobBot;


/************************************************************************/
/*  Global data bot                                               */
/************************************************************************/
MobBotInfo* DataBotInfo::FindMobByBot(int BotID)
{
	for(auto& p : MobBotInfo::ms_aMobBot)
	{
		if(p.second.m_BotID == BotID)
			return &p.second;
	}

	return nullptr;
}

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
			m_Effects.emplace_back(Chance, buffSets.substr(start, end - start), std::make_pair(Seconds, Range));
		}
	}
}

/************************************************************************/
/*  Global data quest bot                                               */
/************************************************************************/
void QuestBotInfo::InitTasks(const std::string& JsonData)
{
	JsonTools::parseFromString(JsonData, [&](const nlohmann::json& pJson)
	{
		// initilize required items
		if(pJson.contains("required_items"))
		{
			for(const auto& p : pJson["required_items"])
			{
				TaskRequiredItems Task;
				Task.m_Item = CItem::FromJSON(p);

				if(Task.m_Item.IsValid())
				{
					const std::string Type = p.value("type", "default");
					if(Type == "pickup")
						Task.m_Type = TaskRequiredItems::Type::PICKUP;
					else if(Type == "show")
						Task.m_Type = TaskRequiredItems::Type::SHOW;
					else
						Task.m_Type = TaskRequiredItems::Type::DEFAULT;

					m_RequiredItems.push_back(Task);
				}
			}
		}

		// initilize reward items
		m_RewardItems = CItem::FromArrayJSON(pJson, "reward_items");

		// initilize defeat bots
		if(pJson.contains("defeat_bots"))
		{
			for(const auto& p : pJson["defeat_bots"])
			{
				const int BotID = p.value("id", -1);
				const int Value = p.value("value", -1);
				if(BotID > 0 && Value > 0)
				{
					m_RequiredDefeat.push_back({ BotID, Value });
				}
			}
		}

		// initilize move to points
		if(pJson.contains("move_to"))
		{
			int LatestBiggerStep = 1;
			for(const auto& p : pJson["move_to"])
			{
				const vec2 Position = { p.value("x", -1.f), p.value("y", -1.f) };
				const int WorldID = p.value("world_id", m_WorldID);
				const int Step = p.value("step", 1);
				const bool Navigator = p.value("navigator", true);
				TaskRequiredMoveTo::Types Type = TaskRequiredMoveTo::Types::MOVE_ONLY;
				const std::string EndText = p.value("end_text", "");

				// pickup item by array json
				CItem PickUpItem {};
				if(p.contains("pick_up_item"))
				{
					PickUpItem = CItem::FromJSON(p["pick_up_item"]);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}

				// required item by array json
				CItem RequiredItem {};
				if(p.contains("required_item"))
				{
					RequiredItem = CItem::FromJSON(p["required_item"]);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}

				// use text json element
				std::string TextUseInChat = p.value("use_in_chat", "");
				if(!TextUseInChat.empty())
				{
					TextUseInChat = "#" + TextUseInChat;
					Type = TaskRequiredMoveTo::Types::USE_CHAT_MODE;
				}

				if(Step > LatestBiggerStep)
					LatestBiggerStep = Step;
				if(total_size_vec2(Position) > 0.f)
				{
					TaskRequiredMoveTo Move;
					Move.m_WorldID = WorldID;
					Move.m_Step = Step;
					Move.m_Navigator = Navigator;
					Move.m_PickupItem = PickUpItem;
					Move.m_RequiredItem = RequiredItem;
					Move.m_Position = Position;
					Move.m_aEndText = EndText;
					Move.m_aTextUseInChat = TextUseInChat;
					Move.m_Type = Type;
					m_RequiredMoveTo.push_back(Move);
				}
			}
		}
	});
}