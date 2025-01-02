-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Хост: 127.0.0.1
-- Время создания: Янв 02 2025 г., 22:57
-- Версия сервера: 10.4.32-MariaDB
-- Версия PHP: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- База данных: `test`
--

-- --------------------------------------------------------

--
-- Структура таблицы `enum_behavior_mobs`
--

CREATE TABLE `enum_behavior_mobs` (
  `ID` int(11) NOT NULL,
  `Behavior` varchar(32) NOT NULL DEFAULT 'Standard'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `enum_behavior_mobs`
--

INSERT INTO `enum_behavior_mobs` (`ID`, `Behavior`) VALUES
(3, 'Sleepy'),
(2, 'Slower'),
(1, 'Standard');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_effects_list`
--

CREATE TABLE `enum_effects_list` (
  `ID` int(11) NOT NULL,
  `Name` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

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
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Дамп данных таблицы `enum_items_functional`
--

INSERT INTO `enum_items_functional` (`FunctionID`, `Name`) VALUES
(-1, 'Not have function'),
(0, 'Equip hammer (Only equip type)'),
(1, 'Equip gun (Only equip type)'),
(2, 'Equip shotgun (Only equip type)'),
(3, 'Equip grenade (Only equip type)'),
(4, 'Equip rifle (Only equip type)'),
(5, 'Equip pickaxe (Only equip type)'),
(6, 'Equip rake (Only equip type)'),
(7, 'Equip armor (Only equip type)'),
(8, 'Equip eidolon (Only equip type)'),
(9, 'Equip potion HP (Only equip type)'),
(10, 'Equip potion MP (Only equip type)'),
(11, 'Equip Tittle (Only equip type)'),
(12, 'Single use x1'),
(13, 'Multiple use x99'),
(14, 'Setting (Only settings or modules type)'),
(15, 'Resource harvestable'),
(16, 'Resource mineable');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_items_types`
--

CREATE TABLE `enum_items_types` (
  `TypeID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Дамп данных таблицы `enum_items_types`
--

INSERT INTO `enum_items_types` (`TypeID`, `Name`) VALUES
(0, 'Invisible'),
(1, 'Usable'),
(2, 'Crafting material'),
(3, 'Module'),
(4, 'Other'),
(5, 'Setting'),
(6, 'Equipment'),
(7, 'Decoration'),
(8, 'Potion');

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
  `Language` varchar(8) NOT NULL DEFAULT 'en',
  `CountryISO` varchar(32) NOT NULL DEFAULT 'UN'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_accounts`
--

INSERT INTO `tw_accounts` (`ID`, `Username`, `Password`, `PasswordSalt`, `RegisterDate`, `LoginDate`, `RegisteredIP`, `LoginIP`, `Language`, `CountryISO`) VALUES
(1, 'kuro', '91bc5440fce5df2645c973c99f9c9db6d3e734039a3cc3b51bdce1f444cebf07', 'AhnTbSVoeRF37TXTcSjL66MG', '2025-01-02 19:29:53', '2025-01-03 04:34:29', '[::1]', '[::1]', 'en', 'UN');

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_aethers`
--

CREATE TABLE `tw_accounts_aethers` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `AetherID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_bans`
--

CREATE TABLE `tw_accounts_bans` (
  `Id` int(11) NOT NULL,
  `AccountId` int(11) NOT NULL,
  `BannedSince` timestamp NULL DEFAULT current_timestamp(),
  `BannedUntil` timestamp NOT NULL DEFAULT current_timestamp(),
  `Reason` varchar(512) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT 'No Reason Given'
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_data`
--

CREATE TABLE `tw_accounts_data` (
  `ID` int(11) NOT NULL,
  `Nick` varchar(32) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  `ProfessionID` int(11) NOT NULL DEFAULT -1,
  `Bank` varchar(512) DEFAULT '0',
  `RankPoints` int(11) NOT NULL DEFAULT 0,
  `CrimeScore` int(11) NOT NULL DEFAULT 0,
  `DailyStamp` bigint(20) NOT NULL DEFAULT 0,
  `WeekStamp` bigint(20) NOT NULL DEFAULT 0,
  `MonthStamp` bigint(20) NOT NULL DEFAULT 0,
  `Upgrade` int(11) NOT NULL DEFAULT 0,
  `Achievements` longtext NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_accounts_data`
--

INSERT INTO `tw_accounts_data` (`ID`, `Nick`, `WorldID`, `ProfessionID`, `Bank`, `RankPoints`, `CrimeScore`, `DailyStamp`, `WeekStamp`, `MonthStamp`, `Upgrade`, `Achievements`) VALUES
(1, 'Kurosio', 0, 0, '0', 5, 0, 1735846650, 1735846650, 1735846650, 0, '');

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC;

--
-- Дамп данных таблицы `tw_accounts_items`
--

INSERT INTO `tw_accounts_items` (`ID`, `ItemID`, `Value`, `Settings`, `Enchant`, `Durability`, `UserID`) VALUES
(1, 25, 1, 0, 0, 100, 1),
(2, 98, 1, 1, 0, 100, 1),
(3, 2, 1, 1, 0, 100, 1),
(4, 34, 1, 1, 0, 100, 1),
(5, 93, 1, 1, 0, 100, 1),
(6, 65, 1, 1, 0, 100, 1),
(8, 95, 95, 0, 0, 100, 1),
(9, 1, 91800, 0, 0, 100, 1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_mailbox`
--

CREATE TABLE `tw_accounts_mailbox` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL,
  `Sender` varchar(32) NOT NULL DEFAULT '''Game''',
  `Description` varchar(64) NOT NULL,
  `AttachedItems` longtext DEFAULT NULL,
  `Readed` tinyint(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_professions`
--

CREATE TABLE `tw_accounts_professions` (
  `ID` int(11) NOT NULL,
  `ProfessionID` int(11) NOT NULL,
  `Data` longtext NOT NULL,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Дамп данных таблицы `tw_accounts_professions`
--

INSERT INTO `tw_accounts_professions` (`ID`, `ProfessionID`, `Data`, `UserID`) VALUES
(1, 0, '{\"attributes\":{\"5\":0,\"6\":0},\"exp\":0,\"level\":1,\"up\":0}', 1),
(2, 1, '{\"attributes\":{\"2\":0,\"3\":0,\"4\":0},\"exp\":0,\"level\":1,\"up\":0}', 1),
(3, 2, '{\"attributes\":{\"7\":0,\"8\":0},\"exp\":0,\"level\":1,\"up\":0}', 1),
(4, 4, '{\"attributes\":{\"12\":1},\"exp\":0,\"level\":1,\"up\":0}', 1),
(5, 3, '{\"attributes\":{\"11\":1},\"exp\":0,\"level\":1,\"up\":0}', 1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_quests`
--

CREATE TABLE `tw_accounts_quests` (
  `ID` int(11) NOT NULL,
  `QuestID` int(11) DEFAULT NULL,
  `UserID` int(11) NOT NULL,
  `Type` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_achievements`
--

CREATE TABLE `tw_achievements` (
  `ID` int(11) NOT NULL,
  `Type` int(11) NOT NULL,
  `Criteria` int(11) NOT NULL,
  `Required` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `Reward` varchar(256) NOT NULL,
  `AchievementPoint` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_attributes`
--

CREATE TABLE `tw_attributes` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `Price` int(11) NOT NULL,
  `Group` int(11) NOT NULL COMMENT '0.tank1.healer2.dps3.weapon4.hard5.jobs 6. others'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_attributes`
--

INSERT INTO `tw_attributes` (`ID`, `Name`, `Price`, `Group`) VALUES
(1, 'DMG', 0, 4),
(2, 'Attack SPD', 1, 2),
(3, 'Crit DMG', 1, 2),
(4, 'Crit', 1, 2),
(5, 'HP', 1, 0),
(6, 'Lucky', 1, 0),
(7, 'MP', 1, 1),
(8, 'Vampirism', 1, 1),
(9, 'Ammo Regen', 1, 3),
(10, 'Ammo', 4, 3),
(11, 'Efficiency', -1, 5),
(12, 'Extraction', -1, 5),
(13, 'Hammer DMG', -1, 4),
(14, 'Gun DMG', -1, 4),
(15, 'Shotgun DMG', -1, 4),
(16, 'Grenade DMG', -1, 4),
(17, 'Rifle DMG', -1, 4),
(18, 'Lucky Drop', -1, 6),
(19, 'Eidolon PWR', -1, 6),
(20, 'Gold Capacity', 0, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_auction_slots`
--

CREATE TABLE `tw_auction_slots` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Value` int(11) NOT NULL,
  `Enchant` int(11) NOT NULL DEFAULT 0,
  `Price` int(11) NOT NULL,
  `OwnerID` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_bots_info`
--

CREATE TABLE `tw_bots_info` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Bot name',
  `JsonTeeInfo` varchar(128) NOT NULL DEFAULT '{ "skin": "default", 	"custom_color": 0, 	"color_body": 0, 	"color_feer": 0}',
  `EquippedModules` varchar(64) NOT NULL DEFAULT '0',
  `SlotHammer` int(11) DEFAULT 2,
  `SlotGun` int(11) DEFAULT 3,
  `SlotShotgun` int(11) DEFAULT 4,
  `SlotGrenade` int(11) DEFAULT 5,
  `SlotRifle` int(11) DEFAULT 6,
  `SlotArmor` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_bots_info`
--

INSERT INTO `tw_bots_info` (`ID`, `Name`, `JsonTeeInfo`, `EquippedModules`, `SlotHammer`, `SlotGun`, `SlotShotgun`, `SlotGrenade`, `SlotRifle`, `SlotArmor`) VALUES
(5, 'Pig', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"pinky\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(6, 'Pig Queen', '{\"color_body\":65408,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"pinky\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(7, 'Skill master', '{\"color_body\":9895735,\"color_feet\":9633571,\"custom_color\":1,\"skin\":\"jeet\"}', '0', 2, 3, 4, 5, 6, NULL),
(8, 'Craftsman', '{\"color_body\":8545280,\"color_feet\":8257280,\"custom_color\":1,\"skin\":\"coala\"}', '0', 2, 3, 4, 5, 6, NULL),
(9, 'Nurse', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"tika\"}', '0', 2, 3, 4, 5, 6, NULL),
(10, 'Lady Sheila', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"nanami\"}', '0', 2, 3, 4, 5, 6, NULL),
(11, 'Willie', '{\"color_body\":6029183,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"kitty_redbopp\"}', '0', 2, 3, 4, 5, 6, NULL),
(12, 'Joel', '{\"color_body\":6029183,\"color_feet\":65408,\"custom_color\":0,\"skin\":\"cammostripes\"}', '0', 2, 3, 4, 5, 6, NULL),
(13, 'Anita', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Jigglypuff\"}', '0', 2, 3, 4, 5, 6, NULL),
(14, 'Betsy', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"mermydon\"}', '0', 2, 3, 4, 5, 6, NULL),
(15, 'Corey', '{\"color_body\":65280,\"color_feet\":10477151,\"custom_color\":1,\"skin\":\"kintaro_2\"}', '0', 2, 3, 4, 5, 6, NULL),
(16, 'York', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"greensward\"}', '0', 2, 3, 4, 5, 6, NULL),
(17, 'Warren', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"musmann\"}', '0', 2, 3, 4, 5, 6, NULL),
(18, 'Caine', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"mermydon-coala\"}', '0', 2, 3, 4, 5, 6, NULL),
(19, 'Brian', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"nanas\"}', '0', 2, 3, 4, 5, 6, NULL),
(20, 'Bunny', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"bunny\"}', '0', 2, 3, 4, 5, 6, NULL),
(21, 'Workman', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"default\"}', '0', 2, 3, 4, 5, 6, NULL),
(22, 'Lumberjacks', '{\"color_body\":12942592,\"color_feet\":11009792,\"custom_color\":1,\"skin\":\"nanami_glow\"}', '0', 2, 3, 4, 5, 6, NULL),
(23, 'Doctor Cal', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"kitty_redstripe\"}', '0', 2, 3, 4, 5, 6, NULL),
(24, 'Antelope', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Fluffi\"}', '0', 2, 3, 4, 5, 6, NULL),
(25, 'Cyclope', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Beast_1\"}', '0', 2, 3, 4, 5, 6, NULL),
(26, 'Auctionist', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"veteran\"}', '0', 2, 3, 4, 5, 6, NULL),
(27, 'Sunbird', '{\"color_body\":9502464,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"amor\"}', '0', 2, 3, NULL, NULL, NULL, NULL),
(28, 'Bloody Bat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":1,\"skin\":\"Bat\"}', '0', 2, 3, 4, 5, 6, NULL),
(29, 'Sneaky Cat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"blackcat\"}', '0', 2, 3, 4, 5, 6, NULL),
(30, 'Dead Miner', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"coffee_hairy\"}', '0', 2, 3, 4, 5, 6, NULL),
(31, 'Shadow Dark', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"darky\"}', '0', 2, 3, 4, 5, 6, NULL),
(32, 'Bat', '{\"color_body\":16711424,\"color_feet\":10477151,\"custom_color\":0,\"skin\":\"Bat\"}', '0', 2, 3, 4, 5, 6, NULL),
(33, 'Flower', '{\"color_body\":8060863,\"color_feet\":2874975,\"custom_color\":0,\"skin\":\"blackcat\"}', '0', 2, 3, 4, 5, 6, NULL),
(34, 'Petal Fairy', '{\"color_body\":8191744,\"color_feet\":9100881,\"custom_color\":1,\"skin\":\"Foxberry\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(35, 'Green Fairy', '{\"color_body\":3211008,\"color_feet\":8379985,\"custom_color\":1,\"skin\":\"Foxberry\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(36, 'Beebis', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"coala_phoenix\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(37, 'Ruin Giant', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"Demon\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(38, 'Larry', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"warpaint\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(39, 'Bentley', '{\"color_body\":9240320,\"color_feet\":16777134,\"custom_color\":1,\"skin\":\"bluestripe\"}', '0', 2, 3, 4, 5, 6, NULL),
(40, 'Kid Sprout', '{\"color_body\":5963520,\"color_feet\":2817792,\"custom_color\":1,\"skin\":\"chera\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(41, 'Wet Sprout', '{\"color_body\":9174784,\"color_feet\":3997440,\"custom_color\":1,\"skin\":\"chera\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(42, 'Elena', '{\"color_body\":0,\"color_feet\":0,\"custom_color\":0,\"skin\":\"clan_wl_nanami\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(43, 'Creature', '{\"color_body\":10878976,\"color_feet\":65280,\"custom_color\":1,\"skin\":\"jeet\"}', '0', 2, 3, 4, 5, 6, NULL),
(44, 'Kane', '{\"color_body\":7456086,\"color_feet\":12320512,\"custom_color\":1,\"skin\":\"greensward\"}', '0', 2, 3, 4, 5, 6, NULL),
(45, 'Otohime', '{\"color_body\":9174784,\"color_feet\":3997440,\"custom_color\":0,\"skin\":\"vulpi\"}', '0', 2, NULL, 4, NULL, NULL, NULL),
(46, 'Merrilee', '{\"color_body\":8519424,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"dragon 2\"}', '0', 2, NULL, NULL, 5, NULL, NULL),
(47, 'Luther', '{\"color_body\":65280,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"bluekitty\"}', '0', 2, 3, 4, 5, 6, NULL),
(48, 'Madeline', '{\"color_body\":13565696,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"vulpi\"}', '0', 2, 3, 4, 5, 6, NULL),
(49, 'Tanuki', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"Bonsly\"}', '0', 2, 3, 4, 5, 6, NULL),
(50, 'Isaac', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"toptri\"}', '0', 2, 3, 4, 5, 6, NULL),
(51, 'Braeden', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"greensward\"}', '0', 2, 3, 4, 5, 6, NULL),
(52, 'Sun Crab', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"kitty_redstripe\"}', '0', 2, 3, 4, 5, 6, NULL),
(53, 'Mobray', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"PaladiN\"}', '0', 2, 3, 4, 5, 6, NULL),
(54, 'Beetle', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"beast\"}', '0', 2, 3, 4, 5, 6, NULL),
(55, 'Aged Man', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"veteran\"}', '0', 2, 3, 4, 5, 6, NULL),
(56, 'Helonian Man', '{\"color_body\":2555648,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"chinese_by_whis\"}', '0', 2, 3, 4, 5, 6, NULL),
(57, 'Grandpa', '{\"color_body\":15697152,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"greyfox\"}', '0', 2, 3, 4, 5, 6, NULL),
(58, 'Dryad', '{\"color_body\":12779264,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"amina_kitty\"}', '0', 2, 3, 4, 5, NULL, NULL),
(59, 'Spirit', '{\"color_body\":2752256,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"asteroid\"}', '0', 2, 3, 4, 5, 6, NULL),
(60, 'Parrot', '{\"color_body\":2752256,\"color_feet\":4980735,\"custom_color\":0,\"skin\":\"clefairy\"}', '0', 2, NULL, NULL, NULL, NULL, NULL),
(61, 'Soran', '{\"color_body\":11155259,\"color_feet\":4980735,\"custom_color\":1,\"skin\":\"coala_warpaint\"}', '0', 2, 3, 4, 5, 6, NULL);

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
  `Effect` set('','Slowdown','Poison','Fire') NOT NULL DEFAULT '',
  `Behavior` set('','Sleepy','Slower') DEFAULT '',
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
  `it_drop_count` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|',
  `it_drop_chance` varchar(64) NOT NULL DEFAULT '|0|0|0|0|0|'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_bots_mobs`
--

INSERT INTO `tw_bots_mobs` (`ID`, `BotID`, `WorldID`, `PositionX`, `PositionY`, `Effect`, `Behavior`, `Level`, `Power`, `Spread`, `Number`, `Respawn`, `Boss`, `it_drop_0`, `it_drop_1`, `it_drop_2`, `it_drop_3`, `it_drop_4`, `it_drop_count`, `it_drop_chance`) VALUES
(1, 20, 0, 1216, 704, '', 'Slower', 3, 8, 0, 7, 1, 0, 40, 44, NULL, NULL, NULL, '|1|1|0|0|0|', '|5|2.11|0|0|0|');

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
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_bots_npc`
--

INSERT INTO `tw_bots_npc` (`ID`, `BotID`, `PosX`, `PosY`, `GiveQuestID`, `DialogData`, `Function`, `Static`, `Emote`, `WorldID`) VALUES
(1, 7, 6472, 7569, NULL, '[{\"text\":\"Someone once told me friendship is magic. That\'s ridiculous. You can\'t turn people into frogs with friendship{OR}Hey..what\'s <bot_9> up to? Have you...have you talked to her, by chance?{OR}\",\"emote\":\"normal\"}\n]', -1, 0, '', 0);

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
  `ScenarioData` longtext DEFAULT NULL,
  `TasksData` longtext NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_bots_quest`
--

INSERT INTO `tw_bots_quest` (`ID`, `BotID`, `QuestID`, `Step`, `WorldID`, `PosX`, `PosY`, `DialogData`, `ScenarioData`, `TasksData`) VALUES
(1, 10, 1, 1, 0, 28353, 2065, '[{\"text\":\"[re][le]It is now getting late. <player>, this boy is so...\"},{\"text\":\"\\\"<player>, have you forgotten your promise already?\\\"\\n\\n Sheila asks with a gentle smile.\",\"action_step\":1}]', '{\"chat\":[{\"text\":\"What was it?\"},{\"text\":\"You are disturbed by a strange dream.\"}]}', '{\"required_items\":[{\"id\":92,\"count\":1}]}');

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_dungeons_records`
--

CREATE TABLE `tw_dungeons_records` (
  `ID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `DungeonID` int(11) NOT NULL,
  `Lifetime` int(11) NOT NULL,
  `PassageHelp` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_groups`
--

CREATE TABLE `tw_groups` (
  `ID` int(11) NOT NULL,
  `OwnerUID` int(11) NOT NULL,
  `AccountIDs` varchar(64) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds`
--

CREATE TABLE `tw_guilds` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT 'Member name',
  `Members` varchar(512) DEFAULT NULL,
  `DefaultRankID` int(11) DEFAULT NULL,
  `LeaderUID` int(11) DEFAULT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` bigint(11) NOT NULL DEFAULT 0,
  `Bank` longtext NOT NULL DEFAULT '0',
  `Score` int(11) NOT NULL DEFAULT 0,
  `LogFlag` bigint(11) NOT NULL DEFAULT 0,
  `AvailableSlots` int(11) NOT NULL DEFAULT 2,
  `ChairExperience` int(11) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_decorations`
--

CREATE TABLE `tw_guilds_decorations` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_history`
--

CREATE TABLE `tw_guilds_history` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) NOT NULL,
  `Text` varchar(64) NOT NULL,
  `Time` datetime NOT NULL DEFAULT current_timestamp()
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_houses`
--

CREATE TABLE `tw_guilds_houses` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) DEFAULT NULL,
  `InitialFee` int(11) NOT NULL DEFAULT 0,
  `RentDays` int(11) NOT NULL DEFAULT 3,
  `Doors` longtext DEFAULT NULL,
  `Farmzones` longtext DEFAULT NULL,
  `Properties` longtext DEFAULT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_guilds_houses`
--

INSERT INTO `tw_guilds_houses` (`ID`, `GuildID`, `InitialFee`, `RentDays`, `Doors`, `Farmzones`, `Properties`, `WorldID`) VALUES
(1, NULL, 0, 3, NULL, NULL, NULL, 1),
(2, NULL, 0, 3, NULL, NULL, NULL, 1),
(3, NULL, 0, 3, NULL, NULL, NULL, 5);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_invites`
--

CREATE TABLE `tw_guilds_invites` (
  `ID` int(11) NOT NULL,
  `GuildID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_guilds_ranks`
--

CREATE TABLE `tw_guilds_ranks` (
  `ID` int(11) NOT NULL,
  `Rights` int(11) NOT NULL DEFAULT 3,
  `Name` varchar(32) NOT NULL DEFAULT 'Rank name',
  `GuildID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
  `AccessData` varchar(128) DEFAULT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_houses_decorations`
--

CREATE TABLE `tw_houses_decorations` (
  `ID` int(11) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `HouseID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
  `Data` longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin DEFAULT NULL CHECK (json_valid(`Data`)),
  `InitialPrice` int(11) NOT NULL DEFAULT 100,
  `Desynthesis` int(11) NOT NULL DEFAULT 100,
  `Attribute0` int(11) DEFAULT NULL,
  `Attribute1` int(11) DEFAULT NULL,
  `AttributeValue0` int(11) NOT NULL DEFAULT 0,
  `AttributeValue1` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_items_list`
--

INSERT INTO `tw_items_list` (`ID`, `Name`, `Description`, `Type`, `Function`, `Data`, `InitialPrice`, `Desynthesis`, `Attribute0`, `Attribute1`, `AttributeValue0`, `AttributeValue1`) VALUES
(1, 'Gold', 'Major currency', 0, -1, NULL, 0, 0, 13, NULL, 0, 0),
(2, 'Hammer', 'A normal hammer', 6, 0, NULL, 0, 0, 13, 3, 3, 3),
(3, 'Gun', 'Conventional weapon', 6, 1, NULL, 5, 0, 14, NULL, 1, 0),
(4, 'Shotgun', 'Conventional weapon', 6, 2, NULL, 5, 0, 15, NULL, 1, 0),
(5, 'Grenade', 'Conventional weapon', 6, 3, NULL, 5, 0, 16, NULL, 1, 0),
(6, 'Rifle', 'Conventional weapon', 6, 4, NULL, 5, 0, 17, NULL, 5, 0),
(7, 'Material', 'Required to improve weapons', 4, 16, NULL, 5, 0, NULL, NULL, 0, 0),
(8, 'Product', 'Required to shop', 4, 15, NULL, 0, 0, NULL, NULL, 0, 0),
(9, 'Skill Point', 'Skill point', 0, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(10, 'Achievement Point', 'Achievement Point', 0, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(11, 'Pickup Shotgun', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(12, 'Pickup Grenade', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(13, 'Pickup Mana', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(14, 'Potion mana regen', 'Regenerate +5%, 15sec every sec.\n', 8, 12, NULL, 5, 10, NULL, NULL, 0, 0),
(15, 'Tiny HP Potion', 'Recovers 7HP per second for 10 seconds.', 8, 12, NULL, 5, 10, NULL, NULL, 0, 0),
(16, 'Capsule survival experience', 'You got 10-50 experience survival', 1, 13, NULL, 5, 0, NULL, NULL, 0, 0),
(17, 'Little bag of gold', 'You got 10-50 gold', 1, 13, NULL, 5, 0, NULL, NULL, 0, 0),
(18, 'Pickup Health', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(19, 'Explosive module for gun', 'It happens sometimes', 3, 14, NULL, 5, 100, 14, NULL, 1, 0),
(20, 'Explosive module for shotgun', 'It happens sometimes', 3, 14, NULL, 5, 100, 15, NULL, 1, 0),
(21, 'Ticket reset class stats', 'Resets only class stats(Dps, Tank, Healer).', 1, 12, NULL, 5, 0, NULL, NULL, 0, 0),
(22, 'Mode PVP', 'Settings game.', 5, 14, NULL, 0, 0, NULL, NULL, 0, 0),
(23, 'Ticket reset weapon stats', 'Resets only ammo stats(Ammo).', 1, 12, NULL, 5, 0, NULL, NULL, 0, 0),
(24, 'Blessing for discount craft', 'Need dress it, -20% craft price', 8, 12, NULL, 5, 0, NULL, NULL, 0, 0),
(25, 'Show equipment description', 'Settings game.', 5, 14, NULL, 0, 0, NULL, NULL, 0, 0),
(26, 'Rusty Rake', 'The usual rusty rake.', 6, 6, NULL, 5, 30, 12, NULL, 3, 0),
(27, 'Rusty Pickaxe', 'The usual rusty pickaxe.', 6, 5, NULL, 5, 30, 11, NULL, 3, 0),
(28, 'Beast Tusk Light Armor (All)', 'Lightweight armor.', 6, 7, NULL, 5, 40, 5, NULL, 10, 0),
(29, 'Boxes of jam', 'A jam made of lots of fresh fruits.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(30, 'Allied seals', 'Allied seals', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(31, 'Wood', '...', 2, 15, NULL, 5, 1, NULL, NULL, 0, 0),
(32, 'Antelope blood', 'The blood extracted from Antelopes.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(33, 'Wild Forest Mushroom', 'They can often be found under big trees.', 2, 15, NULL, 5, 1, NULL, NULL, 0, 0),
(34, 'Show critical damage', 'Settings game.', 5, 14, NULL, 0, 0, NULL, NULL, 0, 0),
(35, 'Althyk\'s Ring', 'Althyk\'s Ring is an item level 1.', 3, 14, NULL, 5, 30, 5, 7, 18, 22),
(36, 'Empyrean Ring', 'Empyrean Ring is an item level 1.', 3, 14, NULL, 5, 30, 5, 7, 28, 16),
(37, 'Ring of Fidelity', 'Ring of Fidelity is an item level 1.', 3, 14, NULL, 5, 30, 4, 1, 5, 1),
(38, 'Eversharp Ring', 'Eversharp Ring is an item level 1.', 3, 14, NULL, 5, 32, 5, NULL, 50, 0),
(39, 'Green Grass Armor (All)', 'Lightweight armor.', 6, 7, NULL, 5, 60, 5, NULL, 20, 0),
(40, 'Green grass', '...', 2, 15, NULL, 5, 1, NULL, NULL, 0, 0),
(41, 'Bloody Woven Flower', 'Soaked in blood', 3, 14, NULL, 5, 50, 7, 10, 40, 3),
(42, 'Grass Rake', 'The usual grass rake.', 6, 6, NULL, 5, 50, 12, NULL, 5, 0),
(43, 'Grass Pickaxe', 'The usual grass pickaxe.', 6, 5, NULL, 5, 50, 11, NULL, 5, 0),
(44, 'Fragile Iron', '...', 2, 16, NULL, 5, 1, NULL, NULL, 0, 0),
(45, 'Refreshing Potion', 'A potion that Village Doctor Cal made.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(46, 'Blue pollen', '...', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(47, 'Green pollen', '...', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(48, 'Relic Fragments', 'Fragments from the ruins.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(49, 'Heavy Crate', 'A crate full of various items.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(50, 'Rations', 'These cloth-wrapped are heavier than they look.', 4, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(51, 'Torn handbag', 'Torn handbag with ammo', 3, 14, NULL, 5, 100, 10, NULL, 5, 0),
(52, 'Sticky Secretion', 'A sticky and slimy liquid.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(53, 'Cooling Liquid', 'An ice-cold cooling liquid.', 2, -1, NULL, 5, 1, NULL, NULL, 0, 0),
(54, 'Bentley\'s Amulet', 'A somewhat shabby amulet that emits magic waves.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(55, 'Uncharged Gem', 'A mysterious gem that can absorb Magic.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(56, 'Charged Gem', 'A gem that has been recharged.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(57, 'Otohime', 'Eidolon Otohime', 6, 8, NULL, 5, 320, 19, NULL, 90, 0),
(58, 'Random Relics Box', 'Small chance of dropping a relics', 1, 13, '{\"random_box\":[{\"item_id\":16,\"value\":3,\"chance\":82},{\"item_id\":17,\"value\":2,\"chance\":81},{\"item_id\":44,\"value\":3,\"chance\":80},{\"item_id\":48,\"value\":3,\"chance\":60},{\"item_id\":19,\"value\":1,\"chance\":1},{\"item_id\":20,\"value\":1,\"chance\":1},{\"item_id\":59,\"value\":1,\"chance\":1}]}', 0, 0, NULL, NULL, 0, 0),
(59, 'Merrilee', 'Eidolon Merrilee', 6, 8, NULL, 0, 0, 19, NULL, 90, 0),
(60, 'Blue Light Armor (All)', 'Lightweight armor.', 6, 7, NULL, 5, 80, 5, NULL, 30, 0),
(61, 'Blue slime', '...', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(62, 'Blue Rake', 'The usual blue rake.', 6, 6, NULL, 5, 60, 12, NULL, 10, 0),
(63, 'Blue Pickaxe', 'The usual blue pickaxe.', 6, 5, NULL, 5, 60, 11, NULL, 10, 0),
(64, 'Poison Hook', 'Inflicts gradual damage.', 3, 14, NULL, 5, 80, 9, NULL, 20, 0),
(65, 'Explosive impulse hook', 'Inflicts gradual explode damage.', 3, 14, NULL, 5, 80, 8, NULL, 20, 0),
(66, 'Magic Spider Hook', 'It\'s sticky to the air.', 3, 14, NULL, 5, 80, 2, NULL, 20, 0),
(67, 'Fruit', 'This fruit gives out a sweet fragrance.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(68, 'Confiscated Material', 'The thick, heavy box', 2, -1, NULL, 5, 3, NULL, NULL, 0, 0),
(69, 'Bestial Light Armor (DoT)', 'Armor for Tank.', 6, 7, NULL, 5, 100, 5, 6, 40, 15),
(70, 'Cleaned Fruit', 'This fruit gives out a sweet fragrance.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(71, 'Hard Fruit', 'The fruit has just been picked from a tree and is caked in mud.', 2, 15, NULL, 5, 2, NULL, NULL, 0, 0),
(72, 'Insect Fluid', 'It\'s extremely sticky and has a slightly sour taste.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(73, 'Waterproof Bomb', 'Covered in waterproof paint and won\'t get wet.', 4, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(74, 'Flyer from Braeden', 'The flyer from Fisherman Braeden is covered in text.', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(75, 'Eternal Sun Belt', 'Eternal Sun Belt is an item level 1.', 3, 14, NULL, 5, 30, 6, 2, 30, 52),
(76, 'Shadower Mantle (DoH)', 'Mantle for Healer.', 6, 7, NULL, 5, 100, 7, 8, 30, 15),
(77, 'Mercenary Armor (DoW)', 'Lightweight armor for DPS.', 6, 7, NULL, 5, 100, 1, 2, 2, 80),
(78, 'Vine', 'It\'s long, but quite firm.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(79, 'Thin Vine', 'It\'s thin and long, but quite firm.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(80, 'Dryad', 'Eidolon Dryad', 6, 8, NULL, 0, 0, 19, NULL, 100, 0),
(81, 'Crab Claw', 'Claw of crab.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(82, 'Broken Bone', 'Cracked bones with plentiful marrow.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(83, 'Sharp Bone', 'Sharp bone can be sold to merchants who need it.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(84, 'Large Petal', 'After a special drying process.', 2, -1, NULL, 5, 2, NULL, NULL, 0, 0),
(85, 'Thorny Ring', 'Thorny Ring is an item level 1.', 3, 14, NULL, 5, 32, 9, 5, 32, 16),
(86, 'Thorny Necklace', 'Thorny Necklace is an item level 1.', 3, 14, NULL, 5, 32, 5, 7, 24, 12),
(87, 'Bone Armillae', 'Bone Armillae is an item level 1.', 3, 14, NULL, 5, 32, 1, NULL, 1, 0),
(88, 'Pig Queen', 'Eidolon Pig Queen', 6, 8, NULL, 0, 0, 19, 1, 150, 1),
(89, 'Eidolon Crystal', 'Required to improve eidolons', 4, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(90, 'Caged Parrot', 'Was it worth it?', 2, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(91, 'Box full of supplies', '....', 0, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(92, 'The adventurer\'s clearance', 'Needed for permission to pass.', 4, -1, NULL, 0, 0, NULL, NULL, 0, 0),
(93, 'Show quest star navigation', 'Settings game.', 5, 14, NULL, 0, 0, NULL, NULL, 0, 0),
(94, 'Pickup Laser', 'Decoration for house!', 7, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(95, 'Ticket guild', 'Command: /gcreate <name>', 4, -1, NULL, 5, 0, NULL, NULL, 0, 0),
(96, 'Customizer', 'Customizer for personal skins', 3, 14, NULL, 5, 80, 9, NULL, 20, 0),
(97, 'Damage Equalizer', 'Disabled self dmg.', 3, 14, NULL, 5, 80, 9, NULL, 20, 0),
(98, 'Show detail gain messages', 'Settings game.', 5, 14, NULL, 0, 0, NULL, NULL, 0, 0),
(99, 'Hammer Lamp', 'Hammer Lamp', 6, 0, NULL, 0, 0, 13, 3, 3, 3),
(100, 'Pizdamet', 'Pizdamet', 6, 3, NULL, 5, 0, 16, NULL, 1, 0),
(101, 'Plazma wall', 'Plazma wall', 6, 4, NULL, 5, 0, 17, NULL, 5, 0),
(102, 'Hammer Blast', 'Hammer Blast', 6, 0, NULL, 0, 0, 13, 3, 3, 3),
(103, 'Magnetic Pulse', 'Conventional weapon', 6, 4, NULL, 5, 0, 17, NULL, 5, 0);

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_positions_farming`
--

CREATE TABLE `tw_positions_farming` (
  `ID` int(11) NOT NULL,
  `ItemID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Health` int(11) NOT NULL DEFAULT 100,
  `PositionX` int(11) NOT NULL,
  `PositionY` int(11) NOT NULL,
  `Distance` int(11) NOT NULL DEFAULT 300 COMMENT 'Range of unit spreading',
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_positions_mining`
--

INSERT INTO `tw_positions_mining` (`ID`, `ItemID`, `Level`, `Health`, `PositionX`, `PositionY`, `Distance`, `WorldID`) VALUES
(1, 44, 1, 180, 4754, 564, 500, 2);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_quests_board_list`
--

CREATE TABLE `tw_quests_board_list` (
  `ID` int(11) NOT NULL,
  `QuestID` int(11) NOT NULL,
  `DailyBoardID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_quests_list`
--

CREATE TABLE `tw_quests_list` (
  `ID` int(11) NOT NULL,
  `NextQuestID` int(11) DEFAULT NULL,
  `Name` varchar(24) NOT NULL DEFAULT 'Quest name',
  `Money` int(11) NOT NULL,
  `Exp` int(11) NOT NULL,
  `Flags` set('Default','Type daily','Type weekly','Type repeatable') NOT NULL DEFAULT 'Default'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_quests_list`
--

INSERT INTO `tw_quests_list` (`ID`, `NextQuestID`, `Name`, `Money`, `Exp`, `Flags`) VALUES
(1, NULL, 'Bad Dreams and Mornings', 30, 25, 'Default');

-- --------------------------------------------------------

--
-- Структура таблицы `tw_quest_boards`
--

CREATE TABLE `tw_quest_boards` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

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
  `Passive` tinyint(1) NOT NULL DEFAULT 0,
  `ProfessionID` int(11) NOT NULL DEFAULT -1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Дамп данных таблицы `tw_skills_list`
--

INSERT INTO `tw_skills_list` (`ID`, `Name`, `Description`, `Type`, `BoostName`, `BoostValue`, `PercentageCost`, `PriceSP`, `MaxLevel`, `Passive`, `ProfessionID`) VALUES
(1, 'Health turret', 'Creates turret a recovery health ', 1, '+ 10sec life span', 1, 25, 28, 6, 0, -1),
(2, 'Sleepy Gravity', 'Magnet mobs to itself', 3, 'radius', 20, 25, 28, 10, 0, -1),
(3, 'Craft Discount', 'Will give discount on the price of craft items', 0, '% discount gold for craft item', 1, 0, 28, 50, 1, -1),
(4, 'Proficiency with weapons', 'You can perform an automatic fire', 0, 'can perform an auto fire with all types of weapons', 1, 0, 120, 1, 1, -1),
(5, 'Blessing of God of war', 'The blessing restores ammo', 3, '% recovers ammo within a radius of 800', 25, 50, 28, 4, 0, -1),
(6, 'Attack Teleport', 'An attacking teleport that deals damage to all mobs radius', 2, '% your strength', 25, 10, 100, 4, 0, -1),
(7, 'Cure I', 'Restores HP all nearby target\'s.', 1, '% adds a health bonus', 3, 5, 10, 5, 0, -1),
(8, 'Provoke', 'Aggresses mobs in case of weak aggression', 3, 'power of aggression', 150, 30, 40, 2, 0, -1),
(9, 'Last Stand', 'Gegege', 3, 'gegege', 150, 30, 40, 2, 0, -1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_voucher`
--

CREATE TABLE `tw_voucher` (
  `ID` int(11) NOT NULL,
  `Code` varchar(32) NOT NULL,
  `Data` text NOT NULL,
  `Multiple` tinyint(1) NOT NULL DEFAULT 0,
  `ValidUntil` bigint(20) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_voucher_redeemed`
--

CREATE TABLE `tw_voucher_redeemed` (
  `ID` int(11) NOT NULL,
  `VoucherID` int(11) NOT NULL,
  `UserID` int(11) NOT NULL,
  `TimeCreated` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_warehouses`
--

CREATE TABLE `tw_warehouses` (
  `ID` int(11) NOT NULL,
  `Name` varchar(32) NOT NULL DEFAULT '''Bussines name''',
  `Properties` varchar(256) NOT NULL DEFAULT '''{}''',
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `Currency` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Структура таблицы `tw_worlds`
--

CREATE TABLE `tw_worlds` (
  `ID` int(11) NOT NULL,
  `Name` varchar(256) NOT NULL,
  `Path` varchar(256) NOT NULL,
  `Type` enum('default','dungeon','tutorial','') NOT NULL,
  `RespawnWorldID` int(11) NOT NULL,
  `JailWorldID` int(11) NOT NULL,
  `RequiredLevel` int(11) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=latin1 COLLATE=latin1_swedish_ci;

--
-- Дамп данных таблицы `tw_worlds`
--

INSERT INTO `tw_worlds` (`ID`, `Name`, `Path`, `Type`, `RespawnWorldID`, `JailWorldID`, `RequiredLevel`) VALUES
(0, 'main', 'main.map', 'default', 0, 0, 1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_world_swap`
--

CREATE TABLE `tw_world_swap` (
  `ID` int(11) NOT NULL,
  `WorldID` int(11) DEFAULT NULL,
  `PositionX` int(11) DEFAULT NULL,
  `PositionY` int(11) DEFAULT NULL,
  `TwoWorldID` int(11) DEFAULT NULL,
  `TwoPositionX` int(11) DEFAULT NULL,
  `TwoPositionY` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

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
-- Индексы таблицы `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  ADD PRIMARY KEY (`Id`),
  ADD KEY `tw_accounts_bans_tw_accounts_ID_fk` (`AccountId`);

--
-- Индексы таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `Nick` (`Nick`),
  ADD KEY `tw_accounts_data_ibfk_3` (`WorldID`);

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
  ADD KEY `OwnerID` (`UserID`);

--
-- Индексы таблицы `tw_accounts_professions`
--
ALTER TABLE `tw_accounts_professions`
  ADD PRIMARY KEY (`ID`);

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
-- Индексы таблицы `tw_achievements`
--
ALTER TABLE `tw_achievements`
  ADD PRIMARY KEY (`ID`);

--
-- Индексы таблицы `tw_aethers`
--
ALTER TABLE `tw_aethers`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_attributes`
--
ALTER TABLE `tw_attributes`
  ADD PRIMARY KEY (`ID`);

--
-- Индексы таблицы `tw_auction_slots`
--
ALTER TABLE `tw_auction_slots`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `OwnerID` (`OwnerID`),
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
  ADD KEY `QuestID` (`QuestID`),
  ADD KEY `WorldID` (`WorldID`);

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
  ADD KEY `Seconds` (`Lifetime`);

--
-- Индексы таблицы `tw_groups`
--
ALTER TABLE `tw_groups`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_groups_ibfk_1` (`OwnerUID`);

--
-- Индексы таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `OwnerID` (`LeaderUID`),
  ADD KEY `Bank` (`Bank`(768)),
  ADD KEY `Level` (`Level`),
  ADD KEY `Experience` (`Exp`);

--
-- Индексы таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_guilds_decorations_ibfk_2` (`ItemID`),
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
  ADD KEY `DecoID` (`ItemID`);

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
-- Индексы таблицы `tw_positions_farming`
--
ALTER TABLE `tw_positions_farming`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `ItemID` (`ItemID`),
  ADD KEY `WorldID` (`WorldID`);

--
-- Индексы таблицы `tw_quests_board_list`
--
ALTER TABLE `tw_quests_board_list`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `tw_quests_board_list_ibfk_1` (`DailyBoardID`),
  ADD KEY `tw_quests_board_list_ibfk_2` (`QuestID`);

--
-- Индексы таблицы `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  ADD PRIMARY KEY (`ID`),
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `NextQuestID` (`NextQuestID`);

--
-- Индексы таблицы `tw_quest_boards`
--
ALTER TABLE `tw_quest_boards`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`);

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
-- Индексы таблицы `tw_worlds`
--
ALTER TABLE `tw_worlds`
  ADD PRIMARY KEY (`ID`);

--
-- Индексы таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  ADD PRIMARY KEY (`ID`),
  ADD KEY `WorldID` (`WorldID`),
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
  MODIFY `FunctionID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=17;

--
-- AUTO_INCREMENT для таблицы `enum_items_types`
--
ALTER TABLE `enum_items_types`
  MODIFY `TypeID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT для таблицы `tw_accounts`
--
ALTER TABLE `tw_accounts`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  MODIFY `Id` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=10;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_professions`
--
ALTER TABLE `tw_accounts_professions`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=6;

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
-- AUTO_INCREMENT для таблицы `tw_achievements`
--
ALTER TABLE `tw_achievements`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_aethers`
--
ALTER TABLE `tw_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT для таблицы `tw_auction_slots`
--
ALTER TABLE `tw_auction_slots`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=62;

--
-- AUTO_INCREMENT для таблицы `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=22;

--
-- AUTO_INCREMENT для таблицы `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=24;

--
-- AUTO_INCREMENT для таблицы `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=121;

--
-- AUTO_INCREMENT для таблицы `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=26;

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
-- AUTO_INCREMENT для таблицы `tw_groups`
--
ALTER TABLE `tw_groups`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=11;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=13;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_ranks`
--
ALTER TABLE `tw_guilds_ranks`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_houses`
--
ALTER TABLE `tw_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

--
-- AUTO_INCREMENT для таблицы `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_items_list`
--
ALTER TABLE `tw_items_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=104;

--
-- AUTO_INCREMENT для таблицы `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_positions_farming`
--
ALTER TABLE `tw_positions_farming`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_quests_board_list`
--
ALTER TABLE `tw_quests_board_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=40;

--
-- AUTO_INCREMENT для таблицы `tw_quest_boards`
--
ALTER TABLE `tw_quest_boards`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=10;

--
-- AUTO_INCREMENT для таблицы `tw_voucher`
--
ALTER TABLE `tw_voucher`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_voucher_redeemed`
--
ALTER TABLE `tw_voucher_redeemed`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- AUTO_INCREMENT для таблицы `tw_worlds`
--
ALTER TABLE `tw_worlds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=14;

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
-- Ограничения внешнего ключа таблицы `tw_accounts_bans`
--
ALTER TABLE `tw_accounts_bans`
  ADD CONSTRAINT `tw_accounts_bans_tw_accounts_ID_fk` FOREIGN KEY (`AccountId`) REFERENCES `tw_accounts` (`ID`);

--
-- Ограничения внешнего ключа таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  ADD CONSTRAINT `tw_accounts_data_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_accounts_data_ibfk_5` FOREIGN KEY (`ID`) REFERENCES `tw_accounts` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_accounts_mailbox_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_aethers_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_bots_mobs_ibfk_14` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_mobs_ibfk_9` FOREIGN KEY (`it_drop_0`) REFERENCES `tw_items_list` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  ADD CONSTRAINT `tw_bots_npc_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_4` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_npc_ibfk_5` FOREIGN KEY (`GiveQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  ADD CONSTRAINT `tw_bots_quest_ibfk_1` FOREIGN KEY (`BotID`) REFERENCES `tw_bots_info` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_8` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_bots_quest_ibfk_9` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  ADD CONSTRAINT `tw_crafts_list_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_crafts_list_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
-- Ограничения внешнего ключа таблицы `tw_groups`
--
ALTER TABLE `tw_groups`
  ADD CONSTRAINT `tw_groups_ibfk_1` FOREIGN KEY (`OwnerUID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE NO ACTION ON UPDATE NO ACTION;

--
-- Ограничения внешнего ключа таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  ADD CONSTRAINT `tw_guilds_ibfk_1` FOREIGN KEY (`LeaderUID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_4` FOREIGN KEY (`HouseID`) REFERENCES `tw_guilds_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_decorations_ibfk_5` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  ADD CONSTRAINT `tw_guilds_houses_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_houses_ibfk_2` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE SET NULL ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  ADD CONSTRAINT `tw_guilds_invites_ibfk_1` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_invites_ibfk_2` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_houses`
--
ALTER TABLE `tw_houses`
  ADD CONSTRAINT `tw_houses_ibfk_1` FOREIGN KEY (`PlantID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_ibfk_3` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  ADD CONSTRAINT `tw_houses_decorations_ibfk_1` FOREIGN KEY (`HouseID`) REFERENCES `tw_houses` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_decorations_ibfk_2` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_houses_decorations_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_items_list`
--
ALTER TABLE `tw_items_list`
  ADD CONSTRAINT `tw_items_list_ibfk_1` FOREIGN KEY (`Type`) REFERENCES `enum_items_types` (`TypeID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_2` FOREIGN KEY (`Function`) REFERENCES `enum_items_functional` (`FunctionID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_3` FOREIGN KEY (`Attribute0`) REFERENCES `tw_attributes` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_items_list_ibfk_4` FOREIGN KEY (`Attribute1`) REFERENCES `tw_attributes` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_logics_worlds`
--
ALTER TABLE `tw_logics_worlds`
  ADD CONSTRAINT `tw_logics_worlds_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_logics_worlds_ibfk_2` FOREIGN KEY (`ParseInt`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_logics_worlds_ibfk_3` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_positions_farming`
--
ALTER TABLE `tw_positions_farming`
  ADD CONSTRAINT `tw_positions_farming_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_positions_farming_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_positions_mining`
--
ALTER TABLE `tw_positions_mining`
  ADD CONSTRAINT `tw_positions_mining_ibfk_1` FOREIGN KEY (`ItemID`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_quests_board_list`
--
ALTER TABLE `tw_quests_board_list`
  ADD CONSTRAINT `tw_quests_board_list_ibfk_1` FOREIGN KEY (`DailyBoardID`) REFERENCES `tw_quest_boards` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_quests_board_list_ibfk_2` FOREIGN KEY (`QuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_quests_list`
--
ALTER TABLE `tw_quests_list`
  ADD CONSTRAINT `tw_quests_list_ibfk_1` FOREIGN KEY (`NextQuestID`) REFERENCES `tw_quests_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_quest_boards`
--
ALTER TABLE `tw_quest_boards`
  ADD CONSTRAINT `tw_quest_boards_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Ограничения внешнего ключа таблицы `tw_warehouses`
--
ALTER TABLE `tw_warehouses`
  ADD CONSTRAINT `tw_warehouses_ibfk_1` FOREIGN KEY (`Currency`) REFERENCES `tw_items_list` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_warehouses_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`);

--
-- Ограничения внешнего ключа таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  ADD CONSTRAINT `tw_world_swap_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_world_swap_ibfk_2` FOREIGN KEY (`TwoWorldID`) REFERENCES `tw_worlds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
