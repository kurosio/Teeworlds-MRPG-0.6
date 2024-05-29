#ifndef GAME_TEEOTHER_STDAFX_SERVER_H
#define GAME_TEEOTHER_STDAFX_SERVER_H

#include <variant>
#include <teeother/tools/allocator.h>
#include <teeother/tools/string.h>
#include <teeother/tools/hashtable.h>
#include <teeother/tools/binaryheap.h>

#include <teeother/tools/flat_hash_map/bytell_hash_map.h>
#include <teeother/tools/flat_hash_map/flat_hash_map.h>
#include <teeother/tools/flat_hash_map/unordered_map.h>
#include <engine/server/sql_connect_pool.h>
#include <teeother/tools/dbfield.h>
#include <teeother/tools/dbset.h>


#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/message.h>
#include <engine/shared/config.h>
#include <game/gamecore.h>
#include <game/version.h>
#include <game/voting.h>
#include <game/server/core/mmo_context.h>

#endif //GAME_TEEOTHER_STDAFX_SHARED_H