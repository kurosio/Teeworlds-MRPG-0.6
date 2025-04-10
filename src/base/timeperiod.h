#ifndef BASE_TIMEPERIOD_H
#define BASE_TIMEPERIOD_H

#include <chrono>
#include <map>
#include <string>

class CTimePeriod final {
public:
    typedef std::map<std::string, int64_t> TokenHolder; // <literal, multiplier> (e.g. <"minutes", 60>)
    typedef std::map<int64_t, int64_t> ResultHolder; // <multiplier, value> (e.g. <60, 15>)

    class CScanner final {
        const std::string m_Source;
        TokenHolder m_Tokens;
        ResultHolder m_Result;

        size_t m_Start = 0;
        size_t m_Current = 0;

        [[nodiscard]] bool AtEnd() const {
            return m_Current >= m_Source.length();
        }

        char Advance() {
            return m_Source[m_Current++];
        }

        [[nodiscard]] char Peek() const {
            if (AtEnd()) return '\0';
            return m_Source[m_Current];
        }

        void ScanToken() {
            if (const char c = Advance(); isdigit(c)) {
                while (isdigit(Peek())) Advance();
                const std::string Value = m_Source.substr(m_Start, m_Current - m_Start);
                const auto Offset = m_Current;

                while (isalpha(Peek())) Advance();
                const std::string Literal = m_Source.substr(Offset, m_Current - Offset);

                if (Literal.empty())
                    AddValue(60, std::stoll(Value));
                else
                    AddValue(Literal, std::stoll(Value));
            }
        }

        void AddValue(const std::string_view Literal, const int64_t Value) {
            if (m_Tokens.contains(Literal.data()))
                AddValue(m_Tokens[Literal.data()], Value);
        }

        void AddValue(int64_t Multiplier, int64_t Value) {
            if (m_Result.contains(Multiplier))
                m_Result[Multiplier] += Value;
            else
                m_Result.emplace(Multiplier, Value);
        }

    public:
        explicit CScanner(const std::string_view Source, TokenHolder Multipliers) : m_Source(Source),
            m_Tokens(std::move(Multipliers)) {
        }

        [[nodiscard]] ResultHolder ScanTokens() {
            while (!AtEnd()) {
                m_Start = m_Current;
                ScanToken();
            }
            return m_Result;
        }
    };

private:
    std::chrono::seconds m_TotalDuration{0};
    int64_t m_Days{0};
    int64_t m_Hours{0};
    int64_t m_Minutes{0};
    int64_t m_Seconds{0};

    void Validate() {
        // Store the normalized values
        auto TotalSeconds = m_TotalDuration.count();

        m_Days = TotalSeconds / 86400L;
        TotalSeconds %= 86400L;

        m_Hours = TotalSeconds / 3600L;
        TotalSeconds %= 3600L;

        m_Minutes = TotalSeconds / 60L;
        m_Seconds = TotalSeconds % 60L;
    }

public:
    explicit CTimePeriod(const int64_t seconds = 0, const int64_t minutes = 0, const int64_t hours = 0,
                         const int64_t days = 0) {
        m_TotalDuration = std::chrono::seconds{seconds} +
                          std::chrono::minutes{minutes} +
                          std::chrono::hours{hours} +
                          std::chrono::days{days};
        Validate();
    }

    explicit CTimePeriod(const std::string_view from) {
        m_TotalDuration = Parse(from);
        Validate();
    }

    explicit CTimePeriod(const std::chrono::seconds duration) {
        m_TotalDuration = duration;
        Validate();
    }

    [[nodiscard]] static std::chrono::seconds Parse(const std::string_view from) {
        std::chrono::seconds TotalDuration{0};
        CScanner Scanner(from,
                         {
                             {"s", 1L}, {"seconds", 1L},
                             {"m", 60L}, {"minutes", 60L},
                             {"h", 3600L}, {"hours", 3600L},
                             {"d", 86400L}, {"days", 86400L},
                             {"mo", 2419200L}, {"months", 2419200L},
                             {"y", 31536000L}, {"years", 31536000L},
                         });

        for (auto Result = Scanner.ScanTokens(); auto [Multiplier, Value]: Result)
            TotalDuration += std::chrono::seconds(Multiplier * Value);

        return TotalDuration;
    }

    [[nodiscard]] static CTimePeriod ParseFactory(const std::string_view from) {
        return CTimePeriod(Parse(from));
    }

    [[nodiscard]] std::string asSqlInterval() const {
        return std::format("interval {} second", m_TotalDuration.count());
    }

    [[nodiscard]] constexpr std::chrono::seconds duration() const noexcept { return m_TotalDuration; }
    [[nodiscard]] constexpr int64_t days() const noexcept { return m_Days; }
    [[nodiscard]] constexpr int64_t hours() const noexcept { return m_Hours; }
    [[nodiscard]] constexpr int64_t minutes() const noexcept { return m_Minutes; }
    [[nodiscard]] constexpr int64_t seconds() const noexcept { return m_Seconds; }

    // Format as string (e.g., "2d 5h 30m 15s")
    [[nodiscard]] std::string toString() const {
        std::string result;
        if (m_Days > 0) result += std::format("{}d ", m_Days);
        if (m_Hours > 0) result += std::format("{}h ", m_Hours);
        if (m_Minutes > 0) result += std::format("{}m ", m_Minutes);
        if (m_Seconds > 0 || result.empty()) result += std::format("{}s", m_Seconds);
        return result;
    }

    [[nodiscard]] constexpr bool isZero() const noexcept {
        return m_TotalDuration.count() == 0;
    }

    auto operator<=>(const CTimePeriod &) const = default;
};

#endif // BASE_TIMEPERIOD_H
