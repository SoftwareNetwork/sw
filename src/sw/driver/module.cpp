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

#include "module.h"

#include "build.h"
#define SW_PACKAGE_API
#include "sw_check_abi_version.h"

#include <boost/dll.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/thread/lock_types.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "module");

bool do_not_remove_bad_module;

namespace sw
{

template <class F>
static auto get_function(const Module::DynamicLibrary &dll, const String &fn, bool required)
{
    auto mangled_name = dll.symbol_storage().get_function<F>(fn);
    if (mangled_name.empty())
        mangled_name = fn;

    // we use shlib directly, because we already demangled name (or not)
    auto &shlib = dll.shared_lib();
    if (shlib.has(mangled_name))
        return shlib.get<F>(mangled_name);
    else if (shlib.has(fn))
        return shlib.get<F>(fn);
    else if (required)
        throw SW_RUNTIME_ERROR("Required function '" + fn + "' is not found in module: " + normalize_path(dll.shared_lib().location()));

    return (F*)nullptr;
}

Module::Module(std::unique_ptr<Module::DynamicLibrary> dll)
    : module(std::move(dll))
{
#define LOAD(f)                                                                                    \
    do                                                                                             \
    {                                                                                              \
        f##_.name = #f;                                                                            \
        f##_.m = this;                                                                             \
        f##_ = get_function<decltype(f##_)::function_type>(*module, f##_.name, f##_.isRequired()); \
    } while (0)

    LOAD(build);
    LOAD(check);
    LOAD(configure);
    LOAD(sw_get_module_abi_version);

    // regardless of config version we must check abi
    // example: new abi pushed to SW Network, but user has old client
    // this is abi mismatch or a crash without this check
    auto current_driver_abi = ::sw_get_module_abi_version();
    auto module_abi = sw_get_module_abi_version_();
    if (current_driver_abi != module_abi)
    {
        auto p = module->shared_lib().location();
        module.reset();
        String rebuild;
        if (!do_not_remove_bad_module)
        {
            fs::remove(p);
            rebuild = " Will rebuild on the next run.";
        }

        if (module_abi > current_driver_abi)
        {
            throw SW_RUNTIME_ERROR("Bad config ABI version. Module ABI (" + std::to_string(module_abi) +
                ") is greater than binary ABI (" + std::to_string(current_driver_abi) +
                "). Update your sw binary." + rebuild);
        }
        if (module_abi < current_driver_abi)
        {
            throw SW_RUNTIME_ERROR("Bad config ABI version. Module ABI (" + std::to_string(module_abi) +
                ") is less than binary ABI (" + std::to_string(current_driver_abi) +
                "). Update sw driver headers (or ask driver maintainer)." + rebuild);
        }
    }

#undef LOAD
}

path Module::getLocation() const
{
    try
    {
        // for some reason getLocation may fail on windows with
        // GetLastError() == 126 (module not found)
        // so we return this boost error instead
        return module->shared_lib().location();
    }
    catch (std::exception &e)
    {
        return e.what();
    }
}

template <class F, bool Required>
template <class ... Args>
typename Module::LibraryCall<F, Required>::std_function_type::result_type
Module::LibraryCall<F, Required>::operator()(Args && ... args) const
{
    if (f)
    {
        try
        {
            return f(std::forward<Args>(args)...);
        }
        catch (const std::exception &e)
        {
            String err = "error in module";
            if (m)
                err += " (" + normalize_path(m->getLocation()) + ")";
            err += ": ";
            err += e.what();
            throw SW_RUNTIME_ERROR(err);
        }
        catch (...)
        {
            String err = "error in module";
            if (m)
                err += " (" + normalize_path(m->getLocation()) + ")";
            err += ": ";
            err += "unknown error";
            throw SW_RUNTIME_ERROR(err);
        }
    }
    else if (Required)
    {
        String err = "Required function";
        if (!name.empty())
            err += " '" + name + "'";
        err += " is not present in the module";
        if (m)
            err += " (" + normalize_path(m->getLocation()) + ")";
        throw SW_RUNTIME_ERROR(err);
    }
    return typename std_function_type::result_type();
}

void Module::build(Build &s) const
{
    build_.s = &s;
    build_.m = this;
    build_(s);
}

void Module::configure(Build &s) const
{
    configure_.m = this;
    configure_(s);
}

void Module::check(Build &s, Checker &c) const
{
    check_.s = &s;
    check_.m = this;
    check_(c);
}

int Module::sw_get_module_abi_version() const
{
    sw_get_module_abi_version_.m = this;
    return sw_get_module_abi_version_();
}

std::unique_ptr<Module> loadSharedLibrary(const path &dll, const FilesOrdered &PATH)
{
    if (dll.empty())
        throw SW_RUNTIME_ERROR("Empty module path");

#ifdef _WIN32
    // set dll deps
    std::vector<void *> cookies;
    if (!PATH.empty())
    {
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_USER_DIRS);
        for (const path &p : PATH)
            cookies.push_back(AddDllDirectory(p.wstring().c_str()));
    }
    SCOPE_EXIT
    {
        // restore
        if (!cookies.empty())
        {
            for (auto c : cookies)
                RemoveDllDirectory(c);
            SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        }
    };
#endif

    std::unique_ptr<Module::DynamicLibrary> dl;

    String err;
    err = "Module " + normalize_path(dll) + " is in bad shape";
    try
    {
        dl = std::make_unique<Module::DynamicLibrary>(dll,
            boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_global
            //, ec
            );
    }
    catch (std::system_error &e)
    {
        err += ": "s + e.what() + " Error code = " + std::to_string(e.code().value()) + ".";
        err += " Will rebuild on the next run.";
        if (!do_not_remove_bad_module)
            fs::remove(dll);
        throw SW_RUNTIME_ERROR(err);
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

    return std::make_unique<Module>(std::move(dl));
}

}
