/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

// This file can be included several times.

// TODO: remove this
#include "././game/variables.h"

MACRO_CONFIG_STR(PlayerName, player_name, 16, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Name of the player")
MACRO_CONFIG_STR(PlayerClan, player_clan, 12, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Clan of the player")
MACRO_CONFIG_INT(PlayerCountry, player_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Country of the player")

MACRO_CONFIG_STR(Password, password, 256, "", CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Password to the server")
MACRO_CONFIG_INT(Events, events, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Enable triggering of events, (eye emotes on some holidays in server, christmas skins in client).")
MACRO_CONFIG_STR(SteamName, steam_name, 16, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Last seen name of the Steam profile")

MACRO_CONFIG_STR(Logfile, logfile, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Filename to log all output to")
MACRO_CONFIG_INT(Logappend, logappend, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Append to logfile instead of overwriting it every time")
MACRO_CONFIG_INT(Loglevel, loglevel, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the logfile (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(StdoutOutputLevel, stdout_output_level, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the system console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(ConsoleOutputLevel, console_output_level, 0, -3, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Adjusts the amount of information in the local/remote console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")
MACRO_CONFIG_INT(ConsoleEnableColors, console_enable_colors, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Enable colors in console output")

MACRO_CONFIG_INT(ClSaveSettings, cl_save_settings, 1, 0, 1, CFGFLAG_CLIENT, "Write the settings file on exit")
MACRO_CONFIG_INT(ClRefreshRate, cl_refresh_rate, 0, 0, 10000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Refresh rate for updating the game (in Hz)")
MACRO_CONFIG_INT(ClRefreshRateInactive, cl_refresh_rate_inactive, 120, 0, 10000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Refresh rate for updating the game when the window is inactive (in Hz)")
MACRO_CONFIG_INT(ClEditor, cl_editor, 0, 0, 1, CFGFLAG_CLIENT, "Open the map editor")
MACRO_CONFIG_INT(ClEditorDilate, cl_editor_dilate, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically dilates embedded images")
MACRO_CONFIG_STR(ClSkinFilterString, cl_skin_filter_string, 25, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Skin filtering string")

MACRO_CONFIG_INT(ClAutoDemoRecord, cl_auto_demo_record, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically record demos")
MACRO_CONFIG_INT(ClAutoDemoOnConnect, cl_auto_demo_on_connect, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Only start a new demo when connect while automatically record demos")
MACRO_CONFIG_INT(ClAutoDemoMax, cl_auto_demo_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically recorded demos (0 = no limit)")
MACRO_CONFIG_INT(ClAutoScreenshot, cl_auto_screenshot, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically take game over screenshot")
MACRO_CONFIG_INT(ClAutoScreenshotMax, cl_auto_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically created screenshots (0 = no limit)")
MACRO_CONFIG_INT(ClAutoCSV, cl_auto_csv, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically create game over csv")
MACRO_CONFIG_INT(ClAutoCSVMax, cl_auto_csv_max, 10, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Maximum number of automatically created csvs (0 = no limit)")
MACRO_CONFIG_INT(ClShowBroadcasts, cl_show_broadcasts, 1, 0, 1, CFGFLAG_CLIENT, "Show broadcasts ingame")
MACRO_CONFIG_INT(ClPrintBroadcasts, cl_print_broadcasts, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Print broadcasts to console")
MACRO_CONFIG_INT(ClPrintMotd, cl_print_motd, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Print motd to console")
MACRO_CONFIG_INT(ClFriendsIgnoreClan, cl_friends_ignore_clan, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ignore clan tag when searching for friends")

MACRO_CONFIG_STR(ClAssetsEntities, cl_assets_entities, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset/assets for entities")
MACRO_CONFIG_STR(ClAssetGame, cl_asset_game, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for game")
MACRO_CONFIG_STR(ClAssetEmoticons, cl_asset_emoticons, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for emoticons")
MACRO_CONFIG_STR(ClAssetParticles, cl_asset_particles, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for particles")
MACRO_CONFIG_STR(ClAssetHud, cl_asset_hud, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for HUD")
MACRO_CONFIG_STR(ClAssetExtras, cl_asset_extras, 50, "default", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The asset for the game graphics that do not come from Teeworlds")

MACRO_CONFIG_STR(BrFilterString, br_filter_string, 128, "Novice", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Server browser filtering string")
MACRO_CONFIG_STR(BrExcludeString, br_exclude_string, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Server browser exclusion string")
MACRO_CONFIG_INT(BrFilterFull, br_filter_full, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out full server in browser")
MACRO_CONFIG_INT(BrFilterEmpty, br_filter_empty, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out empty server in browser")
MACRO_CONFIG_INT(BrFilterSpectators, br_filter_spectators, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out spectators from player numbers")
MACRO_CONFIG_INT(BrFilterFriends, br_filter_friends, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out servers with no friends")
MACRO_CONFIG_INT(BrFilterCountry, br_filter_country, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out servers with non-matching player country")
MACRO_CONFIG_INT(BrFilterCountryIndex, br_filter_country_index, -1, -1, 999, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Player country to filter by in the server browser")
MACRO_CONFIG_INT(BrFilterPw, br_filter_pw, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out password protected servers in browser")
MACRO_CONFIG_STR(BrFilterGametype, br_filter_gametype, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Game types to filter")
MACRO_CONFIG_INT(BrFilterGametypeStrict, br_filter_gametype_strict, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Strict gametype filter")
MACRO_CONFIG_INT(BrFilterConnectingPlayers, br_filter_connecting_players, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter connecting players")
MACRO_CONFIG_STR(BrFilterServerAddress, br_filter_serveraddress, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Server address to filter")
MACRO_CONFIG_INT(BrFilterUnfinishedMap, br_filter_unfinished_map, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show only servers with unfinished maps")

MACRO_CONFIG_STR(BrFilterExcludeCommunities, br_filter_exclude_communities, 512, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out servers by community")
MACRO_CONFIG_STR(BrFilterExcludeCountries, br_filter_exclude_countries, 512, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out communities' servers by country")
MACRO_CONFIG_STR(BrFilterExcludeTypes, br_filter_exclude_types, 512, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Filter out communities' servers by gametype")
MACRO_CONFIG_INT(BrIndicateFinished, br_indicate_finished, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show whether you have finished a DDNet map (transmits your player name to info.ddnet.org/info)")
MACRO_CONFIG_STR(BrLocation, br_location, 16, "auto", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Override location for ping estimation, available: auto, af, as, as:cn, eu, na, oc, sa (Automatic, Africa, Asia, China, Europe, North America, Oceania/Australia, South America")
MACRO_CONFIG_STR(BrCachedBestServerinfoUrl, br_cached_best_serverinfo_url, 256, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Do not set this variable, instead create a ddnet-serverlist-urls.cfg next to settings_ddnet.cfg to specify all possible serverlist URLs")

MACRO_CONFIG_INT(BrSort, br_sort, 4, 0, 256, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting column in server browser")
MACRO_CONFIG_INT(BrSortOrder, br_sort_order, 2, 0, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting order in server browser")
MACRO_CONFIG_INT(BrMaxRequests, br_max_requests, 100, 0, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Number of concurrent requests to use when refreshing server browser")

MACRO_CONFIG_INT(BrDemoSort, br_demo_sort, 0, 0, 2, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting column in demo browser")
MACRO_CONFIG_INT(BrDemoSortOrder, br_demo_sort_order, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sorting order in demo browser")
MACRO_CONFIG_INT(BrDemoFetchInfo, br_demo_fetch_info, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to auto fetch demo infos on refresh")

MACRO_CONFIG_INT(SndBufferSize, snd_buffer_size, 512, 128, 32768, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound buffer size (may cause delay if large)")
MACRO_CONFIG_INT(SndRate, snd_rate, 48000, 5512, 384000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound mixing rate")
MACRO_CONFIG_INT(SndEnable, snd_enable, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound enable")
MACRO_CONFIG_INT(SndMusic, snd_enable_music, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Play background music")
MACRO_CONFIG_INT(SndVolume, snd_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Sound volume")
MACRO_CONFIG_INT(SndChatSoundVolume, snd_chat_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Chat sound volume")
MACRO_CONFIG_INT(SndGameSoundVolume, snd_game_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Game sound volume")
MACRO_CONFIG_INT(SndMapSoundVolume, snd_ambient_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Map Sound sound volume")
MACRO_CONFIG_INT(SndBackgroundMusicVolume, snd_background_music_volume, 30, 0, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Background music sound volume")

MACRO_CONFIG_INT(SndNonactiveMute, snd_nonactive_mute, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mute sounds when window is not active")
MACRO_CONFIG_INT(SndGame, snd_game, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable game sounds")
MACRO_CONFIG_INT(SndGun, snd_gun, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable gun sound")
MACRO_CONFIG_INT(SndLongPain, snd_long_pain, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable long pain sound (used when shooting in freeze)")
MACRO_CONFIG_INT(SndChat, snd_chat, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable regular chat sound")
MACRO_CONFIG_INT(SndTeamChat, snd_team_chat, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable team chat sound")
MACRO_CONFIG_INT(SndServerMessage, snd_servermessage, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable server message sound")
MACRO_CONFIG_INT(SndHighlight, snd_highlight, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable highlighted chat sound")

MACRO_CONFIG_INT(GfxScreen, gfx_screen, 0, 0, 15, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen index")
MACRO_CONFIG_INT(GfxScreenWidth, gfx_screen_width, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen resolution width")
MACRO_CONFIG_INT(GfxScreenHeight, gfx_screen_height, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen resolution height")
MACRO_CONFIG_INT(GfxScreenRefreshRate, gfx_screen_refresh_rate, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen refresh rate")

MACRO_CONFIG_INT(GfxDesktopWidth, gfx_desktop_width, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Desktop resolution width for detecting display changes (not recommended to change manually)")
MACRO_CONFIG_INT(GfxDesktopHeight, gfx_desktop_height, 0, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Desktop resolution height for detecting display changes (not recommended to change manually)")
#if !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_INT(GfxBorderless, gfx_borderless, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Borderless window (not to be used with fullscreen)")
#if !defined(CONF_WEBASM)
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 1, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Set fullscreen mode: 0=no fullscreen, 1=pure fullscreen, 2=desktop fullscreen, 3=windowed fullscreen")
#else
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 0, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Set fullscreen mode: 0=no fullscreen, 1=pure fullscreen, 2=desktop fullscreen, 3=windowed fullscreen")
#endif
#else
MACRO_CONFIG_INT(GfxBorderless, gfx_borderless, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Borderless window (not to be used with fullscreen)")
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 0, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Set fullscreen mode: 0=no fullscreen, 1=pure fullscreen, 2=desktop fullscreen, 3=windowed fullscreen")
#endif
MACRO_CONFIG_INT(GfxHighdpi, gfx_highdpi, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable high-dpi")
MACRO_CONFIG_INT(GfxColorDepth, gfx_color_depth, 24, 16, 24, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Colors bits for framebuffer (fullscreen only)")
MACRO_CONFIG_INT(GfxVsync, gfx_vsync, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Vertical sync (may cause delay)")
MACRO_CONFIG_INT(GfxDisplayAllVideoModes, gfx_display_all_video_modes, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show all video modes")
MACRO_CONFIG_INT(GfxHighDetail, gfx_high_detail, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "High detail")
MACRO_CONFIG_INT(GfxFsaaSamples, gfx_fsaa_samples, 0, 0, 64, CFGFLAG_SAVE | CFGFLAG_CLIENT, "FSAA samples (may cause delay)")
MACRO_CONFIG_INT(GfxRefreshRate, gfx_refresh_rate, 0, 0, 10000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Screen refresh rate")
MACRO_CONFIG_INT(GfxBackgroundRender, gfx_backgroundrender, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Render graphics when window is in background")
MACRO_CONFIG_INT(GfxTextOverlay, gfx_text_overlay, 10, 1, 100, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Stop rendering textoverlay in editor or with entities: high value = less details = more speed")
MACRO_CONFIG_INT(GfxAsyncRenderOld, gfx_asyncrender_old, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "During an update cycle, skip the render cycle, if the render cycle would need to wait for the previous render cycle to finish")
MACRO_CONFIG_INT(GfxQuadAsTriangle, gfx_quad_as_triangle, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Render quads as triangles (fixes quad coloring on some GPUs)")

MACRO_CONFIG_INT(InpMousesens, inp_mousesens, 200, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mouse sensitivity")
MACRO_CONFIG_INT(InpTranslatedKeys, inp_translated_keys, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Translate keys before interpreting them, respects keyboard layouts")
MACRO_CONFIG_INT(InpIgnoredModifiers, inp_ignored_modifiers, 0, 0, 65536, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Ignored keyboard modifier mask")
#if defined(CONF_FAMILY_WINDOWS)
MACRO_CONFIG_INT(InpImeNativeUi, inp_ime_native_ui, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Use native UI for IME (may cause IME to not work in fullscreen mode) (changing requires restart)")
#endif

MACRO_CONFIG_INT(InpControllerEnable, inp_controller_enable, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable controller")
MACRO_CONFIG_STR(InpControllerGUID, inp_controller_guid, 34, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller GUID which uniquely identifies the active controller")
MACRO_CONFIG_INT(InpControllerAbsolute, inp_controller_absolute, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable absolute controller aiming ingame")
MACRO_CONFIG_INT(InpControllerSens, inp_controller_sens, 100, 1, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Ingame controller sensitivity")
MACRO_CONFIG_INT(InpControllerX, inp_controller_x, 0, 0, 12, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller axis that controls X axis of cursor")
MACRO_CONFIG_INT(InpControllerY, inp_controller_y, 1, 0, 12, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller axis that controls Y axis of cursor")
MACRO_CONFIG_INT(InpControllerTolerance, inp_controller_tolerance, 5, 0, 50, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Controller axis tolerance to account for jitter")

MACRO_CONFIG_INT(ClPort, cl_port, 0, 0, 65535, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Port to use for client connections to server (0 to choose a random port, 1024 or higher to set a manual port, requires a restart)")
MACRO_CONFIG_INT(ClDummyPort, cl_dummy_port, 0, 0, 65535, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Port to use for dummy connections to server (0 to choose a random port, 1024 or higher to set a manual port, requires a restart)")
MACRO_CONFIG_INT(ClContactPort, cl_contact_port, 0, 0, 65535, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Port to use for serverinfo connections to server (0 to choose a random port, 1024 or higher to set a manual port, requires a restart)")

MACRO_CONFIG_STR(SvName, sv_name, 128, "unnamed server", CFGFLAG_SERVER, "Server name")
MACRO_CONFIG_STR(Bindaddr, bindaddr, 128, "", CFGFLAG_CLIENT | CFGFLAG_SERVER | CFGFLAG_MASTER, "Address to bind the client/server to")
MACRO_CONFIG_INT(SvIpv4Only, sv_ipv4only, 0, 0, 1, CFGFLAG_SERVER, "Whether to bind only to ipv4, otherwise bind to all available interfaces")
MACRO_CONFIG_INT(SvPort, sv_port, 0, 0, 0, CFGFLAG_SERVER, "Port to use for the server (Only ports 8303-8310 work in LAN server browser, 0 to automatically find a free port in 8303-8310)")
MACRO_CONFIG_STR(SvGamemodeName, sv_gamemode_name, 16, "MRPG", CFGFLAG_SERVER, "Gamemode name what used MRPG core.")
MACRO_CONFIG_STR(SvMap, sv_map, 128, "Multiworlds", CFGFLAG_SERVER, "Map name to use on the server")
MACRO_CONFIG_INT(SvMaxClientsPerIP, sv_max_clients_per_ip, 4, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients with the same IP that can connect to the server")
MACRO_CONFIG_INT(SvHighBandwidth, sv_high_bandwidth, 0, 0, 1, CFGFLAG_SERVER, "Use high bandwidth mode. Doubles the bandwidth required for the server. LAN use only")
MACRO_CONFIG_STR(SvRegister, sv_register, 16, "1", CFGFLAG_SERVER, "Register server with master server for public listing, can also accept a comma-separated list of protocols to register on, like 'ipv4,ipv6'")
MACRO_CONFIG_STR(SvRegisterExtra, sv_register_extra, 256, "", CFGFLAG_SERVER, "Extra headers to send to the register endpoint, comma separated 'Header: Value' pairs")
MACRO_CONFIG_STR(SvRegisterUrl, sv_register_url, 128, "https://master1.ddnet.org/ddnet/15/register", CFGFLAG_SERVER, "Masterserver URL to register to")
MACRO_CONFIG_STR(SvRconPassword, sv_rcon_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password (full access)")
MACRO_CONFIG_STR(SvRconModPassword, sv_rcon_mod_password, 128, "", CFGFLAG_SERVER | CFGFLAG_NONTEEHISTORIC, "Remote console password for moderators (limited access)")
MACRO_CONFIG_INT(SvRconMaxTries, sv_rcon_max_tries, 30, 0, 100, CFGFLAG_SERVER, "Maximum number of tries for remote console authentication")
MACRO_CONFIG_INT(SvRconBantime, sv_rcon_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time a client gets banned if remote console authentication fails. 0 makes it just use kick")
MACRO_CONFIG_INT(SvVanillaAntiSpoof, sv_vanilla_antispoof, 1, 0, 1, CFGFLAG_SERVER, "Enable vanilla Antispoof")
MACRO_CONFIG_INT(SvServerInfoPerSecond, sv_server_info_per_second, 50, 0, 10000, CFGFLAG_SERVER, "Maximum number of complete server info responses that are sent out per second (0 for no limit)")
MACRO_CONFIG_INT(SvVanConnPerSecond, sv_van_conn_per_second, 10, 0, 10000, CFGFLAG_SERVER, "Antispoof specific ratelimit (0 for no limit)")

MACRO_CONFIG_INT(SvHardresetAfterDays, sv_hard_reset_after_days, 7, 1, 14, CFGFLAG_SAVE | CFGFLAG_SERVER, "Reset the server when it has been idle for a specified number of days without players")
MACRO_CONFIG_INT(SvMaxAfkTime, sv_max_afk_time, 600, 0, 144400, CFGFLAG_SAVE | CFGFLAG_SERVER, "Afk timer")
MACRO_CONFIG_INT(SvMapDownloadSpeed, sv_map_download_speed, 8, 1, 16, CFGFLAG_SAVE | CFGFLAG_SERVER, "Number of map data packages a client gets on each request")
MACRO_CONFIG_INT(SvShowWorldWhenConnect, sv_show_world_when_connect, 0, 0, ENGINE_MAX_WORLDS, CFGFLAG_SERVER, "Show default world when player connect")

MACRO_CONFIG_STR(EcBindaddr, ec_bindaddr, 128, "localhost", CFGFLAG_ECON, "Address to bind the external console to. Anything but 'localhost' is dangerous")
MACRO_CONFIG_INT(EcPort, ec_port, 0, 0, 0, CFGFLAG_ECON, "Port to use for the external console")
MACRO_CONFIG_STR(EcPassword, ec_password, 128, "", CFGFLAG_ECON, "External console password")
MACRO_CONFIG_INT(EcBantime, ec_bantime, 0, 0, 1440, CFGFLAG_ECON, "The time a client gets banned if econ authentication fails. 0 just closes the connection")
MACRO_CONFIG_INT(EcAuthTimeout, ec_auth_timeout, 30, 1, 120, CFGFLAG_ECON, "Time in seconds before the the econ authentication times out")
MACRO_CONFIG_INT(EcOutputLevel, ec_output_level, 0, -3, 2, CFGFLAG_ECON, "Adjusts the amount of information in the external console (-3 = none, -2 = error only, -1 = warn, 0 = info, 1 = debug, 2 = trace)")

MACRO_CONFIG_INT(Debug, debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug mode")
MACRO_CONFIG_INT(DbgCurl, dbg_curl, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Debug curl")
MACRO_CONFIG_INT(DbgGraphs, dbg_graphs, 0, 0, 1, CFGFLAG_CLIENT, "Performance graphs")
MACRO_CONFIG_INT(DbgGfx, dbg_gfx, 0, 0, 4, CFGFLAG_CLIENT, "Show graphic library warnings and errors, if the GPU supports it (0: none, 1: minimal, 2: affects performance, 3: verbose, 4: all)")
#ifdef CONF_DEBUG
MACRO_CONFIG_INT(DbgStress, dbg_stress, 0, 0, 1, CFGFLAG_CLIENT, "Stress systems (Debug build only)")
MACRO_CONFIG_STR(DbgStressServer, dbg_stress_server, 32, "localhost", CFGFLAG_CLIENT, "Server to stress (Debug build only)")
#endif

MACRO_CONFIG_INT(HttpAllowInsecure, http_allow_insecure, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SERVER, "Allow insecure HTTP protocol in addition to the secure HTTPS one. Mostly useful for testing.")

// DDRace
MACRO_CONFIG_INT(SvTestingCommands, sv_test_cmds, 0, 0, 1, CFGFLAG_SERVER, "Turns testing commands aka cheats on/off (setting only works in initial config)")
MACRO_CONFIG_INT(ClDDRaceBindsSet, cl_race_binds_set, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What level the DDRace binds are set to (this is automated, you don't need to use this)")
MACRO_CONFIG_INT(SvMapWindow, sv_map_window, 15, 0, 100, CFGFLAG_SERVER, "Map downloading send-ahead window")
MACRO_CONFIG_INT(SvFastDownload, sv_fast_download, 1, 0, 1, CFGFLAG_SERVER, "Enables fast download of maps")

#if defined(CONF_UPNP)
MACRO_CONFIG_INT(SvUseUPnP, sv_use_upnp, 0, 0, 1, CFGFLAG_SERVER, "Enables UPnP support. (Requires -DCONF_UPNP=ON when compiling)")
#endif

MACRO_CONFIG_INT(ClReconnectTimeout, cl_reconnect_timeout, 120, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many seconds to wait before reconnecting (after timeout, 0 for off)")
MACRO_CONFIG_INT(ClReconnectFull, cl_reconnect_full, 5, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many seconds to wait before reconnecting (when server is full, 0 for off)")

MACRO_CONFIG_COL(ClMessageSystemColor, cl_message_system_color, 2817983, CFGFLAG_CLIENT | CFGFLAG_SAVE, "System message color")
MACRO_CONFIG_COL(ClMessageClientColor, cl_message_client_color, 9633471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color")
MACRO_CONFIG_COL(ClMessageHighlightColor, cl_message_highlight_color, 65471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Highlighted message color")
MACRO_CONFIG_COL(ClMessageTeamColor, cl_message_team_color, 5636050, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Team message color")
MACRO_CONFIG_COL(ClMessageColor, cl_message_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message color")
MACRO_CONFIG_COL(ClLaserRifleInnerColor, cl_laser_rifle_inner_color, 11206591, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for Rifle")
MACRO_CONFIG_COL(ClLaserRifleOutlineColor, cl_laser_rifle_outline_color, 11176233, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for Rifle")
MACRO_CONFIG_COL(ClLaserShotgunInnerColor, cl_laser_sg_inner_color, 1467241, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for Shotgun")
MACRO_CONFIG_COL(ClLaserShotgunOutlineColor, cl_laser_sg_outline_color, 1866773, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for Shotgun")
MACRO_CONFIG_COL(ClLaserDoorInnerColor, cl_laser_door_inner_color, 7701379, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for doors")
MACRO_CONFIG_COL(ClLaserDoorOutlineColor, cl_laser_door_outline_color, 7667473, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for doors")
MACRO_CONFIG_COL(ClLaserFreezeInnerColor, cl_laser_freeze_inner_color, 12001153, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser inner color for freezes")
MACRO_CONFIG_COL(ClLaserFreezeOutlineColor, cl_laser_freeze_outline_color, 11613223, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Laser outline color for freezes")
MACRO_CONFIG_COL(ClKillMessageNormalColor, cl_kill_message_normal_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Kill message normal color")
MACRO_CONFIG_COL(ClKillMessageHighlightColor, cl_kill_message_highlight_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Kill message highlight color")

MACRO_CONFIG_INT(ClMessageFriend, cl_message_friend, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable coloring and the heart for friends")
MACRO_CONFIG_COL(ClMessageFriendColor, cl_message_friend_color, 65425, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Friend message color")

MACRO_CONFIG_INT(ConnTimeout, conn_timeout, 100, 5, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_SERVER, "Network timeout")
MACRO_CONFIG_INT(ConnTimeoutProtection, conn_timeout_protection, 1000, 5, 10000, CFGFLAG_SERVER, "Network timeout protection")
MACRO_CONFIG_INT(ClShowIDs, cl_show_ids, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to show client ids in scoreboard")
MACRO_CONFIG_INT(ClScoreboardOnDeath, cl_scoreboard_on_death, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to show scoreboard after death or not")
MACRO_CONFIG_INT(ClAutoRaceRecord, cl_auto_race_record, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save the best demo of each race")
MACRO_CONFIG_INT(ClReplays, cl_replays, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable/disable replays")
MACRO_CONFIG_INT(ClReplayLength, cl_replay_length, 30, 10, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set the default length of the replays")
MACRO_CONFIG_INT(ClRaceRecordServerControl, cl_race_record_server_control, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let the server start the race recorder")
MACRO_CONFIG_INT(ClDemoName, cl_demo_name, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save the player name within the demo")
MACRO_CONFIG_INT(ClDemoAssumeRace, cl_demo_assume_race, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Assume that demos are race demos")
MACRO_CONFIG_INT(ClRaceGhost, cl_race_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable ghost")
MACRO_CONFIG_INT(ClRaceGhostServerControl, cl_race_ghost_server_control, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let the server start the ghost")
MACRO_CONFIG_INT(ClRaceShowGhost, cl_race_show_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghost")
MACRO_CONFIG_INT(ClRaceSaveGhost, cl_race_save_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save ghost")
MACRO_CONFIG_INT(ClRaceGhostStrictMap, cl_race_ghost_strict_map, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Match ghosts by map version instead of only map name")
MACRO_CONFIG_INT(ClRaceGhostSaveBest, cl_race_ghost_save_best, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save only ghosts that are better than the previous record.")
MACRO_CONFIG_INT(ClRaceGhostAlpha, cl_race_ghost_alpha, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Visbility of ghosts (alpha value, 0 invisible, 100 fully visible)")

MACRO_CONFIG_INT(ClDDRaceScoreBoard, cl_ddrace_scoreboard, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable DDRace Scoreboard")
MACRO_CONFIG_INT(SvResetPickups, sv_reset_pickups, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether the weapons are reset on passing the start tile or not")
MACRO_CONFIG_INT(ClShowOthers, cl_show_others, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players in other teams (2 to show own team only)")
MACRO_CONFIG_INT(ClShowOthersAlpha, cl_show_others_alpha, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players in other teams (alpha value, 0 invisible, 100 fully visible)")
MACRO_CONFIG_INT(ClOverlayEntities, cl_overlay_entities, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Overlay game tiles with a percentage of opacity")
MACRO_CONFIG_INT(ClShowQuads, cl_showquads, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show quads (only interesting for mappers, or if your system has extremely bad performance)")
MACRO_CONFIG_COL(ClBackgroundColor, cl_background_color, 128, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background color") // 0 0 128
MACRO_CONFIG_COL(ClBackgroundEntitiesColor, cl_background_entities_color, 128, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background (entities) color") // 0 0 128
MACRO_CONFIG_STR(ClBackgroundEntities, cl_background_entities, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Background (entities)")
MACRO_CONFIG_STR(ClRunOnJoin, cl_run_on_join, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Command to run when joining a server")

// menu background map
MACRO_CONFIG_STR(SvResetFile, sv_reset_file, 128, "reset.cfg", CFGFLAG_SERVER, "File to execute on map change or reload to set the default server settings")
MACRO_CONFIG_STR(SvInputFifo, sv_input_fifo, 128, "", CFGFLAG_SERVER, "Fifo file (non-Windows) or Named Pipe (Windows) to use as input for server console")
MACRO_CONFIG_STR(SvDefaultLanguage, sv_default_language, 64, "en", CFGFLAG_SERVER, "Default language")

// these might need some fine tuning
MACRO_CONFIG_STR(SvBannedVersions, sv_banned_versions, 128, "", CFGFLAG_SERVER, "Comma separated list of banned clients to be kicked on join")

// netlimit
MACRO_CONFIG_INT(SvNetlimit, sv_netlimit, 0, 0, 10000, CFGFLAG_SERVER, "Netlimit: Maximum amount of traffic a client is allowed to use (in kb/s)")
MACRO_CONFIG_INT(SvNetlimitAlpha, sv_netlimit_alpha, 50, 1, 100, CFGFLAG_SERVER, "Netlimit: Alpha of Exponention moving average")

MACRO_CONFIG_INT(SvConnlimit, sv_connlimit, 5, 0, 100, CFGFLAG_SERVER, "Connlimit: Number of connections an IP is allowed to do in a timespan")
MACRO_CONFIG_INT(SvConnlimitTime, sv_connlimit_time, 20, 0, 1000, CFGFLAG_SERVER, "Connlimit: Time in which IP's connections are counted")

#if defined(CONF_FAMILY_UNIX)
MACRO_CONFIG_STR(SvConnLoggingServer, sv_conn_logging_server, 128, "", CFGFLAG_SERVER, "Unix socket server for IP address logging (Unix only)")
#endif

MACRO_CONFIG_INT(ClUnpredictedShadow, cl_unpredicted_shadow, 0, -1, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show unpredicted shadow tee (0 = off, 1 = on, -1 = don't even show in debug mode)")
MACRO_CONFIG_INT(ClPredictFreeze, cl_predict_freeze, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict freeze tiles (0 = off, 1 = on, 2 = partial (allow a small amount of movement in freeze)")
MACRO_CONFIG_INT(ClShowNinja, cl_show_ninja, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ninja skin")
MACRO_CONFIG_INT(ClShowHookCollOther, cl_show_hook_coll_other, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show other players' hook collision line (2 to always show)")
MACRO_CONFIG_INT(ClShowHookCollOwn, cl_show_hook_coll_own, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show own players' hook collision line (2 to always show)")
MACRO_CONFIG_INT(ClHookCollSize, cl_hook_coll_size, 0, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of hook collision line")
MACRO_CONFIG_INT(ClHookCollAlpha, cl_hook_coll_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of hook collision line (0 invisible, 100 fully visible)")

MACRO_CONFIG_COL(ClHookCollColorNoColl, cl_hook_coll_color_no_coll, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies the color of a hookline that hits nothing.")
MACRO_CONFIG_COL(ClHookCollColorHookableColl, cl_hook_coll_color_hookable_coll, 6401973, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies the color of a hookline that hits hookable tiles.")
MACRO_CONFIG_COL(ClHookCollColorTeeColl, cl_hook_coll_color_tee_coll, 2817919, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Specifies the color of a hookline that hits tees.")

MACRO_CONFIG_INT(ClChatTeamColors, cl_chat_teamcolors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show names in chat in team colors")
MACRO_CONFIG_INT(ClChatReset, cl_chat_reset, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset chat when pressing escape")
MACRO_CONFIG_INT(ClChatOld, cl_chat_old, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Old chat style: No tee, no background")

MACRO_CONFIG_INT(ClShowDirection, cl_show_direction, 1, 0, 3, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show key presses (1 = other players', 2 = also your own, 3 = only your own")
MACRO_CONFIG_INT(ClOldGunPosition, cl_old_gun_position, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Tees hold gun a bit higher like in TW 0.6.1 and older")
MACRO_CONFIG_INT(ClConfirmDisconnectTime, cl_confirm_disconnect_time, 20, -1, 1440, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Confirmation popup before disconnecting after game time (in minutes, -1 to turn off, 0 to always turn on)")
MACRO_CONFIG_INT(ClConfirmQuitTime, cl_confirm_quit_time, 20, -1, 1440, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Confirmation popup before quitting after game time (in minutes, -1 to turn off, 0 to always turn on)")
MACRO_CONFIG_STR(ClTimeoutCode, cl_timeout_code, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Timeout code to use")
MACRO_CONFIG_STR(ClDummyTimeoutCode, cl_dummy_timeout_code, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Dummy Timeout code to use")
MACRO_CONFIG_STR(ClTimeoutSeed, cl_timeout_seed, 64, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Timeout seed")
MACRO_CONFIG_STR(ClInputFifo, cl_input_fifo, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "Fifo file (non-Windows) or Named Pipe (Windows) to use as input for client console")
MACRO_CONFIG_INT(ClConfigVersion, cl_config_version, 0, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The config version. Helps newer clients fix bugs with older configs.")

// demo editor
MACRO_CONFIG_INT(ClDemoSliceBegin, cl_demo_slice_begin, -1, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Begin marker for demo slice")
MACRO_CONFIG_INT(ClDemoSliceEnd, cl_demo_slice_end, -1, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "End marker for demo slice")
MACRO_CONFIG_INT(ClDemoShowSpeed, cl_demo_show_speed, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show speed meter on change")
MACRO_CONFIG_INT(ClDemoShowPause, cl_demo_show_pause, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Show pause/play indicator on change")
MACRO_CONFIG_INT(ClDemoKeyboardShortcuts, cl_demo_keyboard_shortcuts, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable keyboard shortcuts in demo player")

// graphic library
#if !defined(CONF_ARCH_IA32) && !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_INT(GfxGLMajor, gfx_gl_major, 1, 1, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library major version")
#elif !defined(CONF_ARCH_IA32)
MACRO_CONFIG_INT(GfxGLMajor, gfx_gl_major, 3, 1, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library major version")
#else
MACRO_CONFIG_INT(GfxGLMajor, gfx_gl_major, 1, 1, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library major version")
#endif
#if !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_INT(GfxGLMinor, gfx_gl_minor, 1, 0, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library minor version")
#else
MACRO_CONFIG_INT(GfxGLMinor, gfx_gl_minor, 3, 0, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library minor version")
#endif
MACRO_CONFIG_INT(GfxGLPatch, gfx_gl_patch, 0, 0, 10, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Graphic library patch version")

// float multiplied with 1000
MACRO_CONFIG_INT(GfxGLTextureLODBIAS, gfx_gl_texture_lod_bias, -500, -15000, 15000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "The lod bias for graphic library texture sampling multiplied by 1000")

MACRO_CONFIG_INT(Gfx3DTextureAnalysisRan, gfx_3d_texture_analysis_ran, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Ran an analyzer to check if sampling 3D/2D array textures works correctly")
MACRO_CONFIG_STR(Gfx3DTextureAnalysisRenderer, gfx_3d_texture_analysis_renderer, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The renderer on which the analysis was performed")
MACRO_CONFIG_STR(Gfx3DTextureAnalysisVersion, gfx_3d_texture_analysis_version, 128, "", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The version on which the analysis was performed")

MACRO_CONFIG_STR(GfxGPUName, gfx_gpu_name, 256, "auto", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The GPU's name, which will be selected by the backend. (if supported by the backend)")
#if !defined(CONF_ARCH_IA32) && !defined(CONF_PLATFORM_MACOS)
MACRO_CONFIG_STR(GfxBackend, gfx_backend, 256, "Vulkan", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The backend to use (e.g. OpenGL or Vulkan)")
#else
MACRO_CONFIG_STR(GfxBackend, gfx_backend, 256, "OpenGL", CFGFLAG_SAVE | CFGFLAG_CLIENT, "The backend to use (e.g. OpenGL or Vulkan)")
#endif
MACRO_CONFIG_INT(GfxRenderThreadCount, gfx_render_thread_count, 3, 0, 0, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Number of threads the backend can use for rendering. (note: the value can be ignored by the backend)")

MACRO_CONFIG_INT(GfxDriverIsBlocked, gfx_driver_is_blocked, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "If 1, the current driver is in a blocked error state.")

MACRO_CONFIG_INT(ClVideoRecorderFPS, cl_video_recorder_fps, 60, 1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "At which FPS the videorecorder should record demos.")
