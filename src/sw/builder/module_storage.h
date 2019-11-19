// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/dll/smart_library.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <primitives/filesystem.h>

namespace sw
{

struct SW_BUILDER_API ModuleStorage
{
    using DynamicLibrary = boost::dll::experimental::smart_library;

    std::unordered_map<path, std::unique_ptr<DynamicLibrary>> modules;
    boost::upgrade_mutex m;

    ModuleStorage() = default;
    ModuleStorage(const ModuleStorage &) = delete;
    ~ModuleStorage();

    const DynamicLibrary &get(const path &dll);
};

}
