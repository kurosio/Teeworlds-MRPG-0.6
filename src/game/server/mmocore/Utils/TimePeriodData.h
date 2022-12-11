#ifndef GAME_SERVER_MMO_UTILS_TIME_PERIOD_DATA_H
#define GAME_SERVER_MMO_UTILS_TIME_PERIOD_DATA_H

#include <string>
#include <regex>
#include <base/math.h>

class TimePeriodData {
    int _days;
    int _hours;
    int _minutes;
    int _seconds;

    void validate()
    {
        _seconds = max(0, _seconds);
        _minutes = max(0, _minutes);
        _hours = max(0, _hours);
        _days = max(0, _days);

        _seconds = asSeconds();

        _minutes = _seconds/60;
        _seconds %= 60;

        _hours = _minutes/60;
        _minutes %= 60;

        _days = _hours/24;
        _hours %= 24;
    }

    static int numberBeforeChar(const std::string& str, char c)
    {
        std::string tmp;
        for (size_t i = str.find(c); i > 0 && i != std::string::npos; --i) {
            if (isdigit(str[int(i) - 1]))
                tmp.insert(tmp.begin(), str[int(i) - 1]);
            else {
                tmp.insert(tmp.begin(), '0');
                break;
            }
        }
        return atoi(tmp.c_str());
    }
public:
    TimePeriodData(int seconds = 0, int minutes = 0, int hours = 0, int days = 0)
    {
        _days = days;
        _hours = hours;
        _minutes = minutes;
        _seconds = seconds;
        validate();
    }
    TimePeriodData(const std::string& from)
    {
        if(!parse(from))
            _days = _hours = _minutes = _seconds = 0;
    }

    bool parse(const std::string& from)
    {
        if(!std::regex_search(from, std::regex("\\d[dhms]")))
        {
            _days = _hours = _minutes = _seconds = 0;
            return false;
        }

        _days = numberBeforeChar(from, 'd');
        _hours = numberBeforeChar(from, 'h');
        _minutes = numberBeforeChar(from, 'm');
        _seconds = numberBeforeChar(from, 's');
        validate();
        return true;
    }

    std::string asSqlInterval() const
    {
        return std::string("interval " + std::to_string(asSeconds()) + " second");
    }

    inline int asSeconds() const
    {
       return (_seconds+_minutes*60+_hours*60*60+_days*24*60*60);
    }

    inline bool isZero() const
    {
        return (asSeconds() == 0);
    }
};

#endif //GAME_SERVER_MMO_UTILS_TIME_PERIOD_DATA_H
