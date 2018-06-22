// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "concurrent_map.h"

#include <sw/builder/command.h>

namespace sw
{

using ConcurrentCommandStorage = ConcurrentMapSimple<size_t>;

struct CommandStorage
{
    ConcurrentCommandStorage commands;

    CommandStorage();
    CommandStorage(const CommandStorage &) = delete;
    CommandStorage &operator=(const CommandStorage &) = delete;
    ~CommandStorage();

    void load();
    void save();

    bool isOutdated(const builder::Command &c);
};

}
