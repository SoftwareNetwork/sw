#include "printer.h"

#include "cmake.h"

const std::vector<String> configuration_types = { "DEBUG", "MINSIZEREL", "RELEASE", "RELWITHDEBINFO" };
const std::vector<String> configuration_types_normal = { "Debug", "MinSizeRel", "Release", "RelWithDebInfo" };
const std::vector<String> configuration_types_no_rel = { "DEBUG", "MINSIZEREL", "RELWITHDEBINFO" };

std::unique_ptr<Printer> Printer::create(PrinterType type)
{
    switch (type)
    {
    case PrinterType::CMake:
    {
        return std::make_unique<CMakePrinter>();
    }
    default:
        throw std::runtime_error("Undefined printer");
    }
}
