#include "time.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "time");

namespace sw {

bool measure_time{};
std::atomic_int nth_measurer{};

TimeMeasurer::TimeMeasurer(std::source_location loc) : TimeMeasurer{ ""s, loc } {
}

TimeMeasurer::TimeMeasurer(const std::string &name, std::source_location loc) : name{ name } {
    init(loc);
}

void TimeMeasurer::init(const std::source_location &loc) {
    std::string space(2 * nth_measurer++, ' ');
    if (name.empty()) {
        name = std::format("{}{}", space, prepare_func(loc.function_name()));
    } else {
        name = std::format("{}{} ({}):", space, name, prepare_func(loc.function_name()));
    }
    print(name);
}

std::string TimeMeasurer::prepare_func(const std::string &s) {
    auto e = s.find('(');
    auto b = s.rfind(' ', e) + 1;
    return s.substr(b, e - b);
}

TimeMeasurer::~TimeMeasurer() {
    stop();
}

void TimeMeasurer::stop() {
    if (done) {
        return;
    }
    done = true;
    --nth_measurer;
    print(std::format("{} time: {} s.", name, getTimeFloat()));
}

void TimeMeasurer::print(const std::string &s) {
    if (measure_time) {
        LOG_INFO(logger, "-> " << s);
    }
}

}

