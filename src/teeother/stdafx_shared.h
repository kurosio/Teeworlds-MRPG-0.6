#ifndef SHARED_STDAFX_H
#define SHARED_STDAFX_H

//TODO: add precompiled headers

// core
#include <atomic>
#include <vector>
#include <list>
#include <mutex>
#include <thread>
#include <map>
#include <unordered_map>
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <queue>

// custom something that is subject to less changes is introduced
#include <base/big_int.h>
#include <base/math.h>
#include <base/vmath.h>
#include <base/threadpool.h>
#include <base/tl/array.h>
#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <teeother/tl/nlohmann_json.h>

#endif //SERVER_STDAFX_H