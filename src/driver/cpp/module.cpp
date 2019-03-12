// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <module.h>

#include <boost/dll.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/sw/cl.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "module");

static cl::opt<bool> do_not_remove_bad_module("do-not-remove-bad-module");

namespace sw
{

ModuleStorage &getModuleStorage()
{
    static ModuleStorage modules;
    return modules;
}

const Module &ModuleStorage::get(const path &dll)
{
    if (dll.empty())
        throw SW_RUNTIME_ERROR("Empty module");

    boost::upgrade_lock lk(m);
    auto i = modules.find(dll);
    if (i != modules.end())
        return i->second;
    boost::upgrade_to_unique_lock lk2(lk);
    return modules.emplace(dll, dll).first->second;
}

Module::Module(const path &dll)
    : dll(dll)
{
    String err;
    err = "Module " + normalize_path(dll) + " is in bad shape";
    try
    {
        module = new boost::dll::shared_library(
#ifdef _WIN32
            dll.wstring()
#else
            dll.u8string()
#endif
            , boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_global
            //, ec
        );
    }
    catch (std::exception &e)
    {
        err += ": "s + e.what();
        err += " Will rebuild on the next run.";
        if (!do_not_remove_bad_module)
            fs::remove(dll);
        throw SW_RUNTIME_ERROR(err);
    }
    catch (...)
    {
        err += ". Will rebuild on the next run.";
        LOG_ERROR(logger, err);
        if (!do_not_remove_bad_module)
            fs::remove(dll);
        throw;
    }

#define LOAD_NAME(f, n)                                           \
    do                                                            \
    {                                                             \
        f##_.name = #f;                                           \
        f##_.m = this;                                            \
        if (module->has(n))                                       \
            f##_ = module->get<decltype(f##_)::function_type>(n); \
    } while (0)

#define LOAD(f) LOAD_NAME(f, f##_.name)

    LOAD(build);
    LOAD(check);
    LOAD(configure);
    LOAD(sw_get_module_abi_version);

#undef LOAD
#undef LOAD_NAME
}

Module::~Module()
{
    delete module;
}

void Module::build(Solution &s) const
{
    build_.s = &s;
    build_(s);
}

void Module::configure(Build &s) const
{
    configure_.s = &s;
    configure_(s);
}

void Module::check(Solution &s, Checker &c) const
{
    check_.s = &s;
    check_(c);
}

int Module::sw_get_module_abi_version() const
{
    return sw_get_module_abi_version_();
}

ModuleStorage &getModuleStorage(Solution &owner)
{
    static std::map<void*, ModuleStorage> s;
    return s[&owner];
}

}
