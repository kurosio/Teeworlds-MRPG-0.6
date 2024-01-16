#ifndef SERVER_STDAFX_H
#define SERVER_STDAFX_H

//TODO: add precompiled headers
#include <variant>

// core
#include <engine/server/sql_connect_pool.h>

// custom something that is subject to less changes is introduced
#include <base/system.h>
#include <game/server/core/mmo_context.h>
#include <teeother/components/localization.h>
#include <teeother/flat_hash_map/bytell_hash_map.h>
#include <teeother/flat_hash_map/flat_hash_map.h>
#include <teeother/flat_hash_map/unordered_map.h>

#include <game/version.h>

#endif //SERVER_STDAFX_H