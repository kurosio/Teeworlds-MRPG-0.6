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
void QuestBotInfo::InitTasksFromJSON(const std::string& JsonData)
{
	Utils::Json::parseFromString(JsonData, [&](const nlohmann::json& pJson)
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

					m_vRequiredItems.push_back(Task);
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
					m_vRequiredDefeat.push_back({ BotID, Value });
				}
			}
		}

		// initilize move to points
		m_AutoCompletesQuestStep = pJson.value("move_to_completes_quest_step", false);
		if(pJson.contains("move_to"))
		{
			int LatestBiggerStep = 1;

			for(const auto& p : pJson["move_to"])
			{
				const vec2 Position = { p.value("x", -1.f), p.value("y", -1.f) };
				const int WorldID = p.value("world_id", m_WorldID);
				const int Step = p.value("step", 1);
				const float Cooldown = p.value("cooldown", 0.f);
				const bool Navigator = p.value("navigator", true);
				const std::string CompletionText = p.value("completion_text", "");
				const std::string TaskName = p.value("name", "Demands a bit of action");

				// initialize data flags
				CItem PickUpItem {};
				CItem RequiredItem {};
				TaskAction::Interaction Interactive {};
				TaskAction::DefeatMob DefeatDescription {};
				unsigned int Type = TaskAction::Types::EMPTY;
				{
					// pickup item
					if(p.contains("pick_up_item"))
					{
						// Create a CItem object from the "pick_up_item" json object and assign it to PickUpItem
						PickUpItem = CItem::FromJSON(p["pick_up_item"]);

						// Set the PICKUP_ITEM flag in the Type variable
						Type |= TaskAction::Types::PICKUP_ITEM;
					}

					// required item
					if(p.contains("required_item"))
					{
						// Create a CItem object from the "required_item" json object and assign it to RequiredItem
						RequiredItem = CItem::FromJSON(p["required_item"]);

						// Set the REQUIRED_ITEM flag in the Type variable
						Type |= TaskAction::Types::REQUIRED_ITEM;
					}

					// interaction
					if(p.contains("interactive"))
					{
						// Retrieve the "interactive" element from the JSON object
						auto pIntJson = p["interactive"];

						// Retrieve the value of the "x" key from the "interactive" element
						// If the key does not exist, use -1 as the default value
						Interactive.m_Position.x = pIntJson.value("x", -1);

						// Retrieve the value of the "y" key from the "interactive" element
						// If the key does not exist, use -1 as the default value
						Interactive.m_Position.y = pIntJson.value("y", -1);

						// Check the current value of the "Type" variable
						if(Type == TaskAction::Types::EMPTY)
						{
							// If it is TaskAction::Types::EMPTY, add TaskAction::Types::INTERACTIVE to it
							Type |= TaskAction::Types::INTERACTIVE;
						}
						else if(Type == TaskAction::Types::PICKUP_ITEM)
						{
							// If it is TaskAction::Types::PICKUP_ITEM, set TaskAction::Types::INTERACTIVE_PICKUP to it
							Type = TaskAction::Types::INTERACTIVE_PICKUP;
						}
						else if(Type == TaskAction::Types::REQUIRED_ITEM)
						{
							// If it is TaskAction::Types::REQUIRED_ITEM, set TaskAction::Types::INTERACTIVE_REQUIRED to it
							Type = TaskAction::Types::INTERACTIVE_REQUIRED;
						}
					}
					// defeat mob json element
					else if(p.contains("defeat_bot"))
					{
						// Retrieve the "defeat_bot" element from the JSON object
						auto pDefJson = p["defeat_bot"];

						// Retrieve the value of the "id" key from the "defeat_bot" element
						// If the key does not exist, use -1 as the default value
						DefeatDescription.m_BotID = pDefJson.value("id", -1);

						// Retrieve the value of the "attribute_power" key from the "defeat_bot" element
						// If the key does not exist, use 10 as the default value
						DefeatDescription.m_AttributePower = pDefJson.value("attribute_power", 10);

						// Retrieve the value of the "attribute_spread" key from the "defeat_bot" element
						// If the key does not exist, use 0 as the default value
						DefeatDescription.m_AttributeSpread = pDefJson.value("attribute_spread", 0);

						// Retrieve the value of the "world_id" key from the "defeat_bot" element
						// If the key does not exist, use the value of m_WorldID as the default value
						DefeatDescription.m_WorldID = pDefJson.value("world_id", m_WorldID);

						// Check the current value of the "Type" variable
						if(Type == TaskAction::Types::EMPTY)
						{
							// If it is TaskAction::Types::EMPTY, add TaskAction::Types::DEFEAT_MOB to it
							Type |= TaskAction::Types::DEFEAT_MOB;
						}
						else if(Type == TaskAction::Types::PICKUP_ITEM)
						{
							// If it is TaskAction::Types::PICKUP_ITEM, set TaskAction::Types::DEFEAT_MOB_PICKUP to it
							Type = TaskAction::Types::DEFEAT_MOB_PICKUP;
						}
					}
				}

				// update latest bigger step
				if(Step > LatestBiggerStep)
				{
					LatestBiggerStep = Step;
				}

				// add new move_to point
				if(!is_negative_vec(Position))
				{
					TaskAction Move;
					Move.m_WorldID = WorldID;
					Move.m_Step = LatestBiggerStep;
					Move.m_Navigator = Navigator;
					Move.m_Cooldown = (int)(Cooldown * (float)SERVER_TICK_SPEED);
					Move.m_PickupItem = PickUpItem;
					Move.m_RequiredItem = RequiredItem;
					Move.m_Position = Position;
					Move.m_CompletionText = CompletionText;
					Move.m_TaskName = TaskName;
					Move.m_Type = maximum(Type, (unsigned int)TaskAction::Types::MOVE_ONLY);
					Move.m_QuestBotID = m_ID;
					Move.m_Interaction = Interactive;
					Move.m_DefeatMobInfo = DefeatDescription;
					m_vRequiredMoveAction.push_back(Move);
				}
			}
		}
	});
}