/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VERSION_H
#define GAME_VERSION_H

#define SETTINGS_FILENAME "settings_mmo"

#define GAME_RELEASE_VERSION "16.4"
#define CLIENT_VERSIONNR 16040

#define GAME_VERSION "0.6.4, " GAME_RELEASE_VERSION
#define GAME_NETVERSION "0.6 626fce9a778df4d4"
extern const char *GIT_SHORTREV_HASH;

// ~~ RELEASE PROTOCOL(CLIENT/SERVER SIDE) VERSION
// in case of a change it will force to update the client when entering the server to the value that is specified here
#define CURRENT_PROTOCOL_VERSION_MRPG 2000

#endif