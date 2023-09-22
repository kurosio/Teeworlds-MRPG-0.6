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
void QuestBotInfo::InitTasks(std::string JsonData)
{
	JsonTools::parseFromString(JsonData, [&](nlohmann::json& pJson)
	{
		// initilize required items
		if(pJson.find("required_items") != pJson.end())
		{
			for(auto& p : pJson["required_items"])
			{
				TaskRequiredItems Task;
				Task.m_Item = CItem::FromJSON(p);

				if(Task.m_Item.IsValid())
				{
					std::string Type = p.value("type", "default");
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
		if(pJson.find("defeat_bots") != pJson.end())
		{
			for(auto& p : pJson["defeat_bots"])
			{
				const int BotID = p.value("id", -1);
				const int Value = p.value("value", -1);
				if(BotID > 0 && Value > 0)
				{
					m_RequiredDefeat.push_back({ BotID, Value });
				}
			}
		}

		// Optimized
		if(pJson.find("move_to") != pJson.end())
		{
			int LatestBiggerStep = 1;
			for(auto& p : pJson["move_to"])
			{
				const vec2 Position = { p.value("x", -1.f), p.value("y", -1.f) };
				const int WorldID = p.value("world_id", m_WorldID);
				const int Step = p.value("step", 1);
				const bool Navigator = p.value("navigator", true);
				const std::string TextChat = p.value("text", "\0").c_str();
				TaskRequiredMoveTo::Types Type = TaskRequiredMoveTo::Types::MOVE_ONLY;
				CItem PickUpItem {};
				CItem RequiredItem {};
				TaskRequiredMoveTo::DefeatMob DefeatMob {};
				const std::string TextUseInChat = p.value("use_in_chat", "\0").c_str();

				if(p.find("pick_up_item") != p.end())
				{
					PickUpItem = CItem::FromJSON(p["pick_up_item"]);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}
				if(p.find("required_item") != p.end())
				{
					RequiredItem = CItem::FromJSON(p["required_item"]);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}
				if(p.find("defeat_mob") != p.end())
				{
					DefeatMob.m_BotID = p.value("id", 0);
					DefeatMob.m_Value = p.value("value", 0);
					Type = TaskRequiredMoveTo::Types::PRESS_FIRE;
				}
				if(!TextUseInChat.empty())
				{
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
					Move.m_DefeatMob = DefeatMob;
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
