-- phpMyAdmin SQL Dump
-- version 5.2.0
-- https://www.phpmyadmin.net/
--
-- Хост: 127.0.0.1
-- Время создания: Ноя 05 2022 г., 11:13
-- Версия сервера: 10.4.24-MariaDB
-- Версия PHP: 8.1.5

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- База данных: `test2`
--

-- --------------------------------------------------------

--
-- Структура таблицы `enum_behavior_mobs`
--

CREATE TABLE `enum_behavior_mobs` (
  `ID` int(11) NOT NULL,
  `Behavior` varchar(32) NOT NULL DEFAULT 'Standard'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `enum_behavior_mobs`
--

INSERT INTO `enum_behavior_mobs` (`ID`, `Behavior`) VALUES
(3, 'Sleepy'),
(2, 'Slime'),
(1, 'Standard');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_effects_list`
--

CREATE TABLE `enum_effects_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(16) CHARACTER SET utf8mb4 DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_effects_list`
--

INSERT INTO `enum_effects_list` (`ID`, `Name`) VALUES
(3, 'Fire'),
(2, 'Poison'),
(1, 'Slowdown');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_items_functional`
--

CREATE TABLE `enum_items_functional` (
  `FunctionID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_items_functional`
--

INSERT INTO `enum_items_functional` (`FunctionID`, `Name`) VALUES
(-1, 'Not have function'),
(0, 'Equip hammer(Only equip type)'),
(1, 'Equip gun(Only equip type)'),
(2, 'Equip shotgun(Only equip type)'),
(3, 'Equip grenade(Only equip type)'),
(4, 'Equip rifle(Only equip type)'),
(5, 'Equip miner(Only equip type)'),
(6, 'Equip rake(Only equip type)'),
(7, 'Equip armor(Only equip type)'),
(8, 'Once use item x1'),
(9, 'Several times use item x99'),
(10, 'Settings(Only settings or modules type)'),
(11, 'Plants item'),
(12, 'Mining item');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_items_types`
--

CREATE TABLE `enum_items_types` (
  `TypeID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_items_types`
--

INSERT INTO `enum_items_types` (`TypeID`, `Name`) VALUES
(0, 'Invisible'),
(1, 'Useds'),
(2, 'Crafts'),
(3, 'Modules'),
(4, 'Others'),
(5, 'Settings'),
(6, 'Equipping'),
(7, 'Decorations'),
(8, 'Potions');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_quest_interactive`
--

CREATE TABLE `enum_quest_interactive` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_quest_interactive`
--

INSERT INTO `enum_quest_interactive` (`ID`, `Name`) VALUES
(1, 'Randomly accept or refuse with the item'),
(2, 'Pick up items that NPC will drop.'),
(1, 'Randomly accept or refuse with the item'),
(2, 'Pick up items that NPC will drop.');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_worlds`
--

CREATE TABLE `enum_worlds` (
  `WorldID` int(11) NOT NULL,
  `Name` varchar(32) CHARACTER SET utf8 NOT NULL,
  `RespawnWorld` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_worlds`
--

INSERT INTO `enum_worlds` (`WorldID`, `Name`, `RespawnWorld`) VALUES
(0, 'Port Skandia', NULL),
(1, 'Logging Site', 1),
(2, 'Test zone', 2),
(3, 'Abandoned mine', 2),
(4, 'Elfia home room', 2),
(5, 'Elfinia occupation of goblins', 5),
(6, 'Elfinia Abandoned mine', NULL),
(7, 'Diana home room', 2),
(8, 'Noctis Resonance', NULL),
(9, 'Departure', 9),
(10, 'Underwater of Neptune', 10),
(11, 'Yugasaki', 11);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts`
--

CREATE TABLE `tw_accounts` (
  `ID` int(11) NOT NULL,
  `Username` varchar(64) NOT NULL,
  `Password` varchar(64) NOT NULL,
  `PasswordSalt` varchar(64) DEFAULT NULL,
  `RegisterDate` varchar(64) NOT NULL,
  `LoginDate` varchar(64) NOT NULL DEFAULT 'First log in',
  `RegisteredIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `LoginIP` varchar(64) NOT NULL DEFAULT '0.0.0.0',
  `Language` varchar(8) NOT NULL DEFAULT 'en'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_aethers`
--

CREATE TABLE `tw_accounts_aethers` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `AetherID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_data`
--

CREATE TABLE `tw_accounts_data` (
  `ID` int(11) NOT NULL,
  `Nick` varchar(32) NOT NULL,
  `DiscordID` varchar(64) NOT NULL DEFAULT 'null',
  `WorldID` int(11) DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `GuildID` int(11) DEFAULT NULL,
  `GuildDeposit` int(11) NOT NULL DEFAULT 0,
  `GuildRank` int(11) DEFAULT NULL,
  `Upgrade` int(11) NOT NULL DEFAULT 0,
  `SpreadShotgun` int(11) NOT NULL DEFAULT 3,
  `SpreadGrenade` int(11) NOT NULL DEFAULT 1,
  `SpreadRifle` int(11) NOT NULL DEFAULT 1,
  `Dexterity` int(11) NOT NULL DEFAULT 0,
  `CriticalHit` int(11) NOT NULL DEFAULT 0,
  `DirectCriticalHit` int(11) NOT NULL DEFAULT 0,
  `Hardness` int(11) NOT NULL DEFAULT 0,
  `Tenacity` int(11) NOT NULL DEFAULT 0,
  `Lucky` int(11) NOT NULL DEFAULT 0,
  `Piety` int(11) NOT NULL DEFAULT 0,
  `Vampirism` int(11) NOT NULL DEFAULT 0,
  `AmmoRegen` int(11) NOT NULL DEFAULT 0,
  `Ammo` int(11) NOT NULL DEFAULT 0,
  `Efficiency` int(11) NOT NULL DEFAULT 0,
  `Extraction` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_farming`
--

CREATE TABLE `tw_accounts_farming` (
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `Upgrade` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_items`
--

CREATE TABLE `tw_accounts_items` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Value` int(11) NOT NULL,
  `Settings` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL,
  `Durability` int(11) NOT NULL DEFAULT 100,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_mailbox`
--

CREATE TABLE `tw_accounts_mailbox` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) DEFAULT NULL,
  `ItemValue` int(11) DEFAULT NULL,
  `Enchant` int(11) DEFAULT NULL,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(64) NOT NULL,
  `UserID` int(11) NOT NULL,
  `IsRead` tinyint(4) NOT NULL DEFAULT 0,
  `FromSend` varchar(32) NOT NULL DEFAULT 'Game'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_mining`
--

CREATE TABLE `tw_accounts_mining` (
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `Upgrade` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_quests`
--

CREATE TABLE `tw_accounts_quests` (
  `ID` int(11) NOT NULL,
  `QuestID` int(11) DEFAULT NULL,
  `UserID` int(11) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_skills`
--

CREATE TABLE `tw_accounts_skills` (
  `ID` int(11) NOT NULL,
  `SkillID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL,
  `UsedByEmoticon` int(11) DEFAULT -1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_aethers`
--

CREATE TABLE `tw_aethers` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL DEFAULT 'Teleport name',
  `WorldID` int(11) NOT NULL,
  `TeleX` int(11) NOT NULL,
  `TeleY` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_aethers`
--

INSERT INTO `tw_aethers` (`ID`, `Name`, `WorldID`, `TeleX`, `TeleY`) VALUES
(1, 'Logging Site', 1, 5857, 1041),
(2, 'Center', 0, 8003, 7089),
(3, 'East', 0, 3194, 7441),
(4, 'Craftsman', 0, 9412, 8465),
(5, 'West', 0, 12385, 6833);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_attributs`
--

CREATE TABLE `tw_attributs` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `FieldName` varchar(32) DEFAULT NULL,
  `Price` int(11) NOT NULL,
  `Type` int(11) NOT NULL COMMENT '0.tank1.healer2.dps3.weapon4.hard5.jobs 6. others',
  `Divide` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_attributs`
--

INSERT INTO `tw_attributs` (`ID`, `Name`, `FieldName`, `Price`, `Type`, `Divide`) VALUES
(1, 'Shotgun Spread', 'SpreadShotgun', 100, 3, 0),
(2, 'Grenade Spread', 'SpreadGrenade', 100, 3, 0),
(3, 'Rifle Spread', 'SpreadRifle', 100, 3, 0),
(4, 'Strength', 'unfield', 0, 4, 10),
(5, 'Dexterity', 'Dexterity', 1, 2, 5),
(6, 'Crit Dmg', 'CriticalHit', 1, 2, 5),
(7, 'Direct Crit Dmg', 'DirectCriticalHit', 1, 2, 5),
(8, 'Hardness', 'Hardness', 1, 0, 5),
(9, 'Lucky', 'Lucky', 1, 0, 5),
(10, 'Piety', 'Piety', 1, 1, 5),
(11, 'Vampirism', 'Vampirism', 1, 1, 5),
(12, 'Ammo Regen', 'AmmoRegen', 1, 3, 5),
(13, 'Ammo', 'Ammo', 30, 3, 0),
(14, 'Efficiency', NULL, -1, 5, 0),
(15, 'Extraction', NULL, -1, 5, 0),
(16, 'Hammer Power', NULL, -1, 4, 10),
(17, 'Gun Power', NULL, -1, 4, 10),
(18, 'Shotgun Power', NULL, -1, 4, 10),
(19, 'Grenade Power', NULL, -1, 4, 10),
(20, 'Rifle Power', NULL, -1, 4, 10),
(21, 'Lucky items', NULL, -1, 6, 5);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_auction_items`
--

CREATE TABLE `tw_auction_items` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `ItemValue` int(11) NOT NULL,
  `Price` int(11) NOT NULL,
  `UserID` int(11) NOT NULL DEFAULT 0,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `Time` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_bots_info`
--

CREATE TABLE `tw_bots_info` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Bot name',
  `JsonTeeInfo` varchar(128) NOT NULL DEFAULT '{ "skin": "default", 	"custom_color": 0, 	"color_body": 0, 	"color_feer": 0}',
  `SlotHammer` int(11) DEFAULT 2,
  `SlotGun` int(11) DEFAULT 3,
  `SlotShotgun` int(11) DEFAULT 4,
  `SlotGrenade` int(11) DEFAULT 5,
  `SlotRifle` int(11) DEFAULT 6,
  `SlotArmor` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_bots_info`
--

INSERT INTO `tw_bots_info` (`ID`, `Name`, `JsonTeeInfo`, `SlotHammer`, `SlotGun`, `SlotShotgun`, `SlotGrenade`, `SlotRifle`, `SlotArmor`) VALUES
(5, 'Pig', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"pinky\"}', 2, NULL, NULL, NULL, NULL, NULL),
(6, 'Pig Queen', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"pinky\"}', 2, 3, NULL, 5, NULL, NULL),
(7, 'Skill master', '{\"color_body\":9895735,\"color_feet\":9633571,\"custom_color\":1,\"skin\":\"jeet\"}', 2, 3, 4, 5, 6, NULL),
(8, 'Craftsman', '{\"color_body\":8545280,\"color_feet\":8257280,\"custom_color\":1,\"skin\":\"coala\"}', 2, 3, 4, 5, 6, NULL),
(9, 'Nurse', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"tika\"}', 2, 3, 4, 5, 6, NULL),
(10, 'Lady Sheila', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"nanami\"}', 2, 3, 4, 5, 6, NULL),
(11, 'Willie', '{\"color_body\":6029183,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"kitty_redbopp\"}', 2, 3, 4, 5, 6, NULL),
(12, 'Joel', '{\"color_body\":6029183,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cammostripes\"}', 2, 3, 4, 5, 6, NULL),
(13, 'Anita', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Jigglypuff\"}', 2, 3, 4, 5, 6, NULL),
(14, 'Betsy', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"mermydon\"}', 2, 3, 4, 5, 6, NULL),
(15, 'Corey', '{\"color_body\":65280,\"color_feet\":10477151,\"custom_color\":1,\"skin\":\"kintaro_2\"}', 2, 3, 4, 5, 6, NULL),
(16, 'York', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"greensward\"}', 2, 3, 4, 5, 6, NULL),
(17, 'Warren', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"musmann\"}', 2, 3, 4, 5, 6, NULL),
(18, 'Caine', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"mermydon-coala\"}', 2, 3, 4, 5, 6, NULL),
(19, 'Brain', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"nanas\"}', 2, 3, 4, 5, 6, NULL),
(20, 'Bunny', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"bunny\"}', 2, 3, 4, 5, 6, NULL),
(21, 'Workman', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"default\"}', 2, 3, 4, 5, 6, NULL),
(22, 'Lumberjacks', '{\"color_body\":12942592,\"color_feet\":11009792,\"custom_color\":1,\"skin\":\"nanami_glow\"}', 2, 3, 4, 5, 6, NULL),
(23, 'Doctor Cal', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"kitty_redstripe\"}', 2, 3, 4, 5, 6, NULL),
(24, 'Antelope', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Fluffi\"}', 2, 3, 4, 5, 6, NULL),
(25, 'Cyclope', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Beast_1\"}', 2, 3, 4, 5, 6, NULL),
(26, 'Auctionist', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"veteran\"}', 2, 3, 4, 5, 6, NULL),
(27, 'Sunbird', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"amor\"}', 2, 3, NULL, NULL, NULL, NULL),
(28, 'Bloody Bat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":1,\"skin\":\"Bat\"}', 2, 3, 4, 5, 6, NULL),
(29, 'Sneaky Cat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"blackcat\"}', 2, 3, 4, 5, 6, NULL),
(30, 'Dead Miner', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"coffee_hairy\"}', 2, 3, 4, 5, 6, NULL),
(31, 'Shadow Dark', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"darky\"}', 2, 3, 4, 5, 6, NULL),
(32, 'Bat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Bat\"}', 2, 3, 4, 5, 6, NULL);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_bots_mobs`
--

CREATE TABLE `tw_bots_mobs` (
  `ID` int(11) NOT NULL,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Effect` varchar(16) DEFAULT NULL,
  `Behavior` varchar(32) NOT NULL DEFAULT 'Standard',
  `HookDissabled` tinyint(4) NOT NULL DEFAULT 0,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Power` int(11) NOT NULL DEFAULT 10,
  `Spread` int(11) NOT NULL DEFAULT 0,
  `Number` int(11) NOT NULL DEFAULT 1,
  `Respawn` int(11) NOT NULL DEFAULT 1,
  `Boss` tinyint(1) NOT NULL DEFAULT 0,
  `it_drop_0` int(11) DEFAULT NULL,
  `it_drop_1` int(11) DEFAULT NULL,
  `it_drop_2` int(11) DEFAULT NULL,
  `it_drop_3` int(11) DEFAULT NULL,
  `it_drop_4` int(11) DEFAULT NULL,
  `it_drop_count` varchar(64) NOT NULL DEFAULT '[0][0][0][0][0]',
  `it_drop_chance` varchar(64) NOT NULL DEFAULT '[0][0][0][0][0]'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_bots_mobs`
--

INSERT INTO `tw_bots_mobs` (`ID`, `BotID`, `WorldID`, `PositionX`, `PositionY`, `Effect`, `Behavior`, `HookDissabled`, `Level`, `Power`, `Spread`, `Number`, `Respawn`, `Boss`, `it_drop_0`, `it_drop_1`, `it_drop_2`, `it_drop_3`, `it_drop_4`, `it_drop_count`, `it_drop_chance`) VALUES
(1, 20, 1, 1216, 704, NULL, 'Sleepy', 0, 3, 3, 0, 7, 1, 0, 40, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|5|0|0|0|0|'),
(3, 24, 1, 2610, 832, NULL, 'Sleepy', 1, 4, 3, 0, 7, 1, 0, 32, 40, NULL, NULL, NULL, '|2|1|0|0|0|', '|6|5|0|0|0|'),
(4, 27, 2, 9512, 1915, 'Fire', 'Standard', 0, 7, 10, 1, 12, 3, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(5, 28, 3, 2982, 2494, 'Fire', 'Standard', 0, 10, 50, 2, 10, 1, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(6, 29, 3, 4840, 2560, 'Fire', 'Standard', 0, 12, 60, 2, 8, 1, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(7, 30, 3, 1150, 3700, 'Fire', 'Standard', 0, 12, 70, 1, 10, 1, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(8, 32, 3, 1440, 5100, 'Fire', 'Standard', 0, 10, 80, 2, 8, 1, 0, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|'),
(9, 31, 3, 3960, 4595, 'Fire', 'Standard', 0, 15, 800, 3, 1, 1, 1, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|', '|0|0|0|0|0|');

-- --------------------------------------------------------

--
-- Структура таблицы `tw_bots_npc`
--

CREATE TABLE `tw_bots_npc` (
  `ID` int(11) NOT NULL,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `GiveQuestID` int(11) DEFAULT NULL,
  `DialogData` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`DialogData`)),
  `Function` int(11) NOT NULL DEFAULT -1,
  `Static` int(11) NOT NULL,
  `Emote` enum('Pain','Happy','Surprise','Angry','Blink') DEFAULT NULL,
  `Number` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_bots_npc`
--

INSERT INTO `tw_bots_npc` (`ID`, `BotID`, `PosX`, `PosY`, `GiveQuestID`, `DialogData`, `Function`, `Static`, `Emote`, `Number`, `WorldID`) VALUES
(1, 7, 6472, 7569, NULL, '[{\"text\":\"Someone once told me friendship is magic. That\'s ridiculous. You can\'t turn people into frogs with friendship{OR}Hey..what\'s <bot_9> up to? Have you...have you talked to her, by chance?{OR}\",\"emote\":\"normal\"}\n]', -1, 0, '', 1, 0),
(2, 8, 9973, 8561, NULL, '[{\"text\":\"This is a big upgrade from wiping that table all day.{OR}A lot of tenacity and a little bit of luck can go a long way...{OR}<bot_9> seems nice. I should bring her back with me.{OR}\",\"emote\":\"happy\"}\n]', -1, 0, 'Happy', 1, 0),
(3, 9, 2791, 7345, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 0, 'Happy', 1, 0),
(4, 11, 3415, 7505, 1, '[{\"text\":\"[p]The nightmare fatigues you greatly, your thoughts fractured and sluggish. You\'re relieved that the terrible dream was only just that, but you regret not being able to see it to its end.\"},{\"text\":\"[p]The people in your dream seem powerful and important, not at all like the people you know at the small fishing village. How did the dream end? You long to find out...\"},{\"text\":\"<player>, are you zoning out? I thought you said you were supposed to deliver some goods to <bot_10>. You\'re an official garrison member; I shouldn\'t need to remind you to do your job!\"},{\"text\":\"I like you a lot; don\'t let me down. The boss wants to discuss something with me, so go ahead and meed with the Chief\'s Wife.\"},{\"text\":\"[p]You are not sure why or when you fell asleep. Yesterday, you agreed to deliver some merchandise to the Chief\'s Wife. It looks as though you\'ve missed the delivery; you have to apologize to her.\",\"action_step\":1}]', -1, 1, 'Blink', 1, 0),
(5, 14, 9417, 6833, NULL, NULL, -1, 1, 'Happy', 1, 0),
(6, 15, 6280, 6417, NULL, NULL, -1, 1, '', 1, 0),
(7, 9, 5612, 1009, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 0, 'Happy', 1, 1),
(8, 26, 5779, 8369, NULL, NULL, 0, 0, 'Blink', 1, 0),
(9, 9, 1075, 2161, NULL, '[{ \"text\": \"I think you look better this way.{OR}Quit being such a baby! I’ve seen worse.{OR}I keep asking for wine, but all <bot_7> gives me are mugs of ale.\"}]', 0, 1, 'Happy', 1, 2);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_bots_quest`
--

CREATE TABLE `tw_bots_quest` (
  `ID` int(11) NOT NULL,
  `BotID` int(11) NOT NULL DEFAULT -1,
  `QuestID` int(11) NOT NULL DEFAULT -1,
  `Step` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DialogData` longtext DEFAULT NULL,
  `RequiredItemID1` int(11) DEFAULT NULL,
  `RequiredItemID2` int(11) DEFAULT NULL,
  `RewardItemID1` int(11) DEFAULT NULL,
  `RewardItemID2` int(11) DEFAULT NULL,
  `RequiredDefeatMobID1` int(11) DEFAULT NULL,
  `RequiredDefeatMobID2` int(11) DEFAULT NULL,
  `Amount` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|0|',
  `InteractionType` int(11) DEFAULT NULL,
  `InteractionTemp` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_bots_quest`
--

INSERT INTO `tw_bots_quest` (`ID`, `BotID`, `QuestID`, `Step`, `WorldID`, `PosX`, `PosY`, `DialogData`, `RequiredItemID1`, `RequiredItemID2`, `RewardItemID1`, `RewardItemID2`, `RequiredDefeatMobID1`, `RequiredDefeatMobID2`, `Amount`, `InteractionType`, `InteractionTemp`) VALUES
(1, 10, 1, 1, 0, 7897, 7921, '[{\"text\":\"It is now getting late. <player>, this boy is so...\"},{\"text\":\"\\\"<player>, have you forgotten your promise already?\\\"\\n\\n Sheila asks with a gentle smile.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(2, 10, 2, 1, 0, 7897, 7921, '[{\"text\":\"[p]You apologize to <bot_10> and ask if there is a way to make it up to her.\"},{\"text\":\"\\\"Your apology is quite sincere. I forgive you. And I do have another task,\\\" <bot_10> says with a kind smile.\"},{\"text\":\"These boxes of jam just arrived from the port. Please help me deliver them to the merchants in the village.\"},{\"text\":\"Sheila gives a frown only an irritated mother can give.\\n\\n \\\"Also, ask them if they have seen my son, <bot_12>. I have no idea where he is.\\\"\",\"action_step\":1},{\"text\":\"I\'ve heard of rebellions, but <bot_12> has gone too far. It\'s late, and he hasn\'t come home yet. I fear he\'s getting spoiled, and we can\'t have that! He needs to learn a lesson.\"}]', NULL, NULL, 29, NULL, NULL, NULL, '|0|0|3|0|0|0|', NULL, NULL),
(3, 13, 2, 2, 0, 8141, 7089, '[{\"text\":\"This is so unusual. So you\'re helping <bot_10> deliver goods today? I suppose you are indeed a real garrison member. Keep up the good work.\",\"action_step\":1},{\"text\":\"From now on, everyone will treat you as an adult. Perhaps you\'ll travel the world, meeting maharajas and princesses. Or maybe you\'ll just live and work in this fishing port all your life. You know, either way.\"},{\"text\":\"Just remember this: in the outside world, you can buy different potions at various drugstores. You might find some other special items there, too.\"},{\"text\":\"[p]You ask <bot_13> if she has seen <bot_12>.\"},{\"text\":\"<bot_12> didn\'t come to the marketplace today. It\'s a bit odd; he usually stops by.\"}]', 29, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(4, 14, 2, 2, 0, 9255, 6801, '[{\"text\":\"\\\"<player>, would you like to buy a piece of <item_28>? We carry the armor in all sizes, and we... what is this?\\\"\\n\\n <bot_14> takes the jar of jam and examines it suspiciously.\",\"action_step\":1},{\"text\":\"\\\"Wait, I clearly told <bot_13> that I don\'t want any jam. It\'s too sweet. I like my jam salty and sour!\\\"\\n\\n <bot_14> huffs.\"},{\"text\":\"Well, let\'s forget about the jam. Why don\'t you treat yourself to some new armor? Being a garrison member in a fishing village is not exactly a dangerous job, but from time to time, you\'ll need to clear out some monsters, and you\'ll be happy to have steel between their claws and your heart!\"},{\"text\":\"[p]You ask <bot_14> if she has seen <bot_12>.\"},{\"text\":\"I saw him yesterday, but not today.\"}]', 29, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(5, 15, 2, 2, 0, 6561, 6449, '[{\"text\":\"\\\"Jam! At last! My toast has been going naked!\\\"\\n\\n <bot_15> pops open the jar, dips in his fingers, and slurps up the jelly eagerly.\",\"action_step\":1},{\"text\":\"\\\"Mmm! <bot_13> was right. The jam is perfectly sweet and tart. Would you like some? I promise my fingers are clean.\\\"\\n\\n <bot_15> follows your gaze to the newly produced weapons.\"},{\"text\":\"Impressive, right? These weapons all come from abroad! This one here, for instance, is the <item_3>, a symbol of power. And over there is the <item_5>.\"},{\"text\":\"I\'ll give you a tip, jelly kid. When you have time, just visit weapon shops in different places. You might find exceptionally good weaponry at reasonable prices!\"},{\"text\":\"[p]You ask <bot_15> if he has seen <bot_12>.\"},{\"text\":\"<bot_12>? No, I haven\'t seen him today.\"}]', 29, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|0|', NULL, NULL),
(6, 10, 2, 3, 0, 7897, 7921, '[{\"text\":\"\\\"Have you delivered the jam? You\'ve truly grown; it\'s time for you to start saving your money.\\\"\\n\\n <bot_10> places some gleaming coins in your palm and closes your fingers around them.\"},{\"text\":\"Did any of the merchants see <bot_12>?\"},{\"text\":\"[p]You shake your head and tell her that none of the merchants have seen him today. Hearing this, <bot_10>\'s irritation starts to turn to worry.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(10, 10, 3, 1, 0, 7726, 7921, '[{\"text\":\"\\\"Hasn\'t anyone seen <bot_12>? Where has that boy gone?\\\"\\n\\n <bot_10> glances around anxiously.\"},{\"text\":\"\\\"He can\'t have gone too far...\\\"\\n\\n She hangs her head, looking despondently at the <item_30> in her hand.\"},{\"text\":\"I ought to go look for him. Could you bring this <item_30> to my husband, <bot_16>? When you see him, ask him to find you a suitable job.\",\"action_step\":1}]', NULL, NULL, 30, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(11, 16, 3, 2, 0, 6558, 7569, '[{\"text\":\"[bot_17]I must report to you, as you are the Village Chief. The sea has begun to churn; the situation grows more unstable by the day. I can only guide the ships into the port with light signals, but...\"},{\"text\":\"<bot_16> frowns, pacing anxiously as he chats with <bot_17>, the lighthouse manager.\"},{\"text\":\"\\\"Got it. The monster population in the forest has skyrocketed, too. Things are getting rough around here.\\\"\\n\\n <bot_16> looks over at you, eyeing the <item_30>.\"},{\"text\":\"<player>, where is my wife, and why are you delivering my lunch?\",\"action_step\":1}]', 30, NULL, 38, NULL, NULL, NULL, '|1|0|1|0|0|0|', NULL, NULL),
(12, 17, 3, 2, 0, 6418, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(13, 16, 4, 1, 0, 6558, 7569, '[{\"text\":\"\\\"<bot_10> dotes on him. She spoils him. He can play alone once in a while, you know? Who knows, he might grow up to be a great conqueror - if he stops being so darn soft!\\\"\\n\\n <bot_16> chortles.\"},{\"text\":\"As for your work... well, I really don\'t have much for you to do.\"},{\"text\":\"[bot_17]<bot_16>, you said you needed someone to replace one of <bot_18>\'s... less enthusiastic workers. How could you forget so quickly?\"},{\"text\":\"I didn\'t forget. But working for <bot_18> is no picnic. Very few are willing to work backbreaking hours at the Logging Site, and <bot_18>\'s not exactly Mr. Personality.\"},{\"text\":\"<player>, would you like to try working at the Logging Site? The salary\'s not bad.\",\"action_step\":1},{\"text\":\"\\\"This boy <bot_19> is a fool! He only makes work harder for me! Unless we\'re short workers, I\'d fire him right away!\\\"\\n\\n <bot_18> grumbles to himself. It seems that working at the Logging Site is quite arduous.\"}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(14, 17, 4, 1, 0, 6428, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(15, 18, 4, 1, 0, 6480, 7569, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(16, 18, 4, 2, 1, 6256, 1105, '[{\"text\":\"Did <bot_16> ask you to come here? Excellent. I was just wondering what the Garrison Members in the village normally do during the day. They just hang about the forest. They might as well be here doing something productive for once.\"},{\"text\":\"Speaking of which, have you seen <bot_19> lately? He hasn\'t come to work in days!\"},{\"text\":\"[p]You shake your head and explain that you don\'t know him.\"},{\"text\":\"Well, there\'s no time to lose. you should just start working right away.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(17, 18, 5, 1, 1, 6256, 1105, '[{\"text\":\"Lately, numerous <bot_20>hops have migrated to the Logging Site. The naughty troublemakers make a mess all day long. Workmen complain constantly about picking up their, er, \'jelly beans.\'\"},{\"text\":\"We cannot let those rascally rabbits slow us down. If the wood is not delivered to the village fast enough, construction there will be delayed.\"},{\"text\":\"I\'ve a task for you. Drive the <bot_20>hops out of the Logging Site so the lumberjacks can focus on their work!\",\"action_step\":1},{\"text\":\"\\\"We have three days to deliver to the furniture shop. We cannot be late.\\\"\\n\\n <bot_18> snaps, scanning a list.\"},{\"text\":\"\\\"Not bad for a newbie.\\\"\\n\\n <bot_18> is clearly satisfied by the reduced number of <bot_20>hops.\"},{\"text\":\"This was originally <bot_19>\'s job, but I think you ought to take his place. You\'re more responsible than he ever was. Go to the Garrison Captain. I will speak to him.\"}]', NULL, NULL, NULL, NULL, 20, NULL, '|0|0|0|0|15|0|', NULL, NULL),
(18, 18, 6, 1, 1, 5971, 1041, '[{\"text\":\"\\\"It\'s about time to send the chopped wood back to the village\\\"\\n\\nCaine explains, scanning the sky.\"},{\"text\":\"You must help me finish off today\'s work if you want your proper salary.\"},{\"text\":\"You\'ll find a good deal of wood in the area. I need you to carry it to me.\"},{\"text\":\"<bot_22> typically pile up the chopped wood under another tree. Take a walk around the Logging Site. It shouldn\'t be hard to find the wood.\",\"action_step\":1},{\"text\":\"[p]You pass the collected lumber to Caine.\"},{\"text\":\"You move pretty fast. What about the others?\"}]', 31, NULL, NULL, NULL, NULL, NULL, '|15|0|0|0|0|0|', 2, NULL),
(19, 18, 7, 1, 1, 5971, 1041, '[{\"text\":\"What are those <bot_21> doing now? It\'s late, and they haven\'t brought the wood back yet.\"},{\"text\":\"<player>, everyone says I\'m too harsh. I suppose you can judge for yourself now. Only you have returned with the wood. What are all the others doing out there, picking each other\'s noses?\"},{\"text\":\"\\\"Take this hammer and search the site for anyone sleeping on the job. Give \'em a wake up call they won\'t forget!\\\"\\n\\n <bot_18> snarls, his face flushed with fury.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(20, 21, 7, 2, 1, 5246, 1073, '[{\"text\":\"[p]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(21, 21, 7, 2, 1, 5574, 1009, '[{\"text\":\"[p]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(22, 21, 7, 2, 1, 6507, 1137, '[{\"text\":\"[p]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(23, 21, 7, 2, 1, 3817, 1169, '[{\"text\":\"[p]You started to wake up the <bot_21>.\"},{\"text\":\"I\'m awake, don\'t touch me.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(24, 18, 7, 3, 1, 5727, 1009, '[{\"text\":\"These lazy fools are becoming more and more arrogant. It\'s time they take a pay cut! I doubt they\'ll dare slack off on the job again.\"},{\"text\":\"See for yourself. <player>, who started working here just today, has contributed more to our project in one day than you have in your career. Shame on you!\"},{\"text\":\"[bot_22]I was just taking a break. I didn\'t know I was going to fall asleep. It\'s not a big deal.\"},{\"text\":\"Not much of an excuse, you lazy fool! You only get half a day\'s pay today. End of discussion!\"},{\"text\":\"[bot_22]You\'re such a scrooge! Everyone needs a nap now and again!\"},{\"text\":\"\\\"If you argue again, I\'ll take away your breaks,\\\" <bot_18> growls, glaring.\"},{\"text\":\"[bot_22]Fine, fine. I know I made a mistake. I won\'t do it again.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(25, 22, 7, 3, 1, 5539, 1009, NULL, NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(26, 18, 8, 1, 1, 5727, 1009, '[{\"text\":\"<player>, you have impressed me. I think I can recommend <bot_23> to you, you can make more money with it than working at the <world_1>.\"},{\"text\":\"[p]You nervously explain to <bot_18> that you know nothing about medicine, and you don\'t want to be a burden to the doctor.\"},{\"text\":\"Oh, c\'mon. It can\'t be that hard! You only go to school for, what, eleven, twelve years to become a doctor? No biggie. Just look at it as a learning experience.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(27, 23, 8, 2, 0, 2688, 7345, '[{\"text\":\"Haha, each of those lazy lumberjacks comes to me with the same complaints. Seems like this <bot_18> guy is pretty strict.\"},{\"text\":\"Hello, <player>, how are you?\"},{\"text\":\"[p]*You tell him how you\'re feeling today*\"},{\"text\":\"Mm, okay.\"},{\"text\":\"<bot_18> sent you to me, I know.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(28, 23, 9, 1, 0, 2688, 7345, '[{\"text\":\"At the moment, I can\'t find any physical problems. Maybe you\'re overworked; that can affect your brain, you know. You\'ve got the same symptoms as the other workmen.\"},{\"text\":\"[p]*You consider his reasoning, but it doesn\'t quite make sense. You explain to <bot_23> that perhaps your continuous nightmares have kept you from getting a proper night\'s sleep.*\"},{\"text\":\"I heard <bot_18> sent you here to help out. Luckily, I\'ve got the perfect project for you.\"},{\"text\":\"This should be a simple task for someone like you. Here, take this; it\'ll get your blood flowing.\"},{\"text\":\"You and the other lumberjacks need something - some sort of medicine to refresh your minds and bodies. Something to put some pep back in your step.\"},{\"text\":\"I need you to prepare some <item_32> for me. You can find <bot_24> nearby.\"},{\"text\":\"You need blood from <bot_24>s to make the refreshing medicine. Nothing\'s more invigorating than animal blood, am I right?\",\"action_step\":1},{\"text\":\"[p]*You present <bot_23> with the <item_32> you collected.*\"},{\"text\":\"*<bot_23> crows happily.*\"},{\"text\":\"I\'m going to officially name you as my assistant. When the time is right, I\'ll teach you everything I know! I bet you\'re a fast learner!\"}]', 32, NULL, NULL, NULL, NULL, NULL, '|10|0|0|0|0|0|', NULL, NULL),
(29, 23, 10, 1, 0, 2688, 7345, '[{\"text\":\"To begin with, approach <bot_14>. She has something for you that will help your task.\"},{\"text\":\"[p]Well.\",\"action_step\":1}]', NULL, NULL, NULL, NULL, NULL, NULL, '|0|0|0|0|0|0|', NULL, NULL),
(30, 14, 10, 2, 0, 9255, 6801, '[{\"text\":\"Oh yes, I was just waiting for you. <bot_23> asked me to give you this.\",\"action_step\":1},{\"text\":\"I don\'t know why, but apparently he wants you to get all kinds of herbs for him.\"},{\"text\":\"We used to have Aliya as an herbalist. I\'m relying on you.\"}]', NULL, NULL, 26, NULL, NULL, NULL, '|0|0|1|0|0|0|', NULL, NULL),
(31, 23, 10, 3, 0, 2688, 7345, '[{\"text\":\"\\\"I\'ve almost completely ground up these herbs. We should put some <item_33>s into the medicine and grind them together, too...\\\" \\n\\n<bot_23> suddenly pauses to examine the medicine.\"},{\"text\":\"\\\"I used up the mushrooms last time I prepared a medication. I haven\'t had time to seek them out again.\\\" \\n\\n<bot_23> scratches his head, anxious.\"},{\"text\":\"<player>, looks like I need to ask you to go gather some <item_33>s. They typically grow near tree trunks. Take a look around the area, and I\'m sure you\'ll find \'em.\\n\\n(You can find the location of ores, loot, and mobs in the Wiki tab in the voting).\",\"action_step\":1},{\"text\":\"These mushrooms may well be Skandia\'s most valuable resource. A good deal of caravans come to the port just to buy them. I hear they are also excellent in risotto.\"},{\"text\":\"[p]<bot_23> grins when you hand him the <item_33>s.\"},{\"text\":\"\\\"Perfect! These are Skandia\'s most unique mushrooms.\\\" \\n\\n<bot_23> examines them out one by one.\"},{\"text\":\"\\\"You are truly an excellent assistant,\\\" \\n\\n<bot_23> comments, patting you on the back.\"}]', 33, NULL, NULL, NULL, NULL, NULL, '|32|0|0|0|0|0|', NULL, NULL);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_crafts_list`
--

CREATE TABLE `tw_crafts_list` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) DEFAULT NULL,
  `ItemValue` int(11) NOT NULL,
  `RequiredItems` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL CHECK (json_valid(`RequiredItems`)),
  `Price` int(11) NOT NULL DEFAULT 100,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_crafts_list`
--

INSERT INTO `tw_crafts_list` (`ID`, `ItemID`, `ItemValue`, `RequiredItems`, `Price`, `WorldID`) VALUES
(1, 39, 10, '{\"items\":[{\"id\":40,\"value\":40},{\"id\":33,\"value\":24}]}', 80, 0),
(2, 41, 10, '{\"items\":[{\"id\":40,\"value\":50},{\"id\":33,\"value\":12},{\"id\":32,\"value\":60}]}', 100, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_dungeons`
--

CREATE TABLE `tw_dungeons` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL DEFAULT 'Unknown',
  `Level` int(11) NOT NULL DEFAULT 1,
  `DoorX` int(11) NOT NULL DEFAULT 0,
  `DoorY` int(11) NOT NULL DEFAULT 0,
  `RequiredQuestID` int(11) NOT NULL DEFAULT -1,
  `Story` tinyint(4) NOT NULL DEFAULT 0,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_dungeons`
--

INSERT INTO `tw_dungeons` (`ID`, `Name`, `Level`, `DoorX`, `DoorY`, `RequiredQuestID`, `Story`, `WorldID`) VALUES
(1, 'Abandoned mine', 10, 1105, 1521, -1, 1, 3);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_dungeons_door`
--

CREATE TABLE `tw_dungeons_door` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL DEFAULT 'Write here name dungeon',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `BotID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `tw_dungeons_door`
--

INSERT INTO `tw_dungeons_door` (`ID`, `Name`, `PosX`, `PosY`, `BotID`, `DungeonID`) VALUES
(1, 'AM 1', 4302, 1940, 28, 1),
(2, 'AM 2', 1808, 3600, 29, 1),
(3, 'AM 3', 750, 4850, 30, 1),
(4, 'AM 4', 2255, 4850, 32, 1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_dungeons_records`
--

CREATE TABLE `tw_dungeons_records` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL,
  `Seconds` int(11) NOT NULL,
  `PassageHelp` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds`
--

CREATE TABLE `tw_guilds` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Member name',
  `UserID` int(11) DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Experience` int(11) NOT NULL DEFAULT 0,
  `Bank` int(11) NOT NULL DEFAULT 0,
  `Score` int(11) NOT NULL DEFAULT 0,
  `AvailableSlots` int(11) NOT NULL DEFAULT 2,
  `ChairExperience` int(11) NOT NULL DEFAULT 1,
  `ChairMoney` int(11) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_decorations`
--

CREATE TABLE `tw_guilds_decorations` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `DecoID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `tw_guilds_decorations`
--

INSERT INTO `tw_guilds_decorations` (`ID`, `PosX`, `PosY`, `HouseID`, `DecoID`, `WorldID`) VALUES
(1, 4371, 6337, 1, 12, 0),
(2, 4050, 6337, 1, 12, 0),
(3, 4096, 6283, 1, 11, 0),
(4, 4319, 6283, 1, 11, 0),
(5, 4198, 6276, 1, 10, 0),
(6, 4202, 6351, 1, 12, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_history`
--

CREATE TABLE `tw_guilds_history` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) NOT NULL,
  `Text` varchar(64) NOT NULL,
  `Time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_houses`
--

CREATE TABLE `tw_guilds_houses` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DoorX` int(11) NOT NULL,
  `DoorY` int(11) NOT NULL,
  `TextX` int(11) NOT NULL,
  `TextY` int(11) NOT NULL,
  `Price` int(11) NOT NULL DEFAULT 50000,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_guilds_houses`
--

INSERT INTO `tw_guilds_houses` (`ID`, `GuildID`, `PosX`, `PosY`, `DoorX`, `DoorY`, `TextX`, `TextY`, `Price`, `WorldID`) VALUES
(1, NULL, 4120, 6449, 4521, 6449, 4206, 6307, 80000, 0),
(2, NULL, 9504, 5713, 9187, 5713, 9486, 5612, 80000, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_invites`
--

CREATE TABLE `tw_guilds_invites` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_ranks`
--

CREATE TABLE `tw_guilds_ranks` (
  `ID` int(11) NOT NULL,
  `Access` int(11) NOT NULL DEFAULT 3,
  `Name` varchar(32) NOT NULL DEFAULT 'Rank name',
  `GuildID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_houses`
--

CREATE TABLE `tw_houses` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) DEFAULT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DoorX` int(11) NOT NULL,
  `DoorY` int(11) NOT NULL,
  `Class` varchar(32) NOT NULL DEFAULT 'Class name',
  `Price` int(11) NOT NULL,
  `HouseBank` int(11) NOT NULL,
  `PlantID` int(11) NOT NULL,
  `PlantX` int(11) NOT NULL,
  `PlantY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_houses`
--

INSERT INTO `tw_houses` (`ID`, `UserID`, `PosX`, `PosY`, `DoorX`, `DoorY`, `Class`, `Price`, `HouseBank`, `PlantID`, `PlantX`, `PlantY`, `WorldID`) VALUES
(1, NULL, 8001, 5297, 8294, 5297, 'Default', 40000, 13781, 40, 7499, 5329, 0),
(2, NULL, 8989, 7729, 8704, 7729, 'Default', 40000, 0, 40, 9466, 7761, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_houses_decorations`
--

CREATE TABLE `tw_houses_decorations` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `DecoID` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_houses_decorations`
--

INSERT INTO `tw_houses_decorations` (`ID`, `PosX`, `PosY`, `HouseID`, `DecoID`, `WorldID`) VALUES
(1, 7970, 5230, 1, 12, 0),
(2, 8173, 5176, 1, 12, 0),
(3, 8075, 5205, 1, 10, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_items_list`
--

CREATE TABLE `tw_items_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Item name',
  `Description` varchar(64) NOT NULL DEFAULT 'Item desc',
  `Type` int(11) DEFAULT -1,
  `Function` int(11) DEFAULT -1,
  `InitialPrice` int(11) NOT NULL DEFAULT 100,
  `Desynthesis` int(11) NOT NULL DEFAULT 100,
  `Attribute0` int(11) DEFAULT NULL,
  `Attribute1` int(11) DEFAULT NULL,
  `AttributeValue0` int(11) NOT NULL DEFAULT 0,
  `AttributeValue1` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_items_list`
--

INSERT INTO `tw_items_list` (`ID`, `Name`, `Description`, `Type`, `Function`, `InitialPrice`, `Desynthesis`, `Attribute0`, `Attribute1`, `AttributeValue0`, `AttributeValue1`) VALUES
(1, 'Gold', 'Major currency', 0, -1, 0, 0, 16, NULL, 0, 0),
(2, 'Hammer', 'A normal hammer', 6, 0, 0, 0, 16, 6, 10, 3),
(3, 'Gun', 'Conventional weapon', 6, 1, 100, 0, 17, NULL, 10, 0),
(4, 'Shotgun', 'Conventional weapon', 6, 2, 10, 0, 18, NULL, 5, 0),
(5, 'Grenade', 'Conventional weapon', 6, 3, 10, 0, 19, NULL, 10, 0),
(6, 'Rifle', 'Conventional weapon', 6, 4, 10, 0, 20, NULL, 10, 0),
(7, 'Material', 'Required to improve weapons', 4, 12, 10, 0, NULL, NULL, 0, 0),
(8, 'Ticket guild', 'Command: /gcreate <name>', 4, -1, 10, 0, NULL, NULL, 0, 0),
(9, 'Skill Point', 'Skill point', 0, -1, 10, 0, NULL, NULL, 0, 0),
(10, 'Decoration Armor', 'Decoration for house!', 7, -1, 10, 0, NULL, NULL, 0, 0),
(11, 'Decoration Hearth Elite', 'Decoration for house!', 7, -1, 10, 0, NULL, NULL, 0, 0),
(12, 'Decoration Ninja Elite', 'Decoration for house!', 7, -1, 10, 0, NULL, NULL, 0, 0),
(13, 'Decoration Hearth', 'Decoration for house!', 7, -1, 10, 0, NULL, NULL, 0, 0),
(14, 'Potion mana regen', 'Regenerate +5%, 15sec every sec.\n', 8, 8, 10, 20, NULL, NULL, 0, 0),
(15, 'Potion health regen', 'Regenerate +3% health, 15sec every sec.', 8, 8, 10, 20, NULL, NULL, 0, 0),
(16, 'Capsule survival experience', 'You got 10-50 experience survival', 1, 9, 10, 0, NULL, NULL, 0, 0),
(17, 'Little bag of gold', 'You got 10-50 gold', 1, 9, 10, 0, NULL, NULL, 0, 0),
(18, 'Potion resurrection', 'Resuscitates in the zone where you died!', 8, -1, 10, 0, NULL, NULL, 0, 0),
(19, 'Explosive module for gun', 'It happens sometimes', 3, 10, 10, 0, 17, NULL, 5, 0),
(20, 'Explosive module for shotgun', 'It happens sometimes', 3, 10, 10, 0, 18, NULL, 5, 0),
(21, 'Ticket reset class stats', 'Resets only class stats(Dps, Tank, Healer).', 1, 8, 10, 0, NULL, NULL, 0, 0),
(22, 'Mode PVP', 'Settings game.', 5, 10, 0, 0, NULL, NULL, 0, 0),
(23, 'Ticket reset weapon stats', 'Resets only ammo stats(Ammo).', 1, 8, 10, 0, NULL, NULL, 0, 0),
(24, 'Blessing for discount craft', 'Need dress it, -20% craft price', 8, 8, 10, 0, NULL, NULL, 0, 0),
(25, 'Show equipment description', 'Settings game.', 5, 10, 0, 0, NULL, NULL, 0, 0),
(26, 'Rusty Rake', 'The usual rusty rake.', 6, 6, 10, 50, 15, NULL, 3, 0),
(27, 'Rusty Pickaxe', 'The usual rusty pickaxe.', 6, 5, 10, 50, 14, NULL, 3, 0),
(28, 'Beast Tusk Light Armor', 'Lightweight armor.', 6, 7, 50, 100, 8, NULL, 32, 0),
(29, 'Boxes of jam', '...', 0, -1, 0, 0, NULL, NULL, 0, 0),
(30, 'Lunchbox', '...', 0, -1, 0, 0, NULL, NULL, 0, 0),
(31, 'Wood', '...', 2, 11, 10, 0, NULL, NULL, 0, 0),
(32, 'Antelope blood', '...', 2, -1, 15, 0, NULL, NULL, 0, 0),
(33, 'Wild Forest Mushroom', '...', 2, 11, 5, 1, NULL, NULL, 0, 0),
(34, 'Show critical damage', 'Settings game.', 5, 10, 0, 0, NULL, NULL, 0, 0),
(35, 'Althyk\'s Ring', 'Althyk\'s Ring is an item level 1.', 3, 10, 10, 0, 8, 10, 20, 30),
(36, 'Empyrean Ring', 'Empyrean Ring is an item level 1.', 3, 10, 10, 0, 8, 10, 32, 18),
(37, 'Ring of Fidelity', 'Ring of Fidelity is an item level 1.', 3, 10, 10, 0, 7, 4, 28, 12),
(38, 'Eversharp Ring', 'Eversharp Ring is an item level 1.', 3, 10, 100, 0, 8, NULL, 126, 0),
(39, 'Green Grass Armor', 'Lightweight armor.', 6, 7, 50, 0, 8, NULL, 48, 0),
(40, 'Green grass', '...', 2, 11, 15, 0, NULL, NULL, 0, 0),
(41, 'Bloody Woven Flower', 'Soaked in blood', 3, 10, 50, 0, 10, 13, 40, 3);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_logics_worlds`
--

CREATE TABLE `tw_logics_worlds` (
  `ID` int(11) NOT NULL,
  `MobID` int(11) NOT NULL,
  `Mode` int(11) NOT NULL DEFAULT 0 COMMENT '(1,3) 0 up 1 left',
  `ParseInt` int(11) NOT NULL COMMENT '(2) health (3)itemid key',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL,
  `Comment` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_positions_mining`
--

CREATE TABLE `tw_positions_mining` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Health` int(11) NOT NULL DEFAULT 100,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_positions_plant`
--

CREATE TABLE `tw_positions_plant` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Health` int(11) NOT NULL DEFAULT 100,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_positions_plant`
--

INSERT INTO `tw_positions_plant` (`ID`, `ItemID`, `Level`, `Health`, `PositionX`, `PositionY`, `Distance`, `WorldID`) VALUES
(1, 33, 1, 125, 4940, 1014, 400, 1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_quests_list`
--

CREATE TABLE `tw_quests_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(24) NOT NULL DEFAULT 'Quest name',
  `Money` int(11) NOT NULL,
  `Exp` int(11) NOT NULL,
  `StoryLine` varchar(24) NOT NULL DEFAULT 'Hero'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_quests_list`
--

INSERT INTO `tw_quests_list` (`ID`, `Name`, `Money`, `Exp`, `StoryLine`) VALUES
(1, 'Bad Dreams and Mornings', 30, 25, 'Main'),
(2, 'A Sticky Situation', 30, 25, 'Main'),
(3, 'Boxed Lunch', 35, 30, 'Main'),
(4, 'Looking for Work', 40, 40, 'Main'),
(5, 'What Big Teeth You Have', 40, 40, 'Main'),
(6, 'Log Lugger', 40, 40, 'Main'),
(7, 'The Lazy Lumberjack', 50, 50, 'Main'),
(8, 'Doctor Cal', 60, 60, 'Main'),
(9, 'Collect Antelope Blood', 60, 60, 'Main'),
(10, 'Mushroom Master', 60, 60, 'Main');

-- --------------------------------------------------------

--
-- Структура таблицы `tw_skills_list`
--

CREATE TABLE `tw_skills_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Description` varchar(64) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0 COMMENT '0-Improvements\r\n1-Healer\r\n2-Dps\r\n3-Tank',
  `BoostName` varchar(64) NOT NULL DEFAULT '''name''',
  `BoostValue` int(11) NOT NULL DEFAULT 1,
  `PercentageCost` int(11) NOT NULL DEFAULT 10,
  `PriceSP` int(11) NOT NULL,
  `MaxLevel` int(11) NOT NULL,
  `Passive` tinyint(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_skills_list`
--

INSERT INTO `tw_skills_list` (`ID`, `Name`, `Description`, `Type`, `BoostName`, `BoostValue`, `PercentageCost`, `PriceSP`, `MaxLevel`, `Passive`) VALUES
(1, 'Health turret', 'Creates turret a recovery health ', 1, 'life span', 3, 25, 28, 8, 0),
(2, 'Sleepy Gravity', 'Magnet mobs to itself', 3, 'radius', 20, 25, 28, 10, 0),
(3, 'Craft Discount', 'Will give discount on the price of craft items', 0, '% discount gold for craft item', 1, 0, 28, 50, 1),
(4, 'Proficiency with weapons', 'You can perform an automatic fire', 0, 'can perform an auto fire with all types of weapons', 1, 0, 120, 1, 1),
(5, 'Blessing of God of war', 'The blessing restores ammo', 3, '% recovers ammo within a radius of 800', 25, 50, 28, 4, 0),
(6, 'Attack Teleport', 'An attacking teleport that deals damage to all mobs radius', 2, '% your strength', 25, 10, 100, 4, 0),
(7, 'Cure I', 'Restores HP all nearby target\'s.', 1, '% adds a health bonus', 3, 5, 10, 5, 0),
(8, 'Health regen', 'Throws a buff HP regen all nearby target\'s.\r\n', 1, 'sec adds a buff lifetime', 3, 70, 72, 5, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_voucher`
--

CREATE TABLE `tw_voucher` (
  `ID` int(11) NOT NULL,
  `Code` varchar(32) NOT NULL,
  `Data` text NOT NULL,
  `Multiple` tinyint(1) NOT NULL DEFAULT 0,
  `ValidUntil` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_voucher`
--

INSERT INTO `tw_voucher` (`ID`, `Code`, `Data`, `Multiple`, `ValidUntil`) VALUES
(1, 'VALENTINE2021', '{\r\n	\"exp\": 10000,\r\n	\"items\": [\r\n		{\r\n			\"id\": 17,\r\n			\"value\": 30\r\n		},\r\n		{\r\n			\"id\": 15000,\r\n			\"value\": 1\r\n		}\r\n	]\r\n}', 1, 1614517578);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_voucher_redeemed`
--

CREATE TABLE `tw_voucher_redeemed` (
  `ID` int(11) NOT NULL,
  `VoucherID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `TimeCreated` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_warehouses`
--

CREATE TABLE `tw_warehouses` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT '''Bussines name''',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `Currency` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) NOT NULL,
  `test` set('ARG1','ARG2','ARG3') NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_warehouses`
--

INSERT INTO `tw_warehouses` (`ID`, `Name`, `PosX`, `PosY`, `Currency`, `WorldID`, `test`) VALUES
(1, 'Betsy shop', 9437, 6833, 1, 0, 'ARG2,ARG3'),
(2, 'Weapons from Correy', 6247, 6417, 1, 0, 'ARG2');

-- --------------------------------------------------------

--
-- Структура таблицы `tw_warehouse_items`
--

CREATE TABLE `tw_warehouse_items` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `ItemValue` int(11) NOT NULL,
  `RequiredItemID` int(11) NOT NULL DEFAULT 1,
  `Price` int(11) NOT NULL,
  `UserID` int(11) NOT NULL DEFAULT 0,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `WarehouseID` int(11) DEFAULT NULL,
  `Time` timestamp NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_warehouse_items`
--

INSERT INTO `tw_warehouse_items` (`ID`, `ItemID`, `ItemValue`, `RequiredItemID`, `Price`, `UserID`, `Enchant`, `WarehouseID`, `Time`) VALUES
(4, 3, 1, 1, 140, 0, 0, 2, '2022-10-09 14:27:38'),
(5, 4, 1, 1, 350, 0, 0, 2, '2022-10-09 14:27:38'),
(6, 5, 1, 1, 350, 0, 0, 2, '2022-10-09 14:27:38'),
(7, 6, 1, 1, 400, 0, 0, 2, '2022-10-09 14:27:38'),
(8, 28, 1, 1, 500, 0, 0, 1, '2022-10-09 14:30:00'),
(9, 35, 1, 1, 490, 0, 0, 1, '2022-10-09 14:30:00'),
(10, 36, 1, 1, 520, 0, 0, 1, '2022-10-09 14:30:00'),
(11, 37, 1, 1, 540, 0, 0, 1, '2022-10-09 14:30:00');

-- --------------------------------------------------------

--
-- Структура таблицы `tw_world_swap`
--

CREATE TABLE `tw_world_swap` (
  `ID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) DEFAULT NULL,
  `PositionY` int(11) DEFAULT NULL,
  `RequiredQuestID` int(11) DEFAULT NULL,
  `TwoWorldID` int(11) DEFAULT NULL,
  `TwoPositionX` int(11) DEFAULT NULL,
  `TwoPositionY` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_world_swap`
--

INSERT INTO `tw_world_swap` (`ID`, `WorldID`, `PositionX`, `PositionY`, `RequiredQuestID`, `TwoWorldID`, `TwoPositionX`, `TwoPositionY`) VALUES
(9, 0, 3607, 8105, 3, 1, 6912, 991),
(10, 0, 13744, 6670, NULL, 2, 496, 2000);

--
-- Индексы сохранённых таблиц
--

--
-- Индексы таблицы `enum_behavior_mobs`
--
ALTER TABLE `enum_behavior_mobs`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `Behavior` (`Behavior`);

--
-- Индексы таблицы `enum_effects_list`
--
ALTER TABLE `enum_effects_list`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `Name` (`Name`);

--
-- Индексы таблицы `enum_items_functional`
--
ALTER TABLE `enum_items_functional`
  ADD PRIMARY KEY (`FunctionID`);

--
-- Индексы таблицы `enum_items_types`
--
ALTER TABLE `enum_items_types`
  ADD PRIMARY KEY (`TypeID`);

--
-- Индексы таблицы `enum_quest_interactive`
--
ALTER TABLE `enum_quest_interactive`
  ADD KEY `ID` (`ID`);

--
-- Индексы таблицы `enum_worlds`
--
ALTER TABLE `enum_worlds`
  ADD PRIMARY KEY (`WorldID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Name` (`Name`),
  ADD KEY `SafeZoneWorldID` (`RespawnWorld`),
  ADD KEY `WorldID_2` (`WorldID`);

--
-- Индексы таблицы `tw_accounts`
--
ALTER TABLE `tw_accounts`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `Password` (`Password`),
  ADD KEY `Username` (`Username`);

--
-- Индексы таблицы `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `TeleportID` (`AetherID`);

--
-- Индексы таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `Nick` (`Nick`),
  ADD KEY `MemberID` (`GuildID`),
  ADD KEY `DiscordID` (`DiscordID`),
  ADD KEY `tw_accounts_data_ibfk_3` (`WorldID`),
  ADD KEY `GuildRank` (`GuildRank`),
  ADD KEY `Level` (`Level`),
  ADD KEY `Exp` (`Exp`);

--
-- Индексы таблицы `tw_accounts_farming`
--
ALTER TABLE `tw_accounts_farming`
  ADD PRIMARY KEY (`UserID`),
  ADD UNIQUE KEY `AccountID` (`UserID`);

--
-- Индексы таблицы `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `ItemID` (`ItemID`);

--
-- Индексы таблицы `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `tw_accounts_inbox_ibfk_2` (`ItemID`);

--
-- Индексы таблицы `tw_accounts_mining`
--
ALTER TABLE `tw_accounts_mining`
  ADD PRIMARY KEY (`UserID`),
  ADD UNIQUE KEY `AccountID` (`UserID`);

--
-- Индексы таблицы `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD UNIQUE KEY `UK_tw_accounts_quests` (`QuestID`,`UserID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `tw_accounts_quests_ibfk_4` (`QuestID`);

--
-- Индексы таблицы `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `SkillID` (`SkillID`),
  ADD KEY `OwnerID` (`UserID`);

--
-- Индексы таблицы `tw_aethers`
--
ALTER TABLE `tw_aethers`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_attributs`
--
ALTER TABLE `tw_attributs`
  ADD PRIMARY KEY (`ID`);

--
-- Индексы таблицы `tw_auction_items`
--
ALTER TABLE `tw_auction_items`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `Time` (`Time`),
  ADD KEY `Price` (`Price`);

--
-- Индексы таблицы `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `SlotWings` (`SlotArmor`),
  ADD KEY `SlotHammer` (`SlotHammer`),
  ADD KEY `SlotGun` (`SlotGun`),
  ADD KEY `tw_bots_world_ibfk_4` (`SlotShotgun`),
  ADD KEY `SlotGrenade` (`SlotGrenade`),
  ADD KEY `SlotRifle` (`SlotRifle`);

--
-- Индексы таблицы `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MobID` (`BotID`),
  ADD KEY `it_drop_0` (`it_drop_0`),
  ADD KEY `it_drop_1` (`it_drop_1`),
  ADD KEY `it_drop_2` (`it_drop_2`),
  ADD KEY `it_drop_3` (`it_drop_3`),
  ADD KEY `it_drop_4` (`it_drop_4`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Effect` (`Effect`),
  ADD KEY `Behavior` (`Behavior`);

--
-- Индексы таблицы `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MobID` (`BotID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `tw_bots_npc_ibfk_3` (`Emote`),
  ADD KEY `tw_bots_npc_ibfk_5` (`GiveQuestID`);

--
-- Индексы таблицы `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MobID` (`BotID`),
  ADD KEY `it_need_0` (`RequiredItemID1`),
  ADD KEY `tw_bots_quest_ibfk_3` (`RequiredItemID2`),
  ADD KEY `tw_bots_quest_ibfk_4` (`RewardItemID1`),
  ADD KEY `it_reward_1` (`RewardItemID2`),
  ADD KEY `QuestID` (`QuestID`),
  ADD KEY `tw_bots_quest_ibfk_6` (`RequiredDefeatMobID1`),
  ADD KEY `tw_bots_quest_ibfk_7` (`RequiredDefeatMobID2`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `interactive_type` (`InteractionType`);

--
-- Индексы таблицы `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `CraftIID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_dungeons_door_ibfk_1` (`DungeonID`),
  ADD KEY `tw_dungeons_door_ibfk_2` (`BotID`);

--
-- Индексы таблицы `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_dungeons_records_ibfk_1` (`UserID`),
  ADD KEY `DungeonID` (`DungeonID`),
  ADD KEY `Seconds` (`Seconds`);

--
-- Индексы таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `Bank` (`Bank`),
  ADD KEY `Level` (`Level`),
  ADD KEY `Experience` (`Experience`);

--
-- Индексы таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_guilds_decorations_ibfk_2` (`DecoID`),
  ADD KEY `tw_guilds_decorations_ibfk_3` (`WorldID`),
  ADD KEY `HouseID` (`HouseID`);

--
-- Индексы таблицы `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `MemberID` (`GuildID`);

--
-- Индексы таблицы `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerMID` (`GuildID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `MemberID` (`GuildID`);

--
-- Индексы таблицы `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `MemberID` (`GuildID`);

--
-- Индексы таблицы `tw_houses`
--
ALTER TABLE `tw_houses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `PlantID` (`PlantID`);

--
-- Индексы таблицы `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `HouseID` (`HouseID`),
  ADD KEY `DecoID` (`DecoID`);

--
-- Индексы таблицы `tw_items_list`
--
ALTER TABLE `tw_items_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ItemID` (`ID`),
  ADD KEY `ItemBonus` (`Attribute0`),
  ADD KEY `ItemID_2` (`ID`),
  ADD KEY `ItemType` (`Type`),
  ADD KEY `tw_items_list_ibfk_3` (`Function`),
  ADD KEY `tw_items_list_ibfk_5` (`Attribute1`);

--
-- Индексы таблицы `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `MobID` (`MobID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `ParseInt` (`ParseInt`);

--
-- Индексы таблицы `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_positions_plant`
--
ALTER TABLE `tw_positions_plant`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`);

--
-- Индексы таблицы `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`);

--
-- Индексы таблицы `tw_voucher`
--
ALTER TABLE `tw_voucher`
  ADD PRIMARY KEY (`ID`);

--
-- Индексы таблицы `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  ADD PRIMARY KEY (`ID`);

--
-- Индексы таблицы `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `Currency` (`Currency`);

--
-- Индексы таблицы `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `OwnerID` (`UserID`),
  ADD KEY `StorageID` (`WarehouseID`),
  ADD KEY `Time` (`Time`),
  ADD KEY `NeedItem` (`RequiredItemID`),
  ADD KEY `Price` (`Price`);

--
-- Индексы таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `RequiredQuestID` (`RequiredQuestID`),
  ADD KEY `TwoWorldID` (`TwoWorldID`);

--
-- AUTO_INCREMENT для сохранённых таблиц
--

--
-- AUTO_INCREMENT для таблицы `enum_behavior_mobs`
--
ALTER TABLE `enum_behavior_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT для таблицы `enum_effects_list`
--
ALTER TABLE `enum_effects_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT для таблицы `enum_items_functional`
--
ALTER TABLE `enum_items_functional`
  MODIFY `FunctionID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=13;

--
-- AUTO_INCREMENT для таблицы `enum_items_types`
--
ALTER TABLE `enum_items_types`
  MODIFY `TypeID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT для таблицы `tw_accounts`
--
ALTER TABLE `tw_accounts`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_aethers`
--
ALTER TABLE `tw_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT для таблицы `tw_auction_items`
--
ALTER TABLE `tw_auction_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=33;

--
-- AUTO_INCREMENT для таблицы `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=10;

--
-- AUTO_INCREMENT для таблицы `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=10;

--
-- AUTO_INCREMENT для таблицы `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=32;

--
-- AUTO_INCREMENT для таблицы `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- AUTO_INCREMENT для таблицы `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_houses`
--
ALTER TABLE `tw_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT для таблицы `tw_items_list`
--
ALTER TABLE `tw_items_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=42;

--
-- AUTO_INCREMENT для таблицы `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_positions_plant`
--
ALTER TABLE `tw_positions_plant`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- AUTO_INCREMENT для таблицы `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT для таблицы `tw_voucher`
--
ALTER TABLE `tw_voucher`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=12;

--
-- AUTO_INCREMENT для таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- Ограничения внешнего ключа сохраненных таблиц
--

--
-- Ограничения внешнего ключа таблицы `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  ADD CONSTRAINT `tw_accounts_aethers_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_aethers_ibfk_2` FOREIGN KEY (`AetherID`) REFERENCES `tw_aethers` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  ADD CONSTRAINT `tw_accounts_data_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_4` FOREIGN KEY (`GuildRank`) REFERENCES `tw_guilds_ranks` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_5` FOREIGN KEY (`ID`) REFERENCES `tw_accounts` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_farming`
--
ALTER TABLE `tw_accounts_farming`
  ADD CONSTRAINT `tw_accounts_farming_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  ADD CONSTRAINT `tw_accounts_items_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_items_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  ADD CONSTRAINT `tw_accounts_mailbox_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_mailbox_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_mining`
--
ALTER TABLE `tw_accounts_mining`
  ADD CONSTRAINT `tw_accounts_mining_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  ADD CONSTRAINT `tw_accounts_quests_ibfk_3` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_quests_ibfk_4` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  ADD CONSTRAINT `tw_accounts_skills_ibfk_1` FOREIGN KEY (`SkillID`) REFERENCES `tw_skills_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_skills_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_aethers`
--
ALTER TABLE `tw_aethers`
  ADD CONSTRAINT `tw_aethers_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  ADD CONSTRAINT `tw_bots_info_ibfk_1` FOREIGN KEY (`SlotArmor`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_2` FOREIGN KEY (`SlotHammer`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_3` FOREIGN KEY (`SlotGun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_4` FOREIGN KEY (`SlotShotgun`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_5` FOREIGN KEY (`SlotGrenade`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_info_ibfk_6` FOREIGN KEY (`SlotRifle`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  ADD CONSTRAINT `tw_bots_mobs_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_10` FOREIGN KEY (`it_drop_1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_11` FOREIGN KEY (`it_drop_2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_12` FOREIGN KEY (`it_drop_3`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_13` FOREIGN KEY (`it_drop_4`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_15` FOREIGN KEY (`Effect`) REFERENCES `enum_effects_list` (`Name`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_16` FOREIGN KEY (`Behavior`) REFERENCES `enum_behavior_mobs` (`Behavior`) ON DELETE NO ACTION ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_8` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE NO ACTION ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_9` FOREIGN KEY (`it_drop_0`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  ADD CONSTRAINT `tw_bots_npc_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_4` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_5` FOREIGN KEY (`GiveQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  ADD CONSTRAINT `tw_bots_quest_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_10` FOREIGN KEY (`InteractionType`) REFERENCES `enum_quest_interactive` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_2` FOREIGN KEY (`RequiredItemID1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_3` FOREIGN KEY (`RequiredItemID2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_4` FOREIGN KEY (`RewardItemID1`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_5` FOREIGN KEY (`RewardItemID2`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_6` FOREIGN KEY (`RequiredDefeatMobID1`) REFERENCES `tw_bots_info` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_7` FOREIGN KEY (`RequiredDefeatMobID2`) REFERENCES `tw_bots_info` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_8` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_9` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  ADD CONSTRAINT `tw_crafts_list_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  ADD CONSTRAINT `tw_dungeons_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE NO ACTION ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  ADD CONSTRAINT `tw_dungeons_door_ibfk_1` FOREIGN KEY (`DungeonID`) REFERENCES `tw_dungeons` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_dungeons_door_ibfk_2` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  ADD CONSTRAINT `tw_dungeons_records_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_dungeons_records_ibfk_2` FOREIGN KEY (`DungeonID`) REFERENCES `tw_dungeons` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD CONSTRAINT `tw_guilds_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_2` FOREIGN KEY (`DecoID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_4` FOREIGN KEY (`HouseID`) REFERENCES `tw_guilds_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  ADD CONSTRAINT `tw_guilds_history_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  ADD CONSTRAINT `tw_guilds_houses_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_houses_ibfk_2` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  ADD CONSTRAINT `tw_guilds_ranks_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_houses`
--
ALTER TABLE `tw_houses`
  ADD CONSTRAINT `tw_houses_ibfk_1` FOREIGN KEY (`PlantID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_ibfk_3` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  ADD CONSTRAINT `tw_houses_decorations_ibfk_1` FOREIGN KEY (`HouseID`) REFERENCES `tw_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_decorations_ibfk_2` FOREIGN KEY (`DecoID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_decorations_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_items_list`
--
ALTER TABLE `tw_items_list`
  ADD CONSTRAINT `tw_items_list_ibfk_1` FOREIGN KEY (`Type`) REFERENCES `enum_items_types` (`TypeID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_2` FOREIGN KEY (`Function`) REFERENCES `enum_items_functional` (`FunctionID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_3` FOREIGN KEY (`Attribute0`) REFERENCES `tw_attributs` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_4` FOREIGN KEY (`Attribute1`) REFERENCES `tw_attributs` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  ADD CONSTRAINT `tw_logics_worlds_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_logics_worlds_ibfk_2` FOREIGN KEY (`ParseInt`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_positions_plant`
--
ALTER TABLE `tw_positions_plant`
  ADD CONSTRAINT `tw_positions_plant_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD CONSTRAINT `tw_warehouses_ibfk_1` FOREIGN KEY (`Currency`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  ADD CONSTRAINT `tw_warehouse_items_ibfk_1` FOREIGN KEY (`WarehouseID`) REFERENCES `tw_warehouses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_warehouse_items_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_warehouse_items_ibfk_3` FOREIGN KEY (`RequiredItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  ADD CONSTRAINT `tw_world_swap_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_world_swap_ibfk_2` FOREIGN KEY (`TwoWorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_world_swap_ibfk_3` FOREIGN KEY (`RequiredQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
