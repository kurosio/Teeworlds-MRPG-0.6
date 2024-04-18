/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_CONTEXT_H
#define GAME_SERVER_CORE_MMO_CONTEXT_H
#include "teeother/flat_hash_map/flat_hash_map.h"

enum class WorldType : int
{
	Default,
	Dungeon,
	Tutorial,
};

enum class DrawboardToolEvent : int
{
	ON_START,
	ON_POINT_ADD,
	ON_POINT_ERASE,
	ON_END,
};

enum class ClassGroup : int
{
	None,
	Tank,
	DPS,
	Healer
};

enum TIME_PERIOD
{
	DAILY_STAMP,
	WEEK_STAMP,
	MONTH_STAMP,
	NUM_STAMPS
};

// Enum for input events related to key presses
enum InputEvents
{
	// Key events for firing different weapons
	KEY_EVENT_FIRE = 1 << 0,
	KEY_EVENT_FIRE_HAMMER = 1 << 1,
	KEY_EVENT_FIRE_GUN = 1 << 2,
	KEY_EVENT_FIRE_SHOTGUN = 1 << 3,
	KEY_EVENT_FIRE_GRENADE = 1 << 4,
	KEY_EVENT_FIRE_LASER = 1 << 5,
	KEY_EVENT_FIRE_NINJA = 1 << 6,

	// Key events for voting
	KEY_EVENT_VOTE_YES = 1 << 7,
	KEY_EVENT_VOTE_NO = 1 << 8,

	// Key event for player states
	KEY_EVENT_SCOREBOARD = 1 << 9,
	KEY_EVENT_CHAT = 1 << 10,

	// Key events for player actions
	KEY_EVENT_JUMP = 1 << 11,
	KEY_EVENT_HOOK = 1 << 12,

	// Key events for changing weapons
	KEY_EVENT_NEXT_WEAPON = 1 << 13,
	KEY_EVENT_PREV_WEAPON = 1 << 14,
	KEY_EVENT_MENU = 1 << 15,
	KEY_EVENT_WANTED_WEAPON = 1 << 16,
	KEY_EVENT_WANTED_HAMMER = 1 << 17,
	KEY_EVENT_WANTED_GUN = 1 << 18,
	KEY_EVENT_WANTED_SHOTGUN = 1 << 19,
	KEY_EVENT_WANTED_GRENADE = 1 << 20,
	KEY_EVENT_WANTED_LASER = 1 << 21,

	// Blocking states for input events
	BLOCK_INPUT_FREEZE_WEAPON = 1 << 0,
	BLOCK_INPUT_FREEZE_HAMMER = 1 << 1,
	BLOCK_INPUT_FREEZE_GUN = 1 << 2,
	BLOCK_INPUT_FREEZE_SHOTGUN = 1 << 3,
	BLOCK_INPUT_FREEZE_GRENADE = 1 << 4,
	BLOCK_INPUT_FREEZE_LASER = 1 << 5,
	BLOCK_INPUT_FIRE = 1 << 6,
	BLOCK_INPUT_HOOK = 1 << 7,
	BLOCK_INPUT_FULL_WEAPON = BLOCK_INPUT_FREEZE_WEAPON | BLOCK_INPUT_FIRE,
}; 

// laser orbite
enum class EntLaserOrbiteType : unsigned char
{
	DEFAULT, // Default value
	MOVE_LEFT, // Move left value
	MOVE_RIGHT, // Move right value
	INSIDE_ORBITE, // Inside orbite
	INSIDE_ORBITE_RANDOM, // Inside orbite
};

// mood type
enum class Mood : short
{
	NORMAL = 0,
	ANGRY,
	AGRESSED,
	FRIENDLY,
	QUEST,
	TANK,
};

// jobs
enum JobAccountStats
{
	JOB_LEVEL = 0,
	JOB_EXPERIENCE = 1,
	JOB_UPGRADES = 2,
	NUM_JOB_ACCOUNTS_STATS
};

// player ticks
enum TickState
{
	Die = 1,
	Respawn,
	LastSelfKill,
	LastEmote,
	LastChangeInfo,
	LastChangeTeam,
	LastChat,
	LastVoteTry,
	LastDialog,
	LastRandomBox,
	PotionRecast,
	RefreshClanTitle,
	RefreshNickLeveling,
	NUM_TICK,
};

// skills
enum Skill
{
	SkillHeartTurret = 1,	// health recovery turret
	SkillSleepyGravity = 2, // mobbing
	SkillCraftDiscount = 3, // discount on crafting
	SkillMasterWeapon = 4, // automatic gunfire
	SkillBlessingGodWar = 5, // refill ammunition
	SkillAttackTeleport = 6, // ?knockout? teleport
	SkillCureI = 7, // health recovery cure
	SkillProvoke = 8, // provoke
};

// toplist types
enum class ToplistType : int
{
	GUILDS_LEVELING,
	GUILDS_WEALTHY,
	PLAYERS_LEVELING,
	PLAYERS_WEALTHY,
	NUM_TOPLIST_TYPES
};

// item functionals
enum ItemFunctional : int
{
	EQUIP_HAMMER = 0,
	EQUIP_GUN,
	EQUIP_SHOTGUN,
	EQUIP_GRENADE,
	EQUIP_LASER,
	EQUIP_PICKAXE,
	EQUIP_RAKE,
	EQUIP_ARMOR,
	EQUIP_EIDOLON,
	NUM_EQUIPPED,

	FUNCTION_ONE_USED = 9,
	FUNCTION_USED,
	FUNCTION_SETTINGS,
	FUNCTION_PLANT,
	FUNCTION_MINER,
	NUM_FUNCTIONS,
};

// item types
enum class ItemType : short
{
	TYPE_INVISIBLE = 0,
	TYPE_USED,
	TYPE_CRAFT,
	TYPE_MODULE,
	TYPE_OTHER,
	TYPE_SETTINGS,
	TYPE_EQUIP,
	TYPE_DECORATION,
	TYPE_POTION,
	NUM_TYPES,
};

// quest state
enum class QuestState : int
{
	NO_ACCEPT = 0,
	ACCEPT,
	FINISHED,
};

// npc functions
enum FunctionsNPC
{
	FUNCTION_NPC_NURSE,
	FUNCTION_NPC_GIVE_QUEST,
	FUNCTION_NPC_GUARDIAN,
};

// menu list
enum MenuList
{
	MENU_MAIN = 1,

	// Main menu
	MENU_EQUIPMENT,
	MENU_INVENTORY,
	MENU_INBOX,
	MENU_UPGRADES,
	MENU_SETTINGS,
	MENU_SETTINGS_LANGUAGE_SELECT,

	// Guild-related
	MENU_GUILD,
	MENU_GUILD_MEMBERSHIP,
	MENU_GUILD_RANKS,
	MENU_GUILD_INVITES,
	MENU_GUILD_LOGS,
	MENU_GUILD_WARS,
	MENU_GUILD_HOUSE_PURCHASE_INFO,
	MENU_GUILD_HOUSE_PLANT_ZONE_SELECTED,
	MENU_GUILD_FINDER,
	MENU_GUILD_FINDER_SELECTED,

	// House-related
	MENU_HOUSE,
	MENU_HOUSE_PLANTS,
	MENU_HOUSE_ACCESS_TO_DOOR,
	MENU_HOUSE_BUY,

	// Warehouse
	MENU_WAREHOUSE,
	MENU_WAREHOUSE_BUY_ITEM_SELECTED,

	// Eidolon Collection menus
	MENU_EIDOLON_COLLECTION,
	MENU_EIDOLON_COLLECTION_SELECTED,

	// Journal menus
	MENU_JOURNAL_MAIN,
	MENU_JOURNAL_FINISHED,
	MENU_JOURNAL_QUEST_INFORMATION,

	// Skills
	MENU_SKILLS_LEARN_LIST,

	// Quest menus
	MENU_DAILY_BOARD,

	// Aethernet menus
	MENU_AETHERNET_LIST,

	// Auction
	MENU_AUCTION_LIST,
	MENU_AUCTION_CREATE_SLOT,

	// Group menus
	MENU_GROUP,

	// Grinding
	MENU_GUIDE_GRINDING,
	MENU_GUIDE_GRINDING_SELECT,

	// Crafting
	MENU_CRAFT_LIST,
	MENU_CRAFT_SELECTED,

	// Other menus
	MENU_TOP_LIST,
	MENU_DUNGEONS,
};

enum TabHideList
{
	TAB_STAT = 1,
	TAB_PERSONAL,
	TAB_INFORMATION,
	TAB_HOUSE_MANAGING,
	TAB_HOUSE_DOORS,
	TAB_HOUSE_SAFE_INTERACTIVE,
	TAB_HOUSE_ACCESS_TO_DOOR_REMOVE,
	TAB_HOUSE_ACCESS_TO_DOOR_ADD,
	TAB_EQUIP_SELECT,
	TAB_UPGR_DPS,
	TAB_UPGR_TANK,
	TAB_UPGR_HEALER,
	TAB_UPGR_WEAPON,
	TAB_INVENTORY_SELECT,
	TAB_UPGR_JOB,
	TAB_GUILD_STAT,
	TAB_GUILD_LOG_SETTINGS,
	TAB_STORAGE,
	TAB_DAILY_BOARD,
	TAB_DAILY_BOARD_WANTED,
	TAB_HOUSE_STAT,
	TAB_AETHER,
	TAB_EIDOLONS,
	TAB_EIDOLON_DESCRIPTION,
	TAB_EIDOLON_UNLOCKING_ENHANCEMENTS,
	TAB_LANGUAGES,
	TAB_SETTINGS,
	TAB_SETTINGS_MODULES,
	TAB_GROUP_COMMANDS,
	NUM_TAB_MENU_INTERACTIVES,

	// start info
	TAB_INFO_INVENTORY,
	TAB_INFO_HOUSE,
	TAB_INFO_HOUSE_INVITES_TO_DOOR,
	TAB_INFO_STAT,
	TAB_INFO_CRAFT,
	TAB_INFO_TOP,
	TAB_INFO_DUNGEON,
	TAB_INFO_UPGR,
	TAB_INFO_DECORATION,
	TAB_INFO_HOUSE_PLANT,
	TAB_INFO_GUILD_HOUSE,
	TAB_INFO_GUILD_HOUSE_PLANT_ZONE,
	TAB_INFO_EIDOLONS,
	TAB_INFO_LOOT,
	TAB_INFO_SKILL,
	TAB_INFO_LANGUAGES,
	TAB_INFO_AUCTION,
	TAB_INFO_AUCTION_BIND,
	TAB_INFO_EQUIP,
	TAB_INFO_STATISTIC_QUESTS,
	NUM_TAB_MENU,

	START_SELF_HIDE_ID = 1000,
};

// primary
enum
{
	/*
		Basic kernel server settings
		This is where the most basic server settings are stored
	*/
	MAX_GROUP_MEMBERS = 4,					// maximum number of players in a group
	MAX_HOUSE_DOOR_INVITED_PLAYERS = 3,		// maximum player what can have access for house door
	MAX_DECORATIONS_PER_HOUSE = 20,				// maximum decorations for houses
	MIN_SKINCHANGE_CLIENTVERSION = 0x0703,	// minimum client version for skin change
	MIN_RACE_CLIENTVERSION = 0x0704,		// minimum client version for race type
	MAILLETTER_MAX_CAPACITY = 20,			// maximum number of emails what is displayed
	MAX_ATTRIBUTES_FOR_ITEM = 2,			// maximum number of stats per item
	POTION_RECAST_APPEND_TIME = 15,			// recast append time for potion in seconds
	MAX_DAILY_QUESTS_BY_BOARD = 3,			// maximum number of daily quests that can be assigned to a specific board in a game. 

	// guild
	MAX_GUILD_SLOTS = 20,					// maximum guild player's
	MAX_GUILD_RANK_NUM = 5,					// maximum guild rank's
	MAX_GUILD_LOGS_NUM = 50,				// maximum guild log's
	DEFAULT_GUILD_AVAILABLE_SLOTS = 2,		// default available slots
	DEFAULT_GUILD_CHAIR = 1,				// default chair boost

	// settings items
	itShowEquipmentDescription = 25,	// Description setting
	itShowCriticalDamage = 34,			// Critical damage setting
	itShowQuestNavigator = 93,			// Show quest path when idle

	// items
	NOPE = -1,
	itGold = 1,							// Money ordinary currency
	itHammer = 2,						// Equipment Hammers
	itMaterial = 7,						// Scraping material
	itProduct = 8,						// Scraping material
	itSkillPoint = 9,					// Skillpoint
	itPotionManaRegen = 14,				// Mana regeneration potion
	itTinyHealthPotion = 15,			// Tiny health potion
	itCapsuleSurvivalExperience = 16,	// Gives 10-50 experience
	itLittleBagGold = 17,				// Gives 10-50 gold
	itDamageEqualizer = 18,				// Module for dissable self dmg
	itExplosiveGun = 19,				// Explosion for gun
	itExplosiveShotgun = 20,			// Explosion for shotgun
	itTicketResetClassStats = 21,		// Ticket to reset the statistics of class upgrades
	itPermissionExceedLimits = 22,		// Reserve
	itTicketResetWeaponStats = 23,		// Ticket to reset the statistics cartridge upgrade
	itTicketDiscountCraft = 24,			// Discount ticket for crafting
	itRandomHomeDecoration = 26,		// Random home decor
	itAlliedSeals = 30,					// Allied seals
	itEidolonOtohime = 57,				// Eidolon
	itRandomRelicsBox = 58,				// Random Relics box
	itEidolonMerrilee = 59,				// Eidolon
	itPoisonHook = 64,					// Poison hook
	itExplodeHook = 65,					// Explode hook
	itSpiderHook = 66,					// Spider hook
	itEidolonDryad = 80,				// Eidolon
	itEidolonPigQueen = 88,				// Eidolon
	itAdventurersBadge = 92,			// The adventurer's badge
	itTicketGuild = 95,					// Ticket for the creation of the guild

	// decoration items
	itPickupHealth = 10,					// Pickup heart
	itPickupArmor = 13,					// Pickup armor
	itPickupShotgun = 11,				// Pickup shotgun
	itPickupGrenade = 12,				// Pickup grenade
	itPickupLaser = 94,				    // Pickup laser

	// all sorting sheets that exist on the server
	SORT_INVENTORY = 0,
	SORT_EQUIPING,
	SORT_GUIDE_WORLD,
	SORT_TOP,
	NUM_SORT_TAB,

	// type of decorations
	DECORATIONS_HOUSE = 0,
	DECORATIONS_GUILD_HOUSE,
};

// todo use template class 
class PotionTools
{
public:
	class Heal
	{
		int m_ItemID {};
		std::string m_Effect {};
		int m_Recovery {};
		int m_Time {};

	public:
		Heal() = delete;
		Heal(int ItemID, std::string Effect, int Recovery, int Time) : m_ItemID(ItemID), m_Effect(Effect), m_Recovery(Recovery), m_Time(Time) {}

		static const Heal* getHealInfo(int ItemID)
		{
			auto p = std::find_if(m_PotionHealthInfo.begin(), m_PotionHealthInfo.end(), [ItemID](const Heal& p){ return p.m_ItemID == ItemID; });
			return p != m_PotionHealthInfo.end() ? p : nullptr;
		}
		static std::initializer_list<Heal>& getList() { return m_PotionHealthInfo; }

		int getItemID() const { return m_ItemID; }
		const char* getEffect() const { return m_Effect.c_str(); }
		int getRecovery() const { return m_Recovery; }
		int getTime() const { return m_Time; }
	};

private:
	inline static std::initializer_list<Heal> m_PotionHealthInfo
	{
		{ itTinyHealthPotion, "TinyHP", 7, 15 }
	};
};

// Define an enum for the different levels of guild rank access
enum GuildRankAccess
{
	RIGHTS_LEADER = -1,           // Highest level of access, reserved for guild leader
	RIGHTS_DEFAULT = 0,           // Default level of access for new members
	RIGHTS_INVITE_KICK,           // Access to invite and kick members
	RIGHTS_UPGRADES_HOUSE,        // Access to upgrade guild house
	RIGHTS_FULL,                  // Full access to all guild functions
	RIGHTS_NUM,                   // Total number of access levels
};

// broadcast priority
enum class BroadcastPriority
{
	LOWER,
	GAME_BASIC_STATS,
	GAME_INFORMATION,
	GAME_PRIORITY,
	GAME_WARNING,
	MAIN_INFORMATION,
	TITLE_INFORMATION,
	VERY_IMPORTANT,
};

// spawn types
enum SpawnTypes
{
	SPAWN_HUMAN = 0,        // Spawn a human player
	SPAWN_BOT = 1,          // Spawn a bot player
	SPAWN_HUMAN_TREATMENT = 2,   // Spawn a human player in a safe location
	SPAWN_HUMAN_PRISON = 3, // Spawn a human prison
	SPAWN_NUM               // The total number of spawn types available
};

// bot types
enum BotsTypes
{
	TYPE_BOT_MOB = 1,       // type for mob bots
	TYPE_BOT_QUEST = 2,     // type for quest bots
	TYPE_BOT_NPC = 3,       // type for NPC bots
	TYPE_BOT_FAKE = 4,      // type for fake bots
	TYPE_BOT_EIDOLON = 5,   // type for eidolon bots
	TYPE_BOT_QUEST_MOB = 6, // type for quest mob bots
};

// save types
enum SaveType
{
	SAVE_ACCOUNT,			// Save Login Password Data
	SAVE_STATS,				// Save Stats Level Exp and other this type
	SAVE_UPGRADES,			// Save Upgrades Damage and other this type
	SAVE_PLANT_DATA,		// Save Plant Account
	SAVE_MINER_DATA,		// Save Mining Account
	SAVE_GUILD_DATA,		// Save Guild Data
	SAVE_SOCIAL_STATUS,		// Save Social status
	SAVE_POSITION,			// Save Position Player
	SAVE_LANGUAGE,			// Save Language Client
	SAVE_TIME_PERIODS,		// Save Time Periods
};

// world day types
enum DayType
{
	NIGHT_TYPE = 0,
	MORNING_TYPE = 1,
	DAY_TYPE = 2,
	EVENING_TYPE = 3
};

/*
	Basic kernel server settings
	This is where the most basic server settings are stored
*/
enum
{
	MAX_DROPPED_FROM_MOBS = 5,  // maximum number of items that can be dropped by mobs
};

enum CDataList
{
	MMO_DATA_INVENTORY_INFORMATION = 0,
};

// skin data
struct CTeeInfo
{
	char m_aSkinName[64]; // name of the skin
	int m_UseCustomColor; // flag indicating whether custom colors are used
	int m_ColorBody; // color value for the body part
	int m_ColorFeet; // color value for the feet part
};

// Enum class declaration for different attribute types
enum class AttributeGroup : int
{
	Tank,      // Tank attribute
	Healer,    // Healer attribute
	Dps,       // Damage Per Second attribute
	Weapon,    // Weapon attribute
	Hardtype,  // Hard type attribute
	Job,       // Job attribute
	Other,     // Other attribute
};

// Attribute context
enum class AttributeIdentifier : int
{
	SpreadShotgun = 1,           // Attribute identifier for spread shotgun
	SpreadGrenade = 2,           // Attribute identifier for spread grenade
	SpreadRifle = 3,             // Attribute identifier for spread rifle
	DMG = 4,                     // Attribute identifier for damage
	AttackSPD = 5,               // Attribute identifier for attack speed
	CritDMG = 6,                 // Attribute identifier for critical damage
	Crit = 7,                    // Attribute identifier for critical chance
	HP = 8,                      // Attribute identifier for health points
	Lucky = 9,                   // Attribute identifier for luck
	MP = 10,                     // Attribute identifier for mana points
	Vampirism = 11,              // Attribute identifier for vampirism
	AmmoRegen = 12,              // Attribute identifier for ammo regeneration
	Ammo = 13,                   // Attribute identifier for ammo
	Efficiency = 14,             // Attribute identifier for efficiency
	Extraction = 15,             // Attribute identifier for extraction
	HammerDMG = 16,              // Attribute identifier for hammer damage
	GunDMG = 17,                 // Attribute identifier for gun damage
	ShotgunDMG = 18,             // Attribute identifier for shotgun damage
	GrenadeDMG = 19,             // Attribute identifier for grenade damage
	RifleDMG = 20,               // Attribute identifier for rifle damage
	LuckyDropItem = 21,          // Attribute identifier for lucky drop item
	EidolonPWR = 22,             // Attribute identifier for eidolon power
	ATTRIBUTES_NUM,              // The number of total attributes
};

using ByteArray = std::basic_string<std::byte>;
namespace Tools
{
	namespace Aesthetic
	{
		namespace Impl
		{
			struct AestheticImpl
			{
				AestheticImpl() = default;
				AestheticImpl(const char* pUnique, const char* pRUnique, const char* pSnake, int SnakesIter, bool Post)
				{
					if(pSnake != nullptr)
					{
						m_Detail.m_Post = Post;
						m_Detail.m_SnakesIter = SnakesIter;
						str_copy(m_Detail.aBufSnakeIter, pSnake, sizeof(m_Detail.aBufSnakeIter));
						for(int i = 0; i < SnakesIter; i++) { str_append(m_Detail.aBufSnake, pSnake, sizeof(m_Detail.aBufSnake)); }
						if(Post && m_Detail.aBufSnake[0] != '\0') { str_utf8_reverse(m_Detail.aBufSnake); }
					}
					if(pUnique != nullptr) { str_append(m_Detail.aBufUnique, pUnique, sizeof(m_Detail.aBufUnique)); }
					if(pRUnique != nullptr) { str_append(m_Detail.aBufRUnique, pRUnique, sizeof(m_Detail.aBufRUnique)); }
					if(Post) { str_format(m_aData, sizeof(m_aData), "%s%s%s", m_Detail.aBufRUnique, m_Detail.aBufSnake, m_Detail.aBufUnique); }
					else { str_format(m_aData, sizeof(m_aData), "%s%s%s", m_Detail.aBufUnique, m_Detail.aBufSnake, m_Detail.aBufRUnique); }
				}
				struct Detail
				{
					char aBufUnique[64] {};
					char aBufSnake[128] {};
					char aBufRUnique[64] {};
					char aBufSnakeIter[64] {};
					int m_SnakesIter {};
					bool m_Post {};
				};
				Detail m_Detail {};
				char m_aData[256] {};
			};

			struct CompareAestheticDetail
			{
				bool operator()(const AestheticImpl::Detail& D1, const char* pUnique, const char* pRUnique, const char* pSnake, int SnakesIter, bool Post) const
				{
					bool cUnique = !pUnique || std::string(D1.aBufUnique) == pUnique;
					bool cRUnique = !pRUnique || std::string(D1.aBufRUnique) == pRUnique;
					bool cSnake = !pSnake || std::string(D1.aBufSnakeIter) == pSnake;
					return cUnique && cRUnique && cSnake && D1.m_SnakesIter == SnakesIter && D1.m_Post == Post;
				}
			};
			inline ska::flat_hash_set<AestheticImpl*> m_aResultCollection {};

			inline AestheticImpl* AestheticText(const char* pUnique, const char* pRUnique, const char* pSnake, int SnakesIter, bool Post)
			{
				const auto iter = std::find_if(m_aResultCollection.begin(), m_aResultCollection.end(),
					[&](const AestheticImpl* pAest)
				{
					return CompareAestheticDetail {}(pAest->m_Detail, pUnique, pRUnique, pSnake, SnakesIter, Post);
				});
				if(iter == m_aResultCollection.end())
				{
					auto* pData = new AestheticImpl(pUnique ? pUnique : "\0", pRUnique ? pRUnique : "\0", pSnake ? pSnake : "\0", SnakesIter, Post);
					m_aResultCollection.emplace(pData);
					return pData;
				}
				return *iter;
			}
		};

		/* BORDURES */
		// Example: ┏━━━━━ ━━━━━┓
		inline const char* B_DEFAULT_TOP(int iter, bool post) {
			return Impl::AestheticText(post ? "\u2513" : "\u250F", nullptr, "\u2501", iter, post)->m_aData;
		}
		// Example: ───※ ·· ※───
		inline const char* B_PILLAR(int iter, bool post) {
			return Impl::AestheticText(nullptr, post ? "\u00B7 \u203B" : "\u203B \u00B7", "\u2500", iter, post)->m_aData;
		}
		// Example: ✯¸.•*•✿✿•*•.¸✯
		inline const char* B_FLOWER(bool post) {
			return Impl::AestheticText(post ? "\u273F\u2022*\u2022.\u00B8\u272F" : "\u272F\u00B8.\u2022*\u2022\u273F", nullptr, nullptr, 0, false)->m_aData;
		}
		// Example: ──⇌ • • ⇋──
		inline const char* B_CONFIDENT(int iter, bool post) {
			return Impl::AestheticText(nullptr, post ? "\u2022 \u21CB" : "\u21CC \u2022", "\u2500", iter, post)->m_aData;
		}
		// Example: •·.·''·.·•Text•·.·''·.·
		inline const char* B_IRIDESCENT(int iter, bool post) {
			return Impl::AestheticText(post ? "''\u00B7.\u00B7" : "\u2022\u00B7.\u00B7''", "\u2022", "'\u00B7.\u00B7'", iter, post)->m_aData;
		}

		/* LINES */
		// Example: ━━━━━━
		inline const char* L_DEFAULT(int iter) {
			return Impl::AestheticText(nullptr, nullptr, "\u2501", iter, false)->m_aData;
		}
		// Example: ︵‿︵‿
		inline const char* L_WAVES(int iter, bool post) {
			return Impl::AestheticText(nullptr, nullptr, "\uFE35\u203F", iter, post)->m_aData;
		}

		/* WRAP LINES */
		// Example:  ────⇌ • • ⇋────
		inline std::string SWL_CONFIDENT(int iter) {
			return std::string(B_CONFIDENT(iter, false)) + std::string(B_CONFIDENT(iter, true));
		}
		// Example: ───※ ·· ※───
		inline std::string SWL_PILAR(int iter) {
			return std::string(B_PILLAR(iter, false)) + std::string(B_PILLAR(iter, true));
		}

		/* QUOTES */
		// Example: -ˏˋ Text here ˊˎ
		inline const char* Q_DEFAULT(bool post) {
			return Impl::AestheticText(post ? "\u02CA\u02CE" : "-\u02CF\u02CB", nullptr, nullptr, 0, post)->m_aData;
		}

		/* SYMBOL SMILIES */
		// Example: 『•✎•』
		inline const char* S_EDIT(const char* pBody) {
			return Impl::AestheticText(std::string("\u300E " + std::string(pBody) + " \u300F").c_str(), nullptr, nullptr, 0, false)->m_aData;
		}
		// Example: ᴄᴏᴍᴘʟᴇᴛᴇ!
		inline const char* S_COMPLETE() {
			return Impl::AestheticText("\u1D04\u1D0F\u1D0D\u1D18\u029F\u1D07\u1D1B\u1D07!", nullptr, nullptr, 0, false)->m_aData;
		}
	}

	namespace String
	{
		inline std::string progressBar(int max_value, int current_value, int step, std::string UTF_fill_symbol, std::string UTF_empty_symbol)
		{
			std::string ProgressBar;
			int numFilled = current_value / step;
			int numEmpty = max_value / step - numFilled;
			ProgressBar.reserve(numFilled + numEmpty);

			for(int i = 0; i < numFilled; i++)
				ProgressBar += UTF_fill_symbol;

			for(int i = 0; i < numEmpty; i++)
				ProgressBar += UTF_empty_symbol;

			return ProgressBar;
		}
	}

	namespace Json
	{
		// Define a static function called parseFromString that takes in a string Data and a callback function pFuncCallback as parameters
		inline void parseFromString(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
		{
			// Check data empty
			if(!Data.empty())
			{
				try
				{
					// Parse the input string into a nlohmann::json object called JsonData
					nlohmann::json JsonData = nlohmann::json::parse(Data);

					// Call the callback function with JsonData as the parameter
					pFuncCallback(JsonData);
				}
				// Catch any exceptions thrown during the parsing process and handle them
				catch(nlohmann::json::exception& s)
				{
					// Output the error message to the debug log
					char aBufError[2048];
					str_format(aBufError, sizeof(aBufError), "[json parse] Invalid json: %s", s.what());
					dbg_assert(false, aBufError);
				}
			}
		}
	}

	namespace Files
	{
		enum Result : int
		{
			ERROR_FILE,
			SUCCESSFUL,
		};

		inline Result loadFile(const char* pFile, ByteArray* pData)
		{
			IOHANDLE File = io_open(pFile, IOFLAG_READ);
			if(!File)
				return ERROR_FILE;

			pData->resize((unsigned)io_length(File));
			io_read(File, pData->data(), (unsigned)pData->size());
			io_close(File);
			return SUCCESSFUL;
		}

		inline Result deleteFile(const char* pFile)
		{
			int Result = fs_remove(pFile);
			return Result == 0 ? SUCCESSFUL : ERROR_FILE;
		}

		inline Result saveFile(const char* pFile, const void* pData, unsigned size)
		{
			// delete old file
			deleteFile(pFile);

			IOHANDLE File = io_open(pFile, IOFLAG_WRITE);
			if(!File)
				return ERROR_FILE;

			io_write(File, pData, size);
			io_close(File);
			return SUCCESSFUL;
		}

	}
}

// This class is a template class that stores static data shared across multiple worlds.
// It is intended to be inherited by other classes.
class _StoreMultiworldIdentifiableStaticData
{
	// This pointer stores the instance of the server class.
	// It is declared as inline static to allow for its initialization outside of the class.
	inline static class IServer* m_pServer {};

public:
	// This method returns the instance of the server class.
	class IServer* Server() const { return m_pServer; }

	// This method initializes the instance of the server class.
	static void Init(IServer* pServer) { m_pServer = pServer; }
};

template < typename T >
class MultiworldIdentifiableStaticData : public _StoreMultiworldIdentifiableStaticData
{
protected:
	// This static pointer stores the shared static data.
	// It is declared as inline static to allow for its initialization outside of the class.
	static inline T m_pData {};

public:
	// This method returns the shared static data.
	static T& Data() { return m_pData; }
};

#endif
