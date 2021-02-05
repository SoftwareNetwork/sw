// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <boost/dll/smart_library.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <primitives/filesystem.h>

namespace sw
{

struct Build;
struct Checker;

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

    Module(std::unique_ptr<Module::DynamicLibrary>, bool do_not_remove_bad_module);
    ~Module();

    // api
    void build(Build &s) const;
    void configure(Build &s) const;
    void check(Build &s, Checker &c) const;
    int sw_get_module_abi_version() const;

private:
    std::unique_ptr<Module::DynamicLibrary> module;
    bool do_not_remove_bad_module;

    mutable LibraryCall<void(Build &), true> build_;
    mutable LibraryCall<void(Build &)> configure_;
    mutable LibraryCall<void(Checker &)> check_;
    mutable LibraryCall<int(), true> sw_get_module_abi_version_;

    path getLocation() const;
};

std::unique_ptr<Module> loadSharedLibrary(const path &dll, const FilesOrdered &PATH, bool do_not_remove_bad_module);

}
