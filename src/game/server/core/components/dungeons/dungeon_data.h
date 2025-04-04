#ifndef GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_DATA_H

/*
 * Dungeon Concept
 *
 * 1. General Description:
 * A Dungeon is a separate world made up of multiple zones, each with a unique scenario and specific objectives. The dungeon follows a fixed script, which includes full enemy clears, as well as alternative solutions such as breaking objects to advance. Each dungeon will have a time completion rating, allowing players to compete for the best results.
 *
 * 2. Dungeon Structure:
 *
 * - Dungeon Zones:
 *   The dungeon consists of several zones, each with a unique theme and objectives. Each zone will contain multiple rooms or sections where players face enemies, traps, puzzles, and other obstacles. The zones can be:
 *   - Combat Zones: where players must fight enemies.
 *   - Puzzle Zones: where players need to solve challenges to progress.
 *   - Mechanism and Object Zones: for example, breaking walls or activating mechanisms to open doors.
 *
 * - Fixed Script Scenario:
 *   Each zone has a pre-written scenario that players must follow. The script will define the order of actions, such as which enemies need to be defeated, which mechanisms must be activated, and what needs to be done to move on to the next zone. This creates a unique sequence of challenges that players must complete step by step.
 *
 * - Completion Time Rating:
 *   Each dungeon run will be rated based on the time taken to complete all objectives. Faster runs will be rewarded with higher rankings. This encourages players to try again and improve their completion times.
 *
 * 3. Synchronization Mode with Player Attributes:
 *
 * - Synchronization Mode:
 *   In this mode, the player’s attributes (level, gear, skills, etc.) are synchronized with the dungeon. This means the difficulty of the dungeon will scale with the player's power, and rewards (loot, gold, experience) will be increased.
 *   - Enhanced Rewards: Due to synchronization, players will have a higher chance of receiving valuable loot, and they will earn more gold and experience. This mode is designed for more advanced players who want a challenging experience and greater rewards.
 *
 * - Non-Synchronization Mode:
 *   In this mode, player attributes do not affect the difficulty of the dungeon. The dungeon remains the same regardless of the player's level or equipment.
 *   - Reduced Rewards: Rewards remain, but they are given in smaller amounts (less gold, experience, and rare items). This mode is perfect for beginner players or those who prefer a more relaxed experience without the enhanced rewards.
 *
 * 4. Progression and Variety of Approaches:
 *
 * - Full Clear:
 *   The standard approach to dungeon progression involves clearing the zone of all enemies, overcoming all obstacles, and activating any necessary mechanisms. Upon completion, the path to the next zone opens.
 *
 * - Alternative Solutions:
 *   In some zones, there may be alternative ways to progress. For example, instead of fighting enemies, the player might break a wall or activate a hidden mechanism to move forward. These alternatives are rewarded with bonuses or additional rewards.
 *
 * - Environmental Interactivity:
 *   Some zones may contain destructible objects or hidden elements that players can use to achieve their objectives (e.g., collapsing structures to break barriers or revealing secret paths).
 *
 * 5. Replayability and Future Progression:
 *
 * - Progressive Difficulty:
 *   The zones in the dungeon may increase in difficulty with each new level or run, offering players new challenges and opportunities to improve their skills. However, the script always remains fixed, allowing players to know exactly what to expect.
 *
 * - Completion Time and Achievements:
 *   Dungeon runs will be evaluated based on time, with the possibility of earning achievements for speed records or unique approaches to solving challenges. This adds an extra layer of motivation for players to improve their results.
 *
 * - Replayability:
 *   Even though the scenarios are fixed, the ability to choose between full clears and alternative approaches to progression keeps the game fresh. The different synchronization modes also create varied gameplay experiences.
 *
 * 6. Future Opportunities and Development:
 *
 * - Multiplayer:
 *   The dungeon can support a cooperative mode, where a group of players tackles the same zone together. This adds a layer of teamwork and coordination to the gameplay experience.
 *
 * - New Zones and Modifications:
 *   New zones can be added in the future, each with unique challenges, enemies, and puzzles. The possibility of adding new storylines and themes will keep the game engaging.
 *
 * - Economy and Trading:
 *   Rewards for completing dungeons can include rare items or gold, which players can trade or sell to others. This introduces an economic aspect to the game and opens up opportunities for player-to-player interaction.
 *
 * Conclusion:
 * This dungeon concept offers a deep and engaging experience with fixed scenarios that challenge players to think strategically and solve puzzles. The synchronization system allows for difficulty and reward adjustments based on player attributes, while the time ranking system adds a competitive element. Given the request for no random events, the dungeon will maintain consistency and predictability in every run.
 */


class CDungeonData : public MultiworldIdentifiableData< std::deque< CDungeonData* > >
{
	int m_ID {};
	vec2 m_DoorPos {};
	int m_Level {};
	std::string m_Name {};
	int m_State {};

	int m_Progress {};
	int m_Players {};
	int m_WorldID {};

public:
	static CDungeonData* CreateElement(int ID)
	{
		auto pData = new CDungeonData();
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(const vec2& DoorPos, int Level, std::string_view Name, int WorldID)
	{
		m_DoorPos = DoorPos;
		m_Level = Level;
		m_Name = Name;
		m_WorldID = WorldID;
	}

	int GetID() const { return m_ID; }
	bool IsPlaying() const { return m_State > 1; }
	int GetLevel() const { return m_Level; }
	int GetProgress() const { return m_Progress; }
	std::string_view GetName() const { return m_Name; }
	int GetPlayersNum() const { return m_Players; }
	int GetWorldID() const { return m_WorldID; }
};

#endif