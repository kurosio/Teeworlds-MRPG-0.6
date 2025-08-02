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

template<typename T>
requires ((!std::is_same_v<T, std::nullopt_t>) && std::is_copy_constructible_v<T>)
constexpr T GetIfExists(const std::vector<std::any> &List, size_t Index, T Fallback)
{
    if(Index < List.size())
    {
        try { // uhm, we have exceptions disabled, how will this work?
            return std::any_cast<T>(List.at(Index));
        } catch (const std::bad_any_cast&) {
            return Fallback;
        }
    }
    return Fallback;
}
template<typename T>
constexpr std::optional<T> GetIfExists(const std::vector<std::any> &List, size_t Index)
{
    if(Index < List.size())
    {
        try {
            return std::optional<T>(std::any_cast<T>(List.at(Index)));
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

#endif // BASE_TYPES_H
