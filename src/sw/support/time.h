#pragma once

#include <primitives/date_time.h>
#include <primitives/templates.h>

#define TIME_MEASURER(...) ::sw::TimeMeasurer ANONYMOUS_VARIABLE_LINE(t){__VA_ARGS__}

namespace sw {

extern bool measure_time;

struct SW_SUPPORT_API TimeMeasurer : ScopedTime {
    std::string name;
    bool done{};

    TimeMeasurer(std::source_location loc = std::source_location::current());
    TimeMeasurer(const std::string &name, std::source_location loc = std::source_location::current());
    void stop();
    ~TimeMeasurer();
private:
    void init(const std::source_location &loc);
    std::string prepare_func(const std::string &s);
    void print(const std::string &);
};

}
