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

// MRPG
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

// dune
MACRO_CONFIG_INT(ClAdaptivePickups, cl_adaptive_pickups, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make pickups grey when you don't need them")

// curl http download
MACRO_CONFIG_INT(ClHTTPConnectTimeoutMs, cl_http_connect_timeout_ms, 2000, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP downloads: timeout for the connect phase in milliseconds (0 to disable)")
MACRO_CONFIG_INT(ClHTTPLowSpeedLimit, cl_http_low_speed_limit, 500, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP downloads: Set low speed limit in bytes per second (0 to disable)")
MACRO_CONFIG_INT(ClHTTPLowSpeedTime, cl_http_low_speed_time, 5, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HTTP downloads: Set low speed limit time period (0 to disable)")

MACRO_CONFIG_INT(EdZoomTarget, ed_zoom_target, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zoom to the current mouse target")
MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Editor shows which keys are pressed")
MACRO_CONFIG_INT(EdColorGridInner, ed_color_grid_inner, 0xFFFFFF26, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color inner grid")
MACRO_CONFIG_INT(EdColorGridOuter, ed_color_grid_outer, 0xFF4C4C4C, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color outer grid")
MACRO_CONFIG_INT(EdColorQuadPoint, ed_color_quad_point, 0xFF0000FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of quad points")
MACRO_CONFIG_INT(EdColorQuadPointHover, ed_color_quad_point_hover, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of quad points when hovering over with the mouse cursor")
MACRO_CONFIG_INT(EdColorQuadPointActive, ed_color_quad_point_active, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of active quad points")
MACRO_CONFIG_INT(EdColorQuadPivot, ed_color_quad_pivot, 0x00FF00FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the quad pivot")
MACRO_CONFIG_INT(EdColorQuadPivotHover, ed_color_quad_pivot_hover, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the quad pivot when hovering over with the mouse cursor")
MACRO_CONFIG_INT(EdColorQuadPivotActive, ed_color_quad_pivot_active, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the active quad pivot")
MACRO_CONFIG_INT(EdColorSelectionQuad, ed_color_selection_quad, 10492639, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the selection area for a quad")
MACRO_CONFIG_INT(EdColorSelectionTile, ed_color_selection_tile, 5500000, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the selection area for a tile")

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

// server
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")
MACRO_CONFIG_INT(SvInfoChangeDelay, sv_info_change_delay, 5, 0, 9999, CFGFLAG_SERVER, "The time in seconds between info changes (skin/color), to avoid ranbow mod set this to a very high time")

MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive clients")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 2, 0, 3, CFGFLAG_SERVER, "How to deal with inactive clients (0=move player to spectator, 1=move player to spectator and kick spectator, 2=move to free spectator slot/kick, 3=kick)")

// another config
MACRO_CONFIG_INT(SvPriceTeleport, sv_price_teleport, 12, 0, 10000, CFGFLAG_SERVER, "Price for teleport*WorldID")
MACRO_CONFIG_INT(SvDoorRadiusHit, sv_door_radius_hit, 16, 16, 1000, CFGFLAG_SERVER, "Door radius hit.")

// auction
MACRO_CONFIG_INT(SvMaxAuctionSlots, sv_amax_slots, 5, 1, 1000, CFGFLAG_SERVER, "Max autction slots")
MACRO_CONFIG_INT(SvAuctionPriceSlot, sv_apriceslot, 40, 0, 100000, CFGFLAG_SERVER, "Price for added new slot auction")
MACRO_CONFIG_INT(SvTimeAuctionSlot, sv_atimeslot, 1440, 5, 100000, CFGFLAG_SERVER, "Time in minutes for auction end slot")
MACRO_CONFIG_INT(SvMaxMasiveAuctionSlots, sv_amax_masslot, 50, 10, 300, CFGFLAG_SERVER, "Max massive auction slots")
MACRO_CONFIG_INT(SvTimeCheckAuction, sv_achecktime, 30, 1, 300, CFGFLAG_SERVER, "Minutes auction checks")

// member group
MACRO_CONFIG_INT(SvPriceUpgradeGuildSlot, sv_price_member_slot, 4100, 100, 9000000, CFGFLAG_SERVER, "Price for upgrade member slots")
MACRO_CONFIG_INT(SvPriceUpgradeGuildAnother, sv_price_member_another, 16800, 100, 9000000, CFGFLAG_SERVER, "Price for upgrade member another")

// leveling
MACRO_CONFIG_INT(SvKillmobsIncreaseLevel, sv_kill_mobs_increase_level, 400, 0, 1000, CFGFLAG_SERVER, "Kill mobs of own level to increase.")
MACRO_CONFIG_INT(SvMiningIncreaseLevel, sv_mining_increase_level, 80, 0, 1000, CFGFLAG_SERVER, "Mining items own level to increase")
MACRO_CONFIG_INT(SvPlantingIncreaseLevel, sv_planting_increase_level, 80, 0, 10000, CFGFLAG_SERVER, "Planting items own level to increase")
MACRO_CONFIG_INT(SvMultiplierExpRaidDungeon, sv_multiplier_exp_raid_dungeon, 150, 100, 1000, CFGFLAG_SERVER, "Dungeon experience multiplier")

MACRO_CONFIG_INT(SvStrongGold, sv_strong_gold, 2, 0, 5, CFGFLAG_SERVER, "Power mobs / strong gold")
MACRO_CONFIG_INT(SvStrongAntiPVP, sv_strong_anti_pvp, 50, 0, 10000, CFGFLAG_SERVER, "Anti-PVP Difference classing leveling for denied.")

// world time
MACRO_CONFIG_INT(SvTimeWaitingsDungeon, sv_waiting_dungeon_time, 180, 0, 1020, CFGFLAG_SERVER, "Watining time dungeon on secound")
MACRO_CONFIG_INT(SvInfoChatMessageTime, sv_chat_message_time, 600, 0, 14400, CFGFLAG_SERVER, "Time chat message time in seconds")
MACRO_CONFIG_INT(SvInfoChatTopMessageTime, sv_chat_message_time, 900, 0, 14400, CFGFLAG_SERVER, "Time chat toplist message time in seconds")

// house
MACRO_CONFIG_INT(SvLimitDecoration, sv_limit_decorations, 10, 5, 20, CFGFLAG_SERVER, "Limit objects for decoration")

// discord
MACRO_CONFIG_INT(SvCreateDiscordBot, sv_discord_bot, 1, 0, 1, CFGFLAG_SERVER, "Create discord bot")
MACRO_CONFIG_STR(SvDiscordToken, sv_discord_token, 256, "", CFGFLAG_SERVER, "Discord Token")
MACRO_CONFIG_STR(SvDiscordApplicationID, sv_discord_app_id, 256, "", CFGFLAG_SERVER, "Discord application id")
MACRO_CONFIG_STR(SvDiscordInviteLink, sv_discord_invite_link, 32, "nope", CFGFLAG_SERVER, "Link to server invitation")
MACRO_CONFIG_STR(SvDiscordGenerateURL, sv_discord_generateurl, 128, "nope", CFGFLAG_SERVER, "Path folder generate image. Example 'submodules/generator'.")
MACRO_CONFIG_STR(SvSiteUrl, sv_site_url, 128, "nope", CFGFLAG_SERVER, "Url site. Example 'https://mrpg.teeworlds.dev'")

// discord roles
MACRO_CONFIG_STR(SvDiscordWelcomeRole, sv_discord_welcome_role, 128, "", CFGFLAG_SERVER, "Discord role that gives when member joins.")

// discord channels
MACRO_CONFIG_STR(SvDiscordWelcomeChannel, sv_discord_welcome_channel, 128, "", CFGFLAG_SERVER, "Discord welcome channel")
MACRO_CONFIG_STR(SvDiscordServerChatChannel, sv_discord_server_chat_channel, 128, "", CFGFLAG_SERVER, "Discord server chat channel")
MACRO_CONFIG_STR(SvDiscordSuggestionChannel, sv_discord_suggestion_channel, 128, "", CFGFLAG_SERVER, "Discord suggestions channel")
MACRO_CONFIG_STR(SvDiscordAdminChannel, sv_discord_admin_channel, 128, "", CFGFLAG_SERVER, "Discord admin channel")

// mysql
MACRO_CONFIG_STR(SvMySqlHost, sv_sql_host, 32, "localhost", CFGFLAG_SERVER, "MySQL Host")
MACRO_CONFIG_STR(SvMySqlDatabase, sv_sql_database, 32, "database", CFGFLAG_SERVER, "MySQL Database")
MACRO_CONFIG_STR(SvMySqlLogin, sv_sql_login, 32, "root", CFGFLAG_SERVER, "MySQL Login")
MACRO_CONFIG_STR(SvMySqlPassword, sv_sql_password, 32, "", CFGFLAG_SERVER, "MySQL Password")
MACRO_CONFIG_INT(SvMySqlPort, sv_sql_port, 3306, 0, 65000, CFGFLAG_SERVER, "MySQL Port")
MACRO_CONFIG_INT(SvMySqlPoolSize, sv_sql_pool_size, 3, 1, 12, CFGFLAG_SERVER, "MySQL Pool size");

MACRO_CONFIG_INT(SvLoltextHspace, sv_loltext_hspace, 7, 7, 25, CFGFLAG_SERVER, "horizontal offset between loltext 'pixels'")
MACRO_CONFIG_INT(SvLoltextVspace, sv_loltext_vspace, 7, 7, 25, CFGFLAG_SERVER, "vertical offset between loltext 'pixels'")

// ui
MACRO_CONFIG_INT(ClNotifyWindow, cl_notify_window, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow client to notify you on chat highlights")

// performance
MACRO_CONFIG_INT(ClInactiveRendering, cl_inactive_rendering, 1, 0, 2, CFGFLAG_CLIENT, "0 = Always render, 1 = Stop rendering when minimized, 2 = Stop rendering when window is inactive")

MACRO_CONFIG_INT(SvMapUpdateRate, sv_mapupdaterate, 5, 1, 100, CFGFLAG_SERVER, "64 player id <-> vanilla id players map update rate")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS - 1, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
#endif
