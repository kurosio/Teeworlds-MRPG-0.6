-- phpMyAdmin SQL Dump
-- version 5.2.0
-- https://www.phpmyadmin.net/
--
-- Хост: 127.0.0.1
-- Время создания: Окт 05 2022 г., 17:26
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
-- Структура таблицы `enum_emotes`
--

CREATE TABLE `enum_emotes` (
  `ID` int(11) NOT NULL,
  `Emote` varchar(64) NOT NULL DEFAULT 'nope'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_emotes`
--

INSERT INTO `enum_emotes` (`ID`, `Emote`) VALUES
(0, 'Normal Emote'),
(1, 'Pain Emote'),
(2, 'Happy Emote'),
(3, 'Surprise Emote'),
(4, 'Angry Emote'),
(5, 'Blink Emote');

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
-- Структура таблицы `enum_mmo_proj`
--

CREATE TABLE `enum_mmo_proj` (
  `ID` int(11) NOT NULL,
  `Name` varchar(64) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_mmo_proj`
--

INSERT INTO `enum_mmo_proj` (`ID`, `Name`) VALUES
(0, 'Magitech Gun'),
(1, 'Magitech Shotgun'),
(2, 'Magitech Grenade'),
(-1, 'No Proj'),
(3, 'Heavenly Gun'),
(4, 'Heavenly Shotgun'),
(5, 'Heavenly Grenade'),
(6, 'Goblin Gun'),
(7, 'Goblin Shotgun'),
(8, 'Goblin Grenade');

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
(2, 'Pick up items that NPC will drop.');

-- --------------------------------------------------------

--
-- Структура таблицы `enum_worlds`
--

CREATE TABLE `enum_worlds` (
  `WorldID` int(11) NOT NULL,
  `Name` varchar(32) CHARACTER SET utf8 NOT NULL,
  `RespawnWorld` int(11) DEFAULT NULL,
  `MusicID` int(11) NOT NULL DEFAULT -1
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Дамп данных таблицы `enum_worlds`
--

INSERT INTO `enum_worlds` (`WorldID`, `Name`, `RespawnWorld`, `MusicID`) VALUES
(0, 'Some1world', NULL, 53),
(1, 'Some2world', 1, 54),
(2, 'Elfinia', 2, 53),
(3, 'Elfinia Deep cave', 2, 54),
(4, 'Elfia home room', 2, 53),
(5, 'Elfinia occupation of goblins', 5, 54),
(6, 'Elfinia Abandoned mine', NULL, 56),
(7, 'Diana home room', 2, 53),
(8, 'Noctis Resonance', NULL, 55),
(9, 'Departure', 9, 53),
(10, 'Underwater of Neptune', 10, 55),
(11, 'Yugasaki', 11, 53);

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

--
-- Дамп данных таблицы `tw_accounts`
--

INSERT INTO `tw_accounts` (`ID`, `Username`, `Password`, `PasswordSalt`, `RegisterDate`, `LoginDate`, `RegisteredIP`, `LoginIP`, `Language`) VALUES
(1, 'kuro', '7dd4cd59370ae03ef3f73b0711cd702df5e1853bf97bf8705244c0c22d759db1', 'TYdfB9M75CLUNGkPCm6PF46C', '2021-10-21 17:26:48', '2022-10-05 22:19:03', '192.168.56.1', '192.168.0.11', 'ru'),
(2, 'kuro', 'bc5905cfaa9e0188814fa2a56ecd6a998a4d39c6345ff9bff296bd5086dee93e', 'g8KmHG6T72CGXjRnn2CBq7Zt', '2021-11-04 08:38:30', '2021-11-04 23:23:59', '192.168.9.2', '192.168.42.35', 'ru'),
(3, 'kuro', 'b46d7846dd83b73171009b49847825aa45abbb37071021d6b4219319a680ba27', 'eVE2pT8AbnS8Lc7AUMLtWRkS', '2021-11-04 12:43:33', '2021-11-04 21:18:05', '192.168.9.2', '192.168.9.2', 'ru'),
(4, 'kuro', 'da6eee14f58fc11fefade37e6974b2fe18259a1619cffc84279a0d3fd0101d1b', 'Yhg56c79pk6LC97TZVjdbLVg', '2021-11-05 06:51:11', '2021-11-05 14:25:01', '192.168.9.2', '192.168.9.2', 'en'),
(5, 'kuro', '60424ee200a9c54748fdbdb8b7cd7df29b651e4d8b4a23752c97a88d2260b2ed', 'YeXACqnVZECdfPpWaeg38NAa', '2022-07-20 12:29:06', '2022-10-01 22:23:09', '192.168.0.10', '192.168.0.10', 'ru'),
(6, 'soska', '433cf6f8aebcb7960384f563972f9c9a06e7210ea53ca07307dde83f407fe093', 'WbGYAeknmmKZLNmBSLcCqMA6', '2022-07-20 14:14:04', '2022-07-20 21:40:16', '192.168.0.10', '192.168.0.10', 'en'),
(7, 'kuro', '15efd964fc5ab19174a5c5d3c650d2b227dbbea5882380a77db935020ea169e5', 'K6hpqc2bmR3jTAMYWbEYUCCD', '2022-08-14 08:29:14', '2022-08-14 15:46:50', '192.168.0.11', '192.168.0.11', 'en'),
(8, '9871爪20001', '2a17bdbb002ddae787cbb1f0d2d78a5081c33ce2b2806cdb633272b7cd2cd1fc', 'ocop8V49U6Me8NKqbG72f95W', '2022-09-22 14:13:20', '2022-09-22 21:13:26', '46.242.12.120', '46.242.12.120', 'ru'),
(9, 'lel1', '252f176ea818de94a38f51570ede8bb28d2745a305c2f6cdff1f79ae219a545e', 'abmSUX5Ta2Pjt9bhb88WbkbP', '2022-09-22 15:58:27', '2022-09-23 00:03:17', '46.147.226.251', '46.147.226.251', 'en'),
(10, 'test', '36235aabe710994638d3d920298d4c9b8207bcc094846d7033b6903bf4e58bf1', 'qmUfHhj3nT2LZp8X2XtALAc2', '2022-09-22 16:32:49', '2022-10-03 21:29:16', '138.199.47.239', '91.207.103.217', 'en'),
(11, 'Hanuko33', 'fb757c3a351dcc37315fe17953d28f1999a4e79f5505a34bc39ced9c12823a11', 'eEdPcLPBaqqBAHLR7RDLVD6S', '2022-10-01 14:48:42', '2022-10-01 21:48:48', '176.221.122.121', '176.221.122.121', 'en'),
(12, 'doruk', 'ffd5646688c0366c5f96230634c239337a593dc3ffafbd3229883011872505a8', 't9YemkDEVZXP7AFLaaq53RNU', '2022-10-01 17:26:49', '2022-10-02 00:26:56', '78.190.156.218', '78.190.156.218', 'en'),
(13, 'cloud', '08a4d250c0de056ba2592c22ab861eca910cdc0abcfef2e8cd5fc36905fe36d0', 'gLpTPVEMXhR2ameYNgcFhmC3', '2022-10-01 19:33:30', '2022-10-02 02:33:37', '77.23.250.44', '77.23.250.44', 'en'),
(14, 'kronos', 'bfeac966c377ce450e9e7d60da99b9164beab4f2ad18b0c8d5dce0737f396cd6', 'bD6gon3NcFBaGVAbtXGTPFZE', '2022-10-01 21:42:30', '2022-10-02 04:44:41', '46.198.179.144', '46.198.179.144', 'en'),
(15, 'komut', '72f31dba66c336a1f0985c4262c8f226798dee446bc13513b0f8ed7934bd042d', 'AtoP7L8KFLqXcBagp7b6ET43', '2022-10-02 09:53:27', '2022-10-02 16:53:29', '87.152.160.13', '87.152.160.13', 'en'),
(16, 'nolhan', '170cf5c0081c8578c92b81a1b402c25f807a2012d717ab5e4036b68ba9503ad1', '2S2odDWYGpCtcWEgATHgEVn9', '2022-10-02 12:07:12', '2022-10-02 19:07:20', '90.51.152.221', '90.51.152.221', 'en'),
(17, '1234', '4c34add7003faa76af44c9c94730769ce04d39eabdf8d5f109c509acceb91e82', 'h4dBSSpGaKkd2Tq7h6oCcYeK', '2022-10-02 14:48:30', '2022-10-02 21:48:37', '88.236.65.116', '88.236.65.116', 'en'),
(18, '<...>', 'd7b36c822d9b1a9d0775d999b04d420cb264c5ce83435bc8f195ba429a094e3a', 'GYGBF7A4hpUCj8N55Npb2WWF', '2022-10-03 06:09:55', '2022-10-03 13:10:09', '2.61.207.196', '2.61.207.196', 'en'),
(19, 'SpIdEr', '0bb31d1777d4f6f56cd6ada9d5925bc497666adb4dccacd65c4f529a4746f308', 'DkYWnMUGMh73GWobRqD3LbgH', '2022-10-03 11:38:48', '2022-10-03 22:21:41', '185.3.32.208', '185.3.32.208', 'en'),
(20, 'vlad', '31a4b5319f3973690797c94ad2dc2e5c0ae0da1ce136457d15267a105325adfb', 'f4gXXdNkbYWYEYFcHEkm4jSn', '2022-10-03 12:23:15', '2022-10-03 19:29:40', '46.147.226.251', '46.147.226.251', 'en'),
(21, 'Dorito', '5dfbafadc8aa194425df825b94b1b6e8d6aee700d65bbb177fd0307644fa05de', 'Cf6hCm44WZ5Z6VjtPGNeogSf', '2022-10-03 12:49:01', '2022-10-03 19:49:21', '91.105.177.78', '91.105.177.78', 'en'),
(22, 'Yamete', '3fa5df9f295e0519900f91830e0201fed422a1c2636e9963154d0cf782d20479', 'fgFcRnM2fbXjL82FL5c64A5X', '2022-10-03 13:00:54', '2022-10-03 20:01:04', '176.59.205.208', '176.59.205.208', 'en'),
(23, 'kuro', '6d76332b95dbae4536ea909b71a07a97920f5233d3430576491cea6bcb0b3d2b', 'ENmCUbCWt7FKpUBUDScLgoNY', '2022-10-03 13:33:54', '2022-10-04 20:54:45', '192.168.0.10', '192.168.0.11', 'en'),
(24, 'login', '62042966243b0d3b5e3fc14aa192ad4385723e83fc185ce294ea49b005875814', '2EjBmnVM47aZqn3R3gt9ACon', '2022-10-03 14:33:55', '2022-10-03 21:34:04', '89.77.152.236', '89.77.152.236', 'en'),
(25, 'androoha23', 'ca7e97f3edd72431362f699289d157a9b7ddf7703849668fe7d504ae88eddf7d', 'ESa34t4Pp5Dokc49dDYDLp6o', '2022-10-03 15:19:16', 'First log in', '188.190.180.188', '0.0.0.0', 'en'),
(26, 'AvOcaDO', 'cd7e122f4a8d4336c1dd5f5525dea5706d2fc0e608d526461ed4e65e25e33a63', '5Dcdc3ppEFa2F2FePaTRofpV', '2022-10-03 15:35:21', '2022-10-03 22:35:32', '185.3.32.208', '185.3.32.208', 'en'),
(27, 'Wrawel', '5f4062f1f5d3303bae368e6ed0f3c55af776544c03b1e8632e8dc2ab9b735dfa', 'afmFgoPTPCekXRae6o3bPPoB', '2022-10-03 16:51:35', '2022-10-03 23:51:39', '77.64.253.139', '77.64.253.139', 'en'),
(28, 'flash429ea', 'c35c493ae9fbd1586caa12bc111b6eb3f07a2041da13655c9585a4c2e49ab138', 'jW7WaXaZBGZSd3H9AWtP3kW3', '2022-10-04 13:31:54', '2022-10-04 23:59:20', '78.162.148.103', '78.162.148.103', 'en'),
(29, 'kuro', '36fd88d7862eb163f6b8eaee9904bab8644db2922d7aff330e12b51eeac75e75', 'LednaqtRd5C72qhKeTtGNRcL', '2022-10-04 14:07:15', '2022-10-04 21:48:25', '192.168.0.11', '192.168.0.11', 'en');

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

--
-- Дамп данных таблицы `tw_accounts_data`
--

INSERT INTO `tw_accounts_data` (`ID`, `Nick`, `DiscordID`, `WorldID`, `Level`, `Exp`, `GuildID`, `GuildDeposit`, `GuildRank`, `Upgrade`, `SpreadShotgun`, `SpreadGrenade`, `SpreadRifle`, `Dexterity`, `CriticalHit`, `DirectCriticalHit`, `Hardness`, `Tenacity`, `Lucky`, `Piety`, `Vampirism`, `AmmoRegen`, `Ammo`, `Efficiency`, `Extraction`) VALUES
(1, 'kurosio', '571251558457540617', 11, 34, 18665, NULL, 0, NULL, 13893, 8, 1, 5, 2, 2, 1, 10000, 0, 1, 1, 1, 10000, 30, 0, 0),
(2, 'kurosio1', 'null', 2, 3, 99, NULL, 0, NULL, 20, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(3, 'kurosio12', 'null', 0, 2, 2, NULL, 0, NULL, 10, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(4, 'kurosio44', 'null', 7, 4, 156, NULL, 0, NULL, 30, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(5, 'missie', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(6, '[D] missie', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(7, 'missiedsadA♪', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(8, '9871爪20001', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(9, '<~{Barsik}~>', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(10, 'Anime.pdf', 'null', 0, 344, 1633919, 1, 0, NULL, 0, 13, 10, 3, 30, 100, 100, 10000, 0, 0, 0, 100, 100, 50, 0, 0),
(11, 'Hanuko33', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(12, 'N L K $', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(13, 'Cloud', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(14, 'Ļåҕ | Kron0s', 'null', NULL, 2, 2, NULL, 0, NULL, 10, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(15, 'Hawk', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(16, 'Mel!odas.*', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(17, 'Akashi', 'null', 0, 2, 7, NULL, 0, NULL, 10, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(18, 'lld', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(19, 'SpIdEr', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(20, 'barski asshole', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(21, 'Dorito', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(22, 'd3vilsss72', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(23, 'emissie', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(24, 'axe', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(25, 'Aver_top', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(26, 'AvOcaDO', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(27, 'Wrawel', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(28, 'LewuArda07', 'null', NULL, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
(29, 'Kurosioы', 'null', 0, 1, 0, NULL, 0, NULL, 0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_farming`
--

CREATE TABLE `tw_accounts_farming` (
  `UserID` int(11) NOT NULL,
  `Level` int(11) NOT NULL DEFAULT 1,
  `Exp` int(11) NOT NULL DEFAULT 0,
  `Quantity` int(11) NOT NULL DEFAULT 1,
  `Upgrade` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_accounts_farming`
--

INSERT INTO `tw_accounts_farming` (`UserID`, `Level`, `Exp`, `Quantity`, `Upgrade`) VALUES
(1, 1, 13, 1, 0),
(2, 1, 2, 1, 0),
(3, 1, 0, 1, 0),
(4, 1, 0, 1, 0),
(5, 1, 0, 1, 0),
(6, 1, 0, 1, 0),
(7, 1, 0, 1, 0),
(8, 1, 0, 1, 0),
(9, 1, 0, 1, 0),
(10, 1, 0, 1, 0),
(11, 1, 0, 1, 0),
(12, 1, 0, 1, 0),
(13, 1, 0, 1, 0),
(14, 1, 0, 1, 0),
(15, 1, 0, 1, 0),
(16, 1, 0, 1, 0),
(17, 1, 0, 1, 0),
(18, 1, 0, 1, 0),
(19, 1, 0, 1, 0),
(20, 1, 0, 1, 0),
(21, 1, 0, 1, 0),
(22, 1, 0, 1, 0),
(23, 1, 0, 1, 0),
(24, 1, 0, 1, 0),
(26, 1, 0, 1, 0),
(27, 1, 0, 1, 0),
(28, 1, 0, 1, 0),
(29, 1, 0, 1, 0);

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

--
-- Дамп данных таблицы `tw_accounts_items`
--

INSERT INTO `tw_accounts_items` (`ID`, `ItemID`, `Value`, `Settings`, `Enchant`, `Durability`, `UserID`) VALUES
(1, 2, 3, 1, 0, 100, 1),
(2, 22, 4, 3, 0, 100, 1),
(6, 1, 12338, 0, 0, 100, 1),
(10, 9, 8363, 0, 0, 100, 1),
(21, 18, 10090, 0, 0, 100, 1),
(27, 15, 9793, 0, 0, 100, 1),
(34, 13, 1, 0, 0, 100, 1),
(38, 23, 2, 0, 0, 100, 1),
(49, 7, 241, 0, 0, 100, 1),
(57, 2, 1, 1, 0, 100, 2),
(58, 22, 1, 1, 0, 100, 2),
(61, 3, 1, 1, 0, 100, 2),
(62, 1, 179, 0, 0, 100, 2),
(65, 2, 1, 1, 0, 100, 3),
(66, 22, 1, 1, 0, 100, 3),
(69, 3, 1, 1, 0, 100, 3),
(70, 1, 15, 0, 0, 100, 3),
(73, 16, 144, 0, 0, 100, 1),
(74, 2, 1, 1, 0, 100, 4),
(75, 22, 1, 1, 0, 100, 4),
(78, 3, 1, 1, 0, 100, 4),
(79, 1, 317, 0, 0, 100, 4),
(81, 9, 2, 0, 0, 100, 4),
(85, 2, 1, 1, 0, 100, 5),
(86, 22, 1, 1, 0, 100, 5),
(87, 2, 1, 1, 0, 100, 6),
(88, 22, 1, 1, 0, 100, 6),
(89, 10, 1, 0, 0, 100, 5),
(90, 12, 1002, 0, 0, 100, 5),
(91, 13, 1, 0, 0, 100, 5),
(92, 14, 1, 0, 0, 100, 5),
(95, 7, 101, 0, 0, 100, 5),
(97, 1, 1, 0, 0, 100, 5),
(98, 3, 1, 1, 0, 100, 5),
(99, 4, 1, 1, 0, 100, 5),
(100, 5, 1, 1, 2, 100, 5),
(101, 6, 1, 1, 0, 100, 5),
(102, 9, 876, 0, 0, 100, 5),
(103, 15, 1000, 0, 0, 100, 5),
(109, 18, 1000, 0, 0, 100, 5),
(113, 2, 1, 1, 0, 100, 7),
(114, 22, 1, 1, 0, 100, 7),
(115, 2, 1, 1, 0, 100, 8),
(116, 22, 1, 1, 0, 100, 8),
(117, 4, 1, 1, 0, 100, 8),
(119, 15, 13, 0, 0, 100, 8),
(120, 20, 1, 1, 0, 100, 8),
(121, 2, 1, 1, 0, 100, 9),
(122, 22, 1, 1, 0, 100, 9),
(125, 2, 1, 1, 2, 100, 10),
(126, 22, 1, 1, 0, 100, 10),
(127, 1, 1353, 0, 0, 100, 10),
(149, 25, 4, 0, 0, 100, 1),
(150, 2, 1, 1, 0, 100, 11),
(151, 22, 1, 1, 0, 100, 11),
(152, 25, 1, 0, 0, 100, 11),
(153, 25, 1, 0, 0, 100, 5),
(157, 2, 1, 1, 0, 100, 12),
(158, 22, 1, 1, 0, 100, 12),
(159, 25, 1, 0, 0, 100, 12),
(160, 2, 1, 1, 0, 100, 13),
(161, 22, 1, 1, 0, 100, 13),
(162, 25, 1, 0, 0, 100, 13),
(163, 2, 1, 1, 0, 100, 14),
(164, 22, 1, 1, 0, 100, 14),
(165, 25, 1, 0, 0, 100, 14),
(166, 4, 1, 1, 10, 100, 14),
(167, 20, 1, 1, 1, 100, 14),
(168, 15, 25, 0, 0, 100, 14),
(169, 14, 10, 0, 0, 100, 14),
(170, 1, 7, 0, 0, 100, 14),
(171, 2, 1, 1, 0, 100, 15),
(172, 22, 1, 1, 0, 100, 15),
(173, 25, 1, 0, 0, 100, 15),
(175, 2, 1, 1, 0, 100, 16),
(176, 22, 1, 1, 0, 100, 16),
(177, 25, 1, 0, 0, 100, 16),
(178, 2, 1, 1, 0, 100, 17),
(179, 22, 1, 1, 0, 100, 17),
(180, 25, 1, 0, 0, 100, 17),
(181, 5, 1, 1, 1, 100, 17),
(182, 3, 1, 1, 0, 100, 17),
(183, 6, 1, 1, 0, 100, 17),
(184, 15, 300, 0, 0, 100, 17),
(185, 19, 1, 1, 0, 100, 17),
(190, 8, 1, 0, 0, 100, 1),
(191, 10, 1, 0, 0, 100, 1),
(192, 19, 1, 1, 0, 100, 1),
(194, 4, 1, 1, 0, 100, 17),
(195, 20, 1, 1, 0, 100, 17),
(196, 20, 1, 1, 0, 100, 1),
(198, 1, 7, 0, 0, 100, 17),
(199, 26, 1, 1, 1, 100, 1),
(201, 2, 1, 1, 0, 100, 18),
(202, 22, 1, 1, 0, 100, 18),
(203, 25, 1, 0, 0, 100, 18),
(204, 18, 4, 0, 0, 100, 18),
(219, 2, 1, 1, 0, 100, 19),
(220, 22, 1, 0, 0, 100, 19),
(221, 25, 1, 0, 0, 100, 19),
(222, 18, 1, 0, 0, 100, 19),
(223, 2, 1, 1, 0, 100, 20),
(224, 22, 1, 1, 0, 100, 20),
(225, 25, 1, 0, 0, 100, 20),
(226, 1, 7, 0, 0, 100, 20),
(227, 2, 1, 1, 0, 100, 21),
(228, 22, 1, 1, 0, 100, 21),
(229, 25, 1, 0, 0, 100, 21),
(230, 2, 1, 1, 0, 100, 22),
(231, 22, 1, 1, 0, 100, 22),
(232, 25, 1, 0, 0, 100, 22),
(233, 2, 1, 1, 0, 100, 23),
(234, 22, 1, 1, 0, 100, 23),
(235, 25, 1, 0, 0, 100, 23),
(237, 25, 2, 0, 0, 100, 10),
(238, 5, 1, 1, 5, 100, 10),
(244, 3, 1, 1, 5, 100, 10),
(245, 4, 1, 1, 3, 100, 10),
(246, 6, 1, 1, 5, 100, 10),
(248, 7, 988075, 0, 0, 100, 10),
(249, 19, 1, 1, 0, 100, 10),
(250, 20, 1, 1, 0, 100, 10),
(251, 9, 1000000, 0, 0, 100, 10),
(254, 14, 996, 0, 0, 100, 10),
(255, 18, 999, 0, 0, 100, 10),
(256, 15, 953, 0, 0, 100, 10),
(257, 2, 1, 1, 0, 100, 24),
(258, 22, 1, 1, 0, 100, 24),
(259, 25, 1, 0, 0, 100, 24),
(260, 2, 1, 1, 0, 100, 26),
(261, 22, 1, 1, 0, 100, 26),
(262, 25, 1, 0, 0, 100, 26),
(263, 2, 1, 1, 0, 100, 27),
(264, 22, 1, 1, 0, 100, 27),
(265, 25, 1, 0, 0, 100, 27),
(283, 3, 1, 1, 0, 100, 1),
(284, 6, 1, 1, 2, 100, 1),
(286, 4, 1, 1, 5, 100, 1),
(292, 17, 1, 0, 0, 100, 1);

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
  `Upgrade` int(11) NOT NULL DEFAULT 0,
  `Quantity` int(11) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;

--
-- Дамп данных таблицы `tw_accounts_mining`
--

INSERT INTO `tw_accounts_mining` (`UserID`, `Level`, `Exp`, `Upgrade`, `Quantity`) VALUES
(1, 1, 0, 0, 1),
(2, 1, 0, 0, 1),
(3, 1, 0, 0, 1),
(4, 1, 0, 0, 1),
(5, 1, 0, 0, 1),
(6, 1, 0, 0, 1),
(7, 1, 0, 0, 1),
(8, 1, 0, 0, 1),
(9, 1, 0, 0, 1),
(10, 1, 0, 0, 1),
(11, 1, 0, 0, 1),
(12, 1, 0, 0, 1),
(13, 1, 0, 0, 1),
(14, 1, 0, 0, 1),
(15, 1, 0, 0, 1),
(16, 1, 0, 0, 1),
(17, 1, 0, 0, 1),
(18, 1, 0, 0, 1),
(19, 1, 0, 0, 1),
(20, 1, 0, 0, 1),
(21, 1, 0, 0, 1),
(22, 1, 0, 0, 1),
(23, 1, 0, 0, 1),
(24, 1, 0, 0, 1),
(26, 1, 0, 0, 1),
(27, 1, 0, 0, 1),
(28, 1, 0, 0, 1),
(29, 1, 0, 0, 1);

-- --------------------------------------------------------

--
-- Структура таблицы `tw_accounts_quests`
--

CREATE TABLE `tw_accounts_quests` (
  `ID` int(11) NOT NULL,
  `QuestID` int(11) DEFAULT NULL,
  `UserID` int(11) NOT NULL,
  `Step` int(11) NOT NULL DEFAULT 1,
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

--
-- Дамп данных таблицы `tw_accounts_skills`
--

INSERT INTO `tw_accounts_skills` (`ID`, `SkillID`, `UserID`, `Level`, `UsedByEmoticon`) VALUES
(1, 1, 1, 8, -1),
(2, 2, 1, 10, -1),
(3, 5, 1, 4, -1),
(4, 6, 1, 4, 0),
(5, 1, 5, 1, 0),
(6, 6, 5, 1, 1),
(7, 4, 1, 1, -1),
(8, 3, 1, 26, -1);

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
(1, 'Sailor', '{\"skin\": \"greensward\",\"custom_color\": 0,\"color_body\": 0,\"color_feet\": 0}', NULL, NULL, NULL, NULL, NULL, NULL),
(2, 'Worker', '{\"skin\": \"default\",\"custom_color\": 0,\"color_body\": 0,\"color_feet\": 0}', NULL, NULL, NULL, NULL, NULL, NULL),
(3, 'Goblin', '{\"skin\": \"default\",\"custom_color\": 0,\"color_body\": 0,\"color_feet\": 0}', NULL, NULL, NULL, NULL, NULL, NULL);

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

INSERT INTO `tw_bots_mobs` (`ID`, `BotID`, `WorldID`, `PositionX`, `PositionY`, `Effect`, `Behavior`, `Level`, `Power`, `Spread`, `Number`, `Respawn`, `Boss`, `it_drop_0`, `it_drop_1`, `it_drop_2`, `it_drop_3`, `it_drop_4`, `it_drop_count`, `it_drop_chance`) VALUES
(1, 3, 0, 5449, 1297, NULL, 'Standard', 12, 60, 1, 10, 1, 0, NULL, NULL, NULL, NULL, NULL, '|1|0|0|0|0|', '|3.96|0|0|0|0|');

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
  `DialogData` varchar(4096) NOT NULL DEFAULT 'empty',
  `Function` int(11) NOT NULL DEFAULT -1,
  `Static` int(11) NOT NULL,
  `Emote` int(11) NOT NULL DEFAULT 0 COMMENT '1.Pain 2.Happy 3.Surprise 4.Angry 5.Blink	',
  `Number` int(11) NOT NULL DEFAULT 1,
  `WorldID` int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_bots_npc`
--

INSERT INTO `tw_bots_npc` (`ID`, `BotID`, `PosX`, `PosY`, `GiveQuestID`, `DialogData`, `Function`, `Static`, `Emote`, `Number`, `WorldID`) VALUES
(1, 1, 1022, 1073, NULL, '[{\"text\":\"**He looks very tired**: Very hot, impossible to work.\",\"emote\":\"pain\"}]', -1, 0, 5, 2, 0),
(2, 2, 5312, 1073, NULL, 'empty', -1, 1, 0, 1, 0);

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
  `GenerateSubName` tinyint(4) NOT NULL DEFAULT 0,
  `PosX` int(11) NOT NULL,
  `PosY` int(11) NOT NULL,
  `DialogData` varchar(4096) NOT NULL DEFAULT 'empty',
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
(1, 16, 10, '{\"items\":[{\"id\": 1, \"value\": 12}]}', 100, 0),
(2, 4, 10, '{\"items\":[{\"id\": 1, \"value\": 1}, {\"id\": 2, \"value\": 5}]}', 100, 0);

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

--
-- Дамп данных таблицы `tw_guilds`
--

INSERT INTO `tw_guilds` (`ID`, `Name`, `UserID`, `Level`, `Experience`, `Bank`, `Score`, `AvailableSlots`, `ChairExperience`, `ChairMoney`) VALUES
(1, 'Krutie', 10, 1, 7, 0, 0, 2, 1, 1);

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
(7, 'Material', 'Required to improve weapons', 4, -1, 10, 0, NULL, NULL, 0, 0),
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
(27, 'Rusty Pickaxe', 'The usual rusty pickaxe.', 6, 5, 10, 50, 14, NULL, 3, 0);

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
(1, 7, 1, 100, 2254, 872, 300, 0);

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
(1, 'Health turret', 'Creates turret a recovery health ', 1, 'life span', 3, 25, 24, 8, 0),
(2, 'Sleepy Gravity', 'Magnet mobs to itself', 3, 'radius', 20, 25, 28, 10, 0),
(3, 'Craft Discount', 'Will give discount on the price of craft items', 0, '% discount gold for craft item', 1, 0, 28, 50, 1),
(4, 'Proficiency with weapons', 'You can perform an automatic fire', 0, 'can perform an auto fire with all types of weapons', 1, 0, 120, 1, 1),
(5, 'Blessing of God of war', 'The blessing restores ammo', 3, '% recovers ammo within a radius of 800', 25, 50, 28, 4, 0),
(6, 'Attack Teleport', 'An attacking teleport that deals damage to all mobs radius', 2, '% your strength', 25, 10, 100, 4, 0);

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
  `WorldID` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

--
-- Дамп данных таблицы `tw_warehouses`
--

INSERT INTO `tw_warehouses` (`ID`, `Name`, `PosX`, `PosY`, `Currency`, `WorldID`) VALUES
(1, '\'Sososos\'', 4307, 625, 1, 0);

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
(3, 15, 1, 1, 50, 0, 0, 1, '2022-10-04 20:13:02');

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
-- Индексы таблицы `enum_emotes`
--
ALTER TABLE `enum_emotes`
  ADD PRIMARY KEY (`ID`);

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
-- Индексы таблицы `enum_mmo_proj`
--
ALTER TABLE `enum_mmo_proj`
  ADD KEY `ID` (`ID`);

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
  ADD UNIQUE KEY `ID` (`ID`),
  ADD KEY `WorldID` (`WorldID`),
  ADD KEY `TwoWorldID` (`TwoWorldID`),
  ADD KEY `tw_world_swap_ibfk_3` (`RequiredQuestID`);

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
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=30;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_aethers`
--
ALTER TABLE `tw_accounts_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_data`
--
ALTER TABLE `tw_accounts_data`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=30;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_items`
--
ALTER TABLE `tw_accounts_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=293;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_mailbox`
--
ALTER TABLE `tw_accounts_mailbox`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_quests`
--
ALTER TABLE `tw_accounts_quests`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_accounts_skills`
--
ALTER TABLE `tw_accounts_skills`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=9;

--
-- AUTO_INCREMENT для таблицы `tw_aethers`
--
ALTER TABLE `tw_aethers`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_auction_items`
--
ALTER TABLE `tw_auction_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_bots_info`
--
ALTER TABLE `tw_bots_info`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=5;

--
-- AUTO_INCREMENT для таблицы `tw_bots_mobs`
--
ALTER TABLE `tw_bots_mobs`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_bots_npc`
--
ALTER TABLE `tw_bots_npc`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_bots_quest`
--
ALTER TABLE `tw_bots_quest`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_crafts_list`
--
ALTER TABLE `tw_crafts_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=3;

--
-- AUTO_INCREMENT для таблицы `tw_dungeons`
--
ALTER TABLE `tw_dungeons`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_dungeons_door`
--
ALTER TABLE `tw_dungeons_door`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_dungeons_records`
--
ALTER TABLE `tw_dungeons_records`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds`
--
ALTER TABLE `tw_guilds`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_decorations`
--
ALTER TABLE `tw_guilds_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_history`
--
ALTER TABLE `tw_guilds_history`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_guilds_houses`
--
ALTER TABLE `tw_guilds_houses`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

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
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_houses_decorations`
--
ALTER TABLE `tw_houses_decorations`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_items_list`
--
ALTER TABLE `tw_items_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=28;

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
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT для таблицы `tw_skills_list`
--
ALTER TABLE `tw_skills_list`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=7;

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
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=2;

--
-- AUTO_INCREMENT для таблицы `tw_warehouse_items`
--
ALTER TABLE `tw_warehouse_items`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=4;

--
-- AUTO_INCREMENT для таблицы `tw_world_swap`
--
ALTER TABLE `tw_world_swap`
  MODIFY `ID` int(11) NOT NULL AUTO_INCREMENT;

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
  ADD CONSTRAINT `tw_bots_npc_ibfk_3` FOREIGN KEY (`Emote`) REFERENCES `enum_emotes` (`ID`) ON DELETE NO ACTION ON UPDATE NO ACTION,
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
-- Ограничения внешнего ключа таблицы `tw_guilds_invites`
--
ALTER TABLE `tw_guilds_invites`
  ADD CONSTRAINT `tw_guilds_invites_ibfk_1` FOREIGN KEY (`UserID`) REFERENCES `tw_accounts_data` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `tw_guilds_invites_ibfk_2` FOREIGN KEY (`GuildID`) REFERENCES `tw_guilds` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_houses_ibfk_2` FOREIGN KEY (`WorldID`) REFERENCES `enum_worlds` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE;

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
  ADD CONSTRAINT `tw_logics_worlds_ibfk_1` FOREIGN KEY (`WorldID`) REFERENCES `tw_world_swap` (`WorldID`) ON DELETE CASCADE ON UPDATE CASCADE,
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
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
