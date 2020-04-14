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

    Module(std::unique_ptr<Module::DynamicLibrary>);

    // api
    void build(Build &s) const;
    void configure(Build &s) const;
    void check(Build &s, Checker &c) const;

private:
    std::unique_ptr<Module::DynamicLibrary> module;

    mutable LibraryCall<void(Build &), true> build_;
    mutable LibraryCall<void(Build &)> configure_;
    mutable LibraryCall<void(Checker &)> check_;

    path getLocation() const;
};

std::unique_ptr<Module> loadSharedLibrary(const path &dll, const FilesOrdered &PATH = {});

}
