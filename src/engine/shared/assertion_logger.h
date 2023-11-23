#ifndef ENGINE_SHARED_ASSERTION_LOGGER_H
#define ENGINE_SHARED_ASSERTION_LOGGER_H

#include <memory>

class ILogger;
class IStorageEngine;

std::unique_ptr<ILogger> CreateAssertionLogger(IStorageEngine *pStorage, const char *pGameName);

#endif
