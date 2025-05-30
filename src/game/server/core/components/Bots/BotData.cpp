/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/collision.h>
#include "BotData.h"

std::map< int, DataBotInfo > DataBotInfo::ms_aDataBot;
std::map< int, NpcBotInfo > NpcBotInfo::ms_aNpcBot;
std::map< int, QuestBotInfo > QuestBotInfo::ms_aQuestBot;
std::map< int, MobBotInfo > MobBotInfo::ms_aMobBot;

void MobBotInfo::InitBehaviors(const DBSet& Behavior)
{
	if(Behavior.hasSet("sleepy"))
		m_BehaviorsFlags |= MOBFLAG_BEHAVIOR_SLEEPY;
	if(Behavior.hasSet("slower"))
		m_BehaviorsFlags |= MOBFLAG_BEHAVIOR_SLOWER;
	if(Behavior.hasSet("poisonous"))
		m_BehaviorsFlags |= MOBFLAG_BEHAVIOR_POISONOUS;
	if(Behavior.hasSet("neutral"))
		m_BehaviorsFlags |= MOBFLAG_BEHAVIOR_NEUTRAL;
}

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
void MobBotInfo::InitDebuffs(int Seconds, int Range, float Chance, const DBSet& buffSets)
{
	for(auto& def : buffSets.getItems())
	{
		CMobDebuff debuff(Chance, def, std::make_pair(Seconds, Range));
		m_Effects.emplace_back(debuff);
	}
}

/************************************************************************/
/*  Global data quest bot                                               */
/************************************************************************/
void QuestBotInfo::InitTasksFromJSON(CCollision* pCollision, const std::string& JsonData)
{
	mystd::json::parse(JsonData, [&](const nlohmann::json& pJson)
	{
		// initilize required items
		if(pJson.contains("required_items"))
		{
			for(const auto& p : pJson["required_items"])
			{
				TaskRequiredItems Task;
				p.get_to(Task.m_Item);

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
		m_RewardItems = pJson.value("reward_items", CItemsContainer {});

		// initilize defeat bots
		if(pJson.contains("defeat_bots"))
		{
			for(const auto& p : pJson["defeat_bots"])
			{
				const int BotID = p.value("id", -1);
				const int Value = p.value("value", -1);
				if(BotID > 0 && Value > 0)
				{
					m_vRequiredDefeats.push_back({ BotID, Value });
				}
			}
		}

		// initilize move to points
		if(pJson.contains("move_to"))
		{
			int LatestBiggerStep = 1;

			for(const auto& p : pJson["move_to"])
			{
				const auto Pos = vec2(p.value("x", -1.f), p.value("y", -1.f));
				const auto VerifyPos = pCollision->VerifyPoint(CCollision::COLFLAG_DEATH | CCollision::COLFLAG_SOLID,
					Pos, "QuestTask: Mob(ID:{}), Pos(X:{}({}), Y:{}({})) - invalid (death, solid) position.", m_ID, Pos.x, Pos.x / 32.f, Pos.y, Pos.y / 32.f);
				const int WorldID = p.value("world_id", m_WorldID);
				const int Step = p.value("step", 1);
				const float Cooldown = p.value("cooldown", 0.f);
				const bool Navigator = p.value("navigator", "true") == "true";
				const std::string CompletionText = p.value("completion_text", "");
				const std::string TaskName = p.value("name", "Demands a bit of action");
				const std::string Mode = p.value("mode", "move");

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
						PickUpItem = p.value("pick_up_item", CItem{});
						Type |= TaskAction::Types::TFPICKUP_ITEM;
					}

					// required item
					if(p.contains("required_item"))
					{
						RequiredItem = p.value("required_item", CItem {});
						Type |= TaskAction::Types::TFREQUIRED_ITEM;
					}

					// move
					if(Mode == "move")
					{
						Type |= TaskAction::Types::TFMOVING;
					}
					// moving press
					else if(Mode == "move_press")
					{
						Type |= TaskAction::Types::TFMOVING_PRESS;
					}
					// interaction
					else if(Mode == "move_follow_press" && p.contains("interactive"))
					{
						auto pIntJson = p["interactive"];
						Interactive.m_Position.x = pIntJson.value("x", -1);
						Interactive.m_Position.y = pIntJson.value("y", -1);

						Type |= TaskAction::Types::TFMOVING_FOLLOW_PRESS;
					}
					// defeat mob json element
					else if(Mode == "defeat_bot" && p.contains("defeat_bot"))
					{
						auto pDefJson = p["defeat_bot"];
						DefeatDescription.m_BotID = pDefJson.value("id", -1);
						DefeatDescription.m_AttributePower = pDefJson.value("attribute_power", 10);
						DefeatDescription.m_WorldID = pDefJson.value("world_id", m_WorldID);

						Type |= TaskAction::Types::TFDEFEAT_MOB;
					}

					// interactives with item's only with accepted press
					if((RequiredItem.IsValid() || PickUpItem.IsValid()) && (Type & TaskAction::Types::TFMOVING))
					{
						Type &= ~TaskAction::Types::TFMOVING;
						Type |= TaskAction::Types::TFMOVING_PRESS;
					}
				}

				// update latest bigger step
				if(Step > LatestBiggerStep)
				{
					LatestBiggerStep = Step;
				}

				// add new move_to point
				TaskAction Move;
				Move.m_WorldID = WorldID;
				Move.m_Step = LatestBiggerStep;
				Move.m_Navigator = Navigator;
				Move.m_Cooldown = (int)(Cooldown * (float)SERVER_TICK_SPEED);
				Move.m_PickupItem = PickUpItem;
				Move.m_RequiredItem = RequiredItem;
				Move.m_Position = VerifyPos;
				Move.m_CompletionText = CompletionText;
				Move.m_TaskName = TaskName;
				Move.m_TypeFlags = maximum(Type, (unsigned int)TaskAction::Types::TFMOVING);
				Move.m_QuestBotID = m_ID;
				Move.m_Interaction = Interactive;
				Move.m_DefeatMobInfo = DefeatDescription;
				m_vRequiredMoveAction.push_back(Move);
			}
		}
	});
}