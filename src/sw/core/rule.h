// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/command.h>

namespace sw
{

struct SW_CORE_API IRule : ICastable
{
    virtual ~IRule() = 0;

    // get commands for ... (building?)
    ///
    virtual Commands getCommands() const = 0;
};

} // namespace sw
