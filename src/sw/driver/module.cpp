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

#include <boost/dll.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/thread/lock_types.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "module");

namespace sw
{

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

Module::Module(const Module::DynamicLibrary &dll, const String &suffix)
    : module(dll)
{
#define LOAD(f)                                                                                                                                     \
    do                                                                                                                                              \
    {                                                                                                                                               \
        f##_.name = #f + suffix;                                                                                                                    \
        f##_.m = this;                                                                                                                              \
        if (0)                                                                                                                                      \
            ;                                                                                                                                       \
        else if (!module.symbol_storage().get_function<decltype(f##_)::function_type>(f##_.name).empty())                                           \
            f##_ = module.get_function<decltype(f##_)::function_type>(f##_.name);                                                                   \
        else if (f##_.isRequired())                                                                                                                 \
            throw SW_RUNTIME_ERROR("Required function '" + f##_.name + "' is not found in module: " + normalize_path(dll.shared_lib().location())); \
    } while (0)

    // not working
    //if (module.shared_lib().has(#f))
        //f##_ = (decltype(f##_)::function_type*)module.shared_lib().get<void*>(#f);

    LOAD(build);
    LOAD(check);
    LOAD(configure);
    //LOAD(sw_get_module_abi_version);

#undef LOAD
}

path Module::getLocation() const
{
    try
    {
        // for some reason getLocation may fail on windows with
        // GetLastError() == 126 (module not found)
        // so we return this boost error instead
        return module.shared_lib().location();
    }
    catch (std::exception &e)
    {
        return e.what();
    }
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

}
