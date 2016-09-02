#pragma once

#include "context.h"
#include "project.h"

#define CPP_HEADER_FILENAME "cppan.h"
#define CPPAN_EXPORT "CPPAN_EXPORT"
#define CPPAN_EXPORT_PREFIX "CPPAN_API_"
#define CPPAN_LOCAL_BUILD_PREFIX "cppan-build-"

#define INCLUDE_GUARD_PREFIX "CPPAN_INCLUDE_GUARD_"

extern const std::vector<String> configuration_types;
extern const std::vector<String> configuration_types_normal;
extern const std::vector<String> configuration_types_no_rel;

enum class PrinterType
{
    CMake,
    //Ninja,
    // add more here
};

struct Config;

struct Printer
{
    Package d;
    class AccessTable *access_table = nullptr;
    Config *cc = nullptr; // current
    Config *pc = nullptr; // parent
    Config *rc = nullptr; // root
    std::set<String> include_guards;

    virtual void prepare_build(const path &fn, const String &cppan) = 0;
    virtual int generate() const = 0;
    virtual int build() const = 0;

    virtual void print() = 0;
    virtual void print_meta() = 0;

    virtual void clear_cache(path p) const = 0;
    virtual void clear_exports(path p) const = 0;

    static std::unique_ptr<Printer> create(PrinterType type);
};
