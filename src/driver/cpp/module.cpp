// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <module.h>

#include <boost/dll.hpp>
#include <boost/thread/lock_types.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "module");

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
        fs::remove(dll);
        throw SW_RUNTIME_ERROR(err);
    }
    catch (...)
    {
        err += ". Will rebuild on the next run.";
        LOG_ERROR(logger, err);
        fs::remove(dll);
        throw;
    }

    build_.name = "build";
    build_.m = this;
    if (module->has("build"))
        build_ = module->get<void(Solution&)>("build");

    check_.name = "check";
    check_.m = this;
    if (module->has("check"))
        check_ = module->get<void(Checker&)>("check");

    configure_.name = "configure";
    configure_.m = this;
    if (module->has("configure"))
        configure_ = module->get<void(Build&)>("configure");
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

ModuleStorage &getModuleStorage(Solution &owner)
{
    static std::map<void*, ModuleStorage> s;
    return s[&owner];
}

}
