// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "checks.h"

#include <boost/dll/smart_library.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace sw
{

struct Build;

struct SW_DRIVER_CPP_API Module
{
    using DynamicLibrary = boost::dll::experimental::smart_library;

    template <class F, bool Required = false>
    struct LibraryCall
    {
        using function_type = F;
        using std_function_type = std::function<F>;

        String name;
        const Build *s = nullptr;
        const Module *m = nullptr;
        std_function_type f;

        LibraryCall &operator=(std::function<F> f)
        {
            this->f = f;
            return *this;
        }

        template <class ... Args>
        typename std_function_type::result_type operator()(Args &&... args) const;

        bool isRequired() const { return Required; }
    };

    Module(const Module::DynamicLibrary &, const String &suffix = {});

    // api
    void build(Build &s) const;
    void configure(Build &s) const;
    void check(Build &s, Checker &c) const;
    int sw_get_module_abi_version() const;

    // needed in scripts
    template <class F, class ... Args>
    auto call(const String &name, Args && ... args) const
    {
        if (!module)
            throw SW_RUNTIME_ERROR("empty module");
        return module.get_function<F>(name)(std::forward<Args>(args)...);
    }

private:
    const DynamicLibrary &module;

    mutable LibraryCall<void(Build &), true> build_;
    mutable LibraryCall<void(Build &)> configure_;
    mutable LibraryCall<void(Checker &)> check_;
    mutable LibraryCall<int(), true> sw_get_module_abi_version_;

    path getLocation() const;
};

}
