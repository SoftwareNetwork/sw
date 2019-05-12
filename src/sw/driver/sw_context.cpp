// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "checks_storage.h"

namespace sw
{

SwContext::SwContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    source_dir = fs::canonical(fs::current_path());
}

SwContext::~SwContext() = default;

ChecksStorage &SwContext::getChecksStorage(const String &config) const
{
    auto i = checksStorages.find(config);
    if (i == checksStorages.end())
    {
        auto [i, _] = checksStorages.emplace(config, std::make_unique<ChecksStorage>());
        return *i->second;
    }
    return *i->second;
}

ChecksStorage &SwContext::getChecksStorage(const String &config, const path &fn) const
{
    auto i = checksStorages.find(config);
    if (i == checksStorages.end())
    {
        auto [i, _] = checksStorages.emplace(config, std::make_unique<ChecksStorage>());
        i->second->load(fn);
        return *i->second;
    }
    return *i->second;
}

}

