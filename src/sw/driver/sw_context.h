// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/builder/sw_context.h>

#include <unordered_map>

namespace sw
{

struct ChecksStorage;
struct ModuleStorage;

struct SW_DRIVER_CPP_API SwDriverContext : SwBuilderContext
{
    path source_dir;

    SwDriverContext(const path &local_storage_root_dir);
    virtual ~SwDriverContext();

    ChecksStorage &getChecksStorage(const String &config) const;
    ChecksStorage &getChecksStorage(const String &config, const path &fn) const;
    ModuleStorage &getModuleStorage() const;

private:
    mutable std::unordered_map<String, std::unique_ptr<ChecksStorage>> checksStorages;
    std::unique_ptr<ModuleStorage> module_storage;
};

} // namespace sw
