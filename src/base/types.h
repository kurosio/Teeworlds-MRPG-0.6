#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#include <any>

enum class TRISTATE
{
	NONE,
	SOME,
	ALL,
};

template<typename... Args>
std::vector<std::any> MakeAnyList(Args&&... args) {
    std::vector<std::any> result;
    result.reserve(sizeof...(args));
    (result.emplace_back(std::forward<Args>(args)), ...);
    return result;
}

#endif // BASE_TYPES_H
