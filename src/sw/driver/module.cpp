// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "module.h"

#include <boost/dll.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/sw/cl.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "module");

static cl::opt<bool> do_not_remove_bad_module("do-not-remove-bad-module");

namespace sw
{

const Module::DynamicLibrary &ModuleStorage::get(const path &dll)
{
    if (dll.empty())
        throw SW_RUNTIME_ERROR("Empty module");

    boost::upgrade_lock lk(m);
    auto i = modules.find(dll);
    if (i != modules.end())
        return *i->second;
    boost::upgrade_to_unique_lock lk2(lk);

    String err;
    err = "Module " + normalize_path(dll) + " is in bad shape";
    try
    {
        auto module = std::make_unique<Module::DynamicLibrary>(dll,
            boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_global
            //, ec
        );

        return *modules.emplace(dll, std::move(module)).first->second;
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
}

Module::Module(const Module::DynamicLibrary &dll, const String &suffix)
    : module(dll)
{
#define LOAD_NAME(f, n)                                                                               \
    do                                                                                                \
    {                                                                                                 \
        f##_.name = #f;                                                                               \
        f##_.m = this;                                                                                \
        if (!module.symbol_storage().get_function<decltype(f##_)::function_type>(n + suffix).empty()) \
            f##_ = module.get_function<decltype(f##_)::function_type>(n + suffix);                    \
    } while (0)

#define LOAD(f) LOAD_NAME(f, f##_.name)

    LOAD(build);
    LOAD(check);
    LOAD(configure);
    LOAD(sw_get_module_abi_version);

#undef LOAD
#undef LOAD_NAME
}

path Module::getLocation() const
{
    return module.shared_lib().location();
}

void Module::build(Build &s) const
{
    build_.s = &s;
    build_(s);
}

void Module::configure(Build &s) const
{
    configure_(s);
}

void Module::check(Build &s, Checker &c) const
{
    check_.s = &s;
    check_(c);
}

int Module::sw_get_module_abi_version() const
{
    return sw_get_module_abi_version_();
}

}
