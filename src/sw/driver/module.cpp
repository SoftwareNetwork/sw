// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
        if (!module.symbol_storage().get_function<decltype(f##_)::function_type>(f##_.name).empty())                                                \
            f##_ = module.get_function<decltype(f##_)::function_type>(f##_.name);                                                                   \
        else if (f##_.isRequired())                                                                                                                 \
            throw SW_RUNTIME_ERROR("Required function '" + f##_.name + "' is not found in module: " + normalize_path(dll.shared_lib().location())); \
    } while (0)

    LOAD(build);
    LOAD(check);
    LOAD(configure);
    LOAD(sw_get_module_abi_version);

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
