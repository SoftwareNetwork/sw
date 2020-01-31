// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "module_storage.h"

#include <boost/dll.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/exceptions.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "module_storage");

bool do_not_remove_bad_module;

namespace sw
{

const ModuleStorage::DynamicLibrary &ModuleStorage::get(const path &dll)
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
        auto module = std::make_unique<DynamicLibrary>(dll,
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

ModuleStorage::~ModuleStorage()
{
    if (std::uncaught_exceptions())
    {
        LOG_DEBUG(logger, "Exception might be thrown from one of the modules, so not unloading them");

        // unknown exception, cannot copy, so do not unload
        for (auto &[k, v] : modules)
            v.release();
    }
}

}
