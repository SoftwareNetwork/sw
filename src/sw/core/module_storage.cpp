/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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

const ModuleStorage::DynamicLibrary &ModuleStorage::get(const path &dll, const FilesOrdered &PATH)
{
    if (dll.empty())
        throw SW_RUNTIME_ERROR("Empty module");

    boost::upgrade_lock lk(m);
    auto i = modules.find(dll);
    if (i != modules.end())
        return *i->second;
    boost::upgrade_to_unique_lock lk2(lk);

#ifdef _WIN32
    // set dll deps
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_USER_DIRS);
    std::vector<void *> cookies;
    for (const path &p : PATH)
        cookies.push_back(AddDllDirectory(p.wstring().c_str()));
    SCOPE_EXIT
    {
        // restore
        for (auto c : cookies)
            RemoveDllDirectory(c);
    };
#endif

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
