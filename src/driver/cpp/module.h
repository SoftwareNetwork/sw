// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "solution.h"

#include <boost/dll/shared_library.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace sw
{

struct SW_DRIVER_CPP_API Module
{
    template <class F, bool Required = false>
    struct LibraryCall
    {
        String name;
        Solution *s = nullptr;
        std::function<F> f;

        LibraryCall &operator=(std::function<F> f)
        {
            this->f = f;
            return *this;
        }

        template <class ... Args>
        void operator()(Args && ... args) const
        {
            if (f)
            {
                try
                {
                    f(std::forward<Args>(args)...);
                }
                catch (const std::exception &e)
                {
                    String err = "error in module: ";
                    if (s && !s->current_module.empty())
                        err += s->current_module + ": ";
                    err += e.what();
                    throw SW_RUNTIME_ERROR(err);
                }
                catch (...)
                {
                    String err = "error in module: ";
                    if (s && !s->current_module.empty())
                        err += s->current_module + ": ";
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
                if (s && !s->current_module.empty())
                    err += " " + s->current_module;
                throw SW_RUNTIME_ERROR(err);
            }
        }
    };

    boost::dll::shared_library *module = nullptr;

    Module(const path &dll);
    Module(const Module &) = delete;
    ~Module();

    // api
    void build(Solution &s) const;
    void configure(Build &s) const;
    void check(Solution &s, Checker &c) const;

    template <class F, class ... Args>
    auto call(const String &name, Args && ... args) const
    {
        if (!module)
            throw SW_RUNTIME_ERROR("empty module");
        return module->get<F>(name)(std::forward<Args>(args)...);
    }

private:
    mutable LibraryCall<void(Solution &), true> build_;
    mutable LibraryCall<void(Build &)> configure_;
    mutable LibraryCall<void(Checker &)> check_;
};

struct SW_DRIVER_CPP_API ModuleStorage
{
    std::unordered_map<path, Module> modules;
    boost::upgrade_mutex m;

    ModuleStorage() = default;
    ModuleStorage(const ModuleStorage &) = delete;

    const Module &get(const path &dll);
};

ModuleStorage &getModuleStorage(Solution &owner);

}
