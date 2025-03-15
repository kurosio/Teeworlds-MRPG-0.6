#ifndef GAME_SERVER_CORE_TOOLS_OPTIONS_PACKER_H
#define GAME_SERVER_CORE_TOOLS_OPTIONS_PACKER_H

class BaseOptionPacker
{
public:
    virtual ~BaseOptionPacker() = default;
};

template<typename... Args>
class TupleOptionPacker : public BaseOptionPacker
{
public:
    std::tuple<Args...> m_TupleData;

    explicit TupleOptionPacker(Args... values)
        : m_TupleData(values...) {
    }
};

class OptionPacker
{
    std::shared_ptr<BaseOptionPacker> m_Options {};

public:
    template<typename... Args>
    void Pack(Args... values)
    {
        m_Options = std::make_shared<TupleOptionPacker<std::decay_t<Args>...>>(values...);
    }

    template<typename... Args>
    auto Unpack() -> std::tuple<std::decay_t<Args>...>
    {
        using CleanOption = TupleOptionPacker<std::decay_t<Args>...>;
        auto options = dynamic_cast<CleanOption*>(m_Options.get());

        // create options by default values
        if(!options)
        {
            return std::tuple<std::decay_t<Args>...>(std::decay_t<Args>{}...);
        }

        // unpacking options
        return unpackHelper(options->m_TupleData, std::index_sequence_for<Args...> {});
    }

protected:
    template<typename Tuple, std::size_t... Is>
    auto unpackHelper(Tuple& tuple, std::index_sequence<Is...>) -> std::tuple<typename std::tuple_element<Is, Tuple>::type...>
    {
        return std::tuple<typename std::tuple_element<Is, Tuple>::type...>(std::get<Is>(tuple)...);
    }
};

#endif // GAME_SERVER_CORE_TOOLS_OPTIONS_PACKER_H