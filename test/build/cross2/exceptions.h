#pragma once

#include <stdexcept>
#include <string>

namespace sw
{

struct API RuntimeError : std::runtime_error
{
    RuntimeError(const std::string &msg);
};

}
