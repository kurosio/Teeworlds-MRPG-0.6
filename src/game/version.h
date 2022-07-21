/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VERSION_H
#define GAME_VERSION_H
#include <generated/nethash.cpp>

#define SETTINGS_FILENAME "settings_mmo"
#define GAME_VERSION "0.6.5"
#define GAME_NETVERSION_HASH_FORCED "802f1be60a05665f"
#define GAME_NETVERSION "0.6 " GAME_NETVERSION_HASH_FORCED
#define CLIENT_VERSION 0x0605
#define PREV_CLIENT_VERSION 0x0704

// ~~ RELEASE GAME SIDE VERSION
#define GAME_RELEASE_VERSION "2.0.9"

// ~~ RELEASE PROTOCOL(CLIENT/SERVER SIDE) VERSION
// in case of a change it will force to update the client when entering the server to the value that is specified here
#define PROTOCOL_VERSION_MMO 1027

#endif
