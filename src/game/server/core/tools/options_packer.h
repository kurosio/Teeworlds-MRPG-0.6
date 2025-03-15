#ifndef GAME_SERVER_CORE_TOOLS_OPTIONS_PACKER_H
#define GAME_SERVER_CORE_TOOLS_OPTIONS_PACKER_H

// concepts
template<typename T>
concept NotConstPointer = !std::is_pointer_v<T> || !std::is_const_v<std::remove_pointer_t<T>>;

class BaseOptionPacker
{
public:
    virtual ~BaseOptionPacker() = default;
};

template<typename... Args>
class OptionTuplePacker : public BaseOptionPacker
{
public:
    std::tuple<Args...> m_OptionData;

    // here perfect capture is replaced by copying to avoid problems
    explicit OptionTuplePacker(Args... values)
        : m_OptionData(values...) {
    }
};

class OptionPacker
{
    std::shared_ptr<BaseOptionPacker> m_OptionContainer {};

public:
    template<typename... Args>
    void Pack(Args&&... values) requires (NotConstPointer<Args> && ...)
    {
        // compile assert for empty unpack options
        static_assert(sizeof...(Args) > 0, "Pack must be called with at least one argument");

        m_OptionContainer = std::make_shared<OptionTuplePacker<std::decay_t<Args>...>>(std::forward<Args>(values)...);
    }

    template<typename... Args>
    auto Unpack() -> std::tuple<std::decay_t<Args>...>
    {
        // compile assert for empty unpack options
        static_assert(sizeof...(Args) > 0, "Unpack must be called with at least one argument");

        using CleanOption = OptionTuplePacker<std::decay_t<Args>...>;
        auto optionPacker = dynamic_cast<CleanOption*>(m_OptionContainer.get());

        // create options by default values
        if(!optionPacker)
        {
            return std::tuple<std::decay_t<Args>...>(std::decay_t<Args>{}...);
        }

        // unpacking options
        return unpackHelper(optionPacker->m_OptionData, std::index_sequence_for<Args...> {});
    }

protected:
    template<typename Tuple, std::size_t... Is>
    auto unpackHelper(Tuple& tuple, std::index_sequence<Is...>) -> std::tuple<typename std::tuple_element<Is, Tuple>::type...>
    {
        return std::tuple<typename std::tuple_element<Is, Tuple>::type...>(std::get<Is>(tuple)...);
    }
};

#endif // GAME_SERVER_CORE_TOOLS_OPTIONS_PACKER_H