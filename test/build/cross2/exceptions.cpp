#include "exceptions.h"

namespace sw
{

RuntimeError::RuntimeError(const std::string &msg)
    : std::runtime_error("")
{
}

} // namespace sw
