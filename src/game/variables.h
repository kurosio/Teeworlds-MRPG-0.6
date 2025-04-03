/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times

// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use prediction for objects in the game world")
MACRO_CONFIG_INT(ClPredictPlayers, cl_predict_players, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict movements of other players")
MACRO_CONFIG_INT(ClPredictProjectiles, cl_predict_projectiles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict position of projectiles")
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates")
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates")
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 30, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%")
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup")
MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowChat, cl_showchat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat")
MACRO_CONFIG_INT(ClFilterchat, cl_filterchat, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat messages from: 0=all, 1=friends only, 2=no one")
MACRO_CONFIG_INT(ClShowsocial, cl_showsocial, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show social data like names, clans, chat etc.")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter")
MACRO_CONFIG_INT(ClDisableWhisper, cl_disable_whisper, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable completely the whisper feature.")
MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance")
MACRO_CONFIG_INT(ClDynamicCamera, cl_dynamic_camera, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Switches camera mode. 0=static camera, 1=dynamic camera")
MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zone that doesn't trigger the dynamic camera")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Trigger amount for the dynamic camera")
MACRO_CONFIG_INT(ClMouseMaxDistanceDynamic, cl_mouse_max_distance_dynamic, 1000, 1, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse max distance, in dynamic camera mode")
MACRO_CONFIG_INT(ClMouseMaxDistanceStatic, cl_mouse_max_distance_static, 400, 1, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse max distance, in static camera mode")
MACRO_CONFIG_INT(ClCustomizeSkin, cl_customize_skin, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use a customized skin")
MACRO_CONFIG_INT(ClShowUserId, cl_show_user_id, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show the ID for every user")
MACRO_CONFIG_INT(ClDialogsSpeedNPC, cl_mmo_dialogs_speeed_npc, 80, 80, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "dialoges speed with npc (mrpg)")
MACRO_CONFIG_INT(ClShowMEffects, cl_mmo_effects, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable effects (mrpg)")
MACRO_CONFIG_INT(ClShowAuthMenu, cl_mmo_show_auth_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show auth menu (mrpg)")
MACRO_CONFIG_INT(ClShowVoteColor, cl_mmo_show_vote_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show colored votes in mmo server")
MACRO_CONFIG_STR(GameTexture, mmo_game_texture, 255, "\0", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gameskin texture")
MACRO_CONFIG_STR(GameParticles, mmo_particle_texture, 255, "\0", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Particle texture")
MACRO_CONFIG_STR(GameEmoticons, mmo_emoticons_texture, 255, "\0", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Emoticons texture")
MACRO_CONFIG_STR(GameCursor, mmo_cursor_texture, 255, "\0", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cursor texture")
MACRO_CONFIG_STR(GameEntities, mmo_entities_texture, 255, "\0", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Entities texture")
MACRO_CONFIG_INT(ClGBrowser, cl_gbrowser, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gamer server browser")
MACRO_CONFIG_INT(HdColorProgress, hud_color_progress, 48127, 0, 16777215, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color progress bar hud")
MACRO_CONFIG_INT(ClAdaptivePickups, cl_adaptive_pickups, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make pickups grey when you don't need them")
MACRO_CONFIG_INT(ClHTTPConnectTimeoutMs, cl_http_connect_timeout_ms, 2000, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP downloads: timeout for the connect phase in milliseconds (0 to disable)")
MACRO_CONFIG_INT(ClHTTPLowSpeedLimit, cl_http_low_speed_limit, 500, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP downloads: Set low speed limit in bytes per second (0 to disable)")
MACRO_CONFIG_INT(ClHTTPLowSpeedTime, cl_http_low_speed_time, 5, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP downloads: Set low speed limit time period (0 to disable)")
MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show initial set-up dialog")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 5, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day")
MACRO_CONFIG_INT(ClShowXmasHats, cl_show_xmas_hats, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "0=never, 1=during christmas, 2=always")
MACRO_CONFIG_INT(ClShowEasterEggs, cl_show_easter_eggs, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "0=never, 1=during easter, 2=always")
MACRO_CONFIG_STR(ClVersionServer, cl_version_server, 100, "version.teeworlds.com", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Server to use to check for new versions")
MACRO_CONFIG_STR(ClFontfile, cl_fontfile, 255, "DejaVuSans.ttf", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What font file to use")
MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use")
MACRO_CONFIG_INT(UiBrowserPage, ui_browser_page, 5, 5, 9, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface serverbrowser page")
MACRO_CONFIG_INT(UiSettingsPage, ui_settings_page, 0, 0, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface settings page")
MACRO_CONFIG_STR(UiInternetServerAddress, ui_internet_server_address, 64, "localhost:8303", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface server address (Internet page)")
MACRO_CONFIG_STR(UiLanServerAddress, ui_lan_server_address, 64, "localhost:8303", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface server address (LAN page)")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")
MACRO_CONFIG_INT(UiJoystickSens, ui_joystick_sens, 100, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Joystick sensitivity for menus/editor")
MACRO_CONFIG_INT(UiAutoswitchInfotab, ui_autoswitch_infotab, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Switch to the info tab when clicking on a server")
MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable clipping")
MACRO_CONFIG_STR(ClMenuMap, cl_menu_map, 64, "mmo", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background map in the menu, auto = automatic based on season")
MACRO_CONFIG_INT(ClShowMenuMap, cl_show_menu_map, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Display background map in the menu")
MACRO_CONFIG_INT(ClMenuAlpha, cl_menu_alpha, 40, 0, 75, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Transparency of the menu background")
MACRO_CONFIG_INT(ClRotationRadius, cl_rotation_radius, 30, 1, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Menu camera rotation radius")
MACRO_CONFIG_INT(ClRotationSpeed, cl_rotation_speed, 40, 1, 120, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Menu camera rotations in seconds")
MACRO_CONFIG_INT(ClCameraSpeed, cl_camera_speed, 5, 1, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Menu camera speed")
MACRO_CONFIG_INT(ClShowStartMenuImages, cl_show_start_menu_images, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show start menu images")
MACRO_CONFIG_INT(ClSkipStartMenu, cl_skip_start_menu, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Skip the start menu")
MACRO_CONFIG_INT(ClStatboardInfos, cl_statboard_infos, 1259, 1, 2047, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mask of info to display on the global statboard")

// -----------------------
// Time and Periodic Events Configuration
// -----------------------
MACRO_CONFIG_INT(SvTimePeriodCheckTime, sv_time_period_check_timer, 5, 1, 60, CFGFLAG_SERVER, "Time period check interval (in minutes)")
MACRO_CONFIG_INT(SvInfoChangeDelay, sv_info_change_delay, 5, 0, 9999, CFGFLAG_SERVER, "Delay (in seconds) between info changes (skin/color), set high to avoid rapid changes")

// -----------------------
// Message Configuration
// -----------------------
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")

// -----------------------
// Inactivity and Kick Configuration
// -----------------------
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 3, 0, 1000, CFGFLAG_SERVER, "Time in minutes to wait before handling inactive clients")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 2, 0, 3, CFGFLAG_SERVER, "How to deal with inactive clients (0=move to spectator, 1=move to spectator and kick, 2=kick or move to free spectator slot, 3=kick directly)")

// -----------------------
// Skill Points Configuration
// -----------------------
MACRO_CONFIG_INT(SvSkillPointsPerLevel, sv_skill_points_per_level, 10, 0, 100, CFGFLAG_SERVER, "Number of Skill Points awarded per level up")
MACRO_CONFIG_INT(SvSkillPointsDropChanceMob, sv_skill_points_drop_chance_mob, 5, 0, 100, CFGFLAG_SERVER, "Chance (in percent) for Skill Points to drop from regular mobs")
MACRO_CONFIG_INT(SvSkillPointsDropChanceRareMob, sv_skill_points_drop_chance_rare_mob, 10, 0, 100, CFGFLAG_SERVER, "Chance (in percent) for Skill Points to drop from rare mobs")

// -----------------------
// Teleportation and Interaction Configuration
// -----------------------
MACRO_CONFIG_INT(SvTeleportFeePerDistance, sv_teleport_fee_per_distance, 12, 0, 10000, CFGFLAG_SERVER, "Teleportation fee per unit of world distance")
MACRO_CONFIG_INT(SvDoorRadiusHit, sv_door_radius_hit, 16, 16, 1000, CFGFLAG_SERVER, "Door radius for interaction")

// -----------------------
// Inventory and Resource Management Configuration
// -----------------------
MACRO_CONFIG_INT(SvWarehouseProductsCanTake, sv_warehouse_products_can_take, 50, 10, 100000, CFGFLAG_SERVER, "Maximum amount of products a player can carry from the warehouse")
MACRO_CONFIG_INT(SvGatheringEntitiesPerTile, sv_gathering_entities_per_tile, 2, 1, 4, CFGFLAG_SERVER, "Number of entities that can be gathering from a single tile")
MACRO_CONFIG_INT(SvUpdateEntityTextNames, sv_update_ent_text_names, 50, 25, 1000, CFGFLAG_SERVER, "Frequency of updating entity text names")

MACRO_CONFIG_INT(SvMainQuestActivityCoin, sv_main_quest_activity_coin, 20, 1, 100000, CFGFLAG_SERVER, "Activity coin reward for completing a main quest")
MACRO_CONFIG_INT(SvSideQuestActivityCoin, sv_side_quest_activity_coin, 20, 1, 100000, CFGFLAG_SERVER, "Activity coin reward for completing a side quest")
MACRO_CONFIG_INT(SvRepeatableActivityCoin, sv_repeatable_quest_activity_coin, 5, 1, 100000, CFGFLAG_SERVER, "Activity coin reward for completing a repeatable quest")
MACRO_CONFIG_INT(SvDailyActivityCoin, sv_daily_quest_quest_activity_coin, 100, 1, 100000, CFGFLAG_SERVER, "Activity coin reward for completing a daily quest")
MACRO_CONFIG_INT(SvWeeklyActivityCoin, sv_weekly_quest_activity_coin, 500, 1, 100000, CFGFLAG_SERVER, "Activity coin reward for completing a weekly quest")

MACRO_CONFIG_INT(SvDroppedItemLifetime, sv_dropped_item_lifetime, 20, 5, 60, CFGFLAG_SERVER, "Lifetime dropped item in seconds")

// -----------------------
// Auction System Configuration
// -----------------------
MACRO_CONFIG_INT(SvMaxPlayerAuctionSlots, sv_max_player_auction_slots, 5, 1, 1000, CFGFLAG_SERVER, "Maximum number of auction slots available to a player")
MACRO_CONFIG_INT(SvAuctionSlotTaxRate, sv_auction_slot_tax_rate, 10, 0, 100, CFGFLAG_SERVER, "Tax rate (in percent) for adding a new auction slot")
MACRO_CONFIG_INT(SvMaxAuctionSlots, sv_max_auction_slots, 50, 10, 300, CFGFLAG_SERVER, "Maximum number of total auction slots")

// -----------------------
// Guild System Configuration
// -----------------------
MACRO_CONFIG_INT(SvGuildSlotUpgradePrice, sv_guild_slot_upgrade_price, 4100, 100, 9000000, CFGFLAG_SERVER, "Price for upgrading guild member slots")
MACRO_CONFIG_INT(SvGuildAnotherUpgradePrice, sv_guild_another_upgrade_price, 16800, 100, 9000000, CFGFLAG_SERVER, "Price for upgrading additional guild features")
MACRO_CONFIG_INT(SvGuildWarDurationMinutes, sv_guild_war_duration_minutes, 30, 1, 240, CFGFLAG_SERVER, "Duration of a guild war in minutes")

// -----------------------
// Experience and Gold Configuration
// -----------------------
MACRO_CONFIG_INT(SvMobKillExpFactor, sv_mob_kill_exp_factor, 1000, 100, 5000, CFGFLAG_SERVER, "Experience multiplier for killing mobs")
MACRO_CONFIG_INT(SvChairExpFactor, sv_chair_exp_factor, 10000, 1000, 50000, CFGFLAG_SERVER, "Experience multiplier for using a chair")
MACRO_CONFIG_INT(SvMobKillGoldFactor, sv_mob_kill_gold_factor, 1200, 100, 5000, CFGFLAG_SERVER, "Gold multiplier for killing mobs")
MACRO_CONFIG_INT(SvChairGoldFactor, sv_chair_gold_factor, 12000, 1, 50000, CFGFLAG_SERVER, "Gold multiplier for using a chair")

// -----------------------
// Crafting and Resource Leveling Configuration
// -----------------------
MACRO_CONFIG_INT(SvMiningLevelIncrease, sv_mining_level_increase, 80, 0, 100000, CFGFLAG_SERVER, "Amount of experience required to level up mining")
MACRO_CONFIG_INT(SvFarmingLevelIncrease, sv_farming_level_increase, 80, 0, 100000, CFGFLAG_SERVER, "Amount of experience required to level up farming")
MACRO_CONFIG_INT(SvRaidDungeonExpMultiplier, sv_raid_dungeon_exp_multiplier, 150, 100, 1000, CFGFLAG_SERVER, "Experience multiplier for completing raid dungeons")

// -----------------------
// Death Penalty and Financial Configuration
// -----------------------
MACRO_CONFIG_INT(SvGoldLossOnDeath, sv_gold_loss_on_death, 5, 0, 100, CFGFLAG_SERVER, "Percentage of gold lost upon death")
MACRO_CONFIG_INT(SvArrestGoldOnDeath, sv_arrest_gold_on_death, 30, 0, 100, CFGFLAG_SERVER, "Percentage of gold confiscated upon death from kill list")
MACRO_CONFIG_INT(SvBankCommissionRate, sv_bank_commission_rate, 5, 0, 100, CFGFLAG_SERVER, "Commission rate for bank transactions (in percentage)")

// -----------------------
// Miscellaneous Configuration
// -----------------------
MACRO_CONFIG_INT(SvGenerateMoneyBagPerMinute, sv_generate_money_bag_per_minute, 15, 0, 1020, CFGFLAG_SERVER, "Generate money bag per minute (in minutes)")
MACRO_CONFIG_INT(SvDungeonWaitingTime, sv_dungeon_waiting_time, 180, 0, 1020, CFGFLAG_SERVER, "Waiting time for dungeon entry (in seconds)")
MACRO_CONFIG_INT(SvChatMessageInterval, sv_chat_message_interval, 600, 0, 14400, CFGFLAG_SERVER, "Interval for displaying chat messages (in seconds)")
MACRO_CONFIG_INT(SvChatTopMessageInterval, sv_chat_top_message_interval, 900, 0, 14400, CFGFLAG_SERVER, "Interval for displaying chat toplist messages (in seconds)")
MACRO_CONFIG_INT(SvPlayerPeriodCheckInterval, sv_player_check_interval, 1440, 0, 14400, CFGFLAG_SERVER, "Time interval for checking player status (in seconds)")
MACRO_CONFIG_INT(SvLimitDecoration, sv_limit_decorations, 10, 5, 20, CFGFLAG_SERVER, "Maximum number of decoration objects allowed")
MACRO_CONFIG_STR(SvDiscordInviteLink, sv_discord_invite_link, 32, "nope", CFGFLAG_SERVER, "Link to server invitation")
MACRO_CONFIG_INT(SvIntervalTileTextUpdate, sv_interval_tile_text_update, 1, 1, 60, CFGFLAG_SERVER, "Interval update tile text")
MACRO_CONFIG_INT(SvCrimeIntervalDecrease, sv_crime_interval_decrease, 5, 1, 60, CFGFLAG_SERVER, "Interval decrease crime score")

MACRO_CONFIG_INT(SvMinRating, sv_min_rating, 1000, 1000, 5000, CFGFLAG_SERVER, "Minial rating player")
MACRO_CONFIG_INT(SvMaxRating, sv_max_rating, 2500, 2500, 10000, CFGFLAG_SERVER, "Maximal rating player")
MACRO_CONFIG_INT(SvRatingCoefficientBase, sv_rating_coefficient_base, 40, 5, 100, CFGFLAG_SERVER, "Coefficient rating base")

// mysql
MACRO_CONFIG_STR(SvMySqlHost, sv_sql_host, 32, "localhost", CFGFLAG_SERVER, "MySQL Host")
MACRO_CONFIG_STR(SvMySqlDatabase, sv_sql_database, 32, "database", CFGFLAG_SERVER, "MySQL Database")
MACRO_CONFIG_STR(SvMySqlLogin, sv_sql_login, 32, "root", CFGFLAG_SERVER, "MySQL Login")
MACRO_CONFIG_STR(SvMySqlPassword, sv_sql_password, 32, "", CFGFLAG_SERVER, "MySQL Password")
MACRO_CONFIG_INT(SvMySqlPort, sv_sql_port, 3306, 0, 65000, CFGFLAG_SERVER, "MySQL Port")
MACRO_CONFIG_INT(SvMySqlPoolSize, sv_sql_pool_size, 3, 2, 12, CFGFLAG_SERVER, "MySQL Pool size");

// settings
MACRO_CONFIG_INT(SvLoltextHspace, sv_loltext_hspace, 7, 7, 25, CFGFLAG_SERVER, "horizontal offset between loltext 'pixels'")
MACRO_CONFIG_INT(SvLoltextVspace, sv_loltext_vspace, 7, 7, 25, CFGFLAG_SERVER, "vertical offset between loltext 'pixels'")
MACRO_CONFIG_INT(SvMaxSmoothViewCamSpeed, sv_max_smooth_view_cam_speed, 64, 32, 256, CFGFLAG_SERVER, "Max smooth view cam speed'")

// ui
MACRO_CONFIG_INT(ClNotifyWindow, cl_notify_window, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow client to notify you on chat highlights")
MACRO_CONFIG_INT(ClInactiveRendering, cl_inactive_rendering, 1, 0, 2, CFGFLAG_CLIENT, "0 = Always render, 1 = Stop rendering when minimized, 2 = Stop rendering when window is inactive")
MACRO_CONFIG_INT(SvMapDistanceActveBot, sv_map_distance_active_bot, 1000, 400, 10000, CFGFLAG_SERVER, "max distance for active bot")
MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 5, 1, 100, CFGFLAG_SERVER, "64 player id <-> vanilla id players map update rate")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS - 1, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
#endif
